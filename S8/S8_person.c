// name: Karol
// surname: Korszun

#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>

#define printF(x) write(1, x, strlen(x))
#define ERROR_MSG_QUEUE "Error connecting to queue\n"
#define ERROR_MSG_KEY "Error getting key\n"
#define REQUEST_TIMES 1
#define TIMES_RESPONSE 2
#define RESERVE 3
#define RESERVE_RESPONSE 4

static const char DNI_LETTERS[] = "TRWAGMYFPDXBNJZSQVHLCKE";

typedef struct {
    long id_msg;
    char dni[9];
    int hour;
} RequestMessage;

typedef struct {
    long id_msg;
    int response;
    int available_hours;
} ResponseMessage;

char *readUntil(int fd, char end) {
    char *string = NULL;
    char c = '\0';
    int i = 0, size;

    while (1) {
        size = read(fd, &c, sizeof(char));
        if (string == NULL) {
            string = (char *)malloc(sizeof(char));
        }
        if (c != end && size > 0) {
            string = (char *)realloc(string, sizeof(char) * (i + 2));
            string[i++] = c;
        } else {
            break;
        }
    }
    string[i] = '\0';
    return string;
}

int validate_dni(const char *dni) {
    if (strlen(dni) != 9) return 0;

    for (int i = 0; i < 8; i++) {
        if (dni[i] < '0' || dni[i] > '9') return 0;
    }

    int number = atoi(dni);
    int index = number % 23;
    return (dni[8] == DNI_LETTERS[index]);
}

int main() {
    key_t key;
    int id_queue;
    RequestMessage req_msg = {0};
    ResponseMessage resp_msg = {0};
    char *buffer;

    key = ftok(".", 'S');
    if (key == (key_t)-1) {
        printF(ERROR_MSG_KEY);
        exit(-1);
    }

    id_queue = msgget(key, 0600);
    if (id_queue == -1) {
        printF(ERROR_MSG_QUEUE);
        exit(-1);
    }

    int valid = 0;
    do {
        printF("Please enter your DNI: ");
        buffer = readUntil(0, '\n');
        if (buffer == NULL) continue;

        valid = validate_dni(buffer);
        if (valid) {
            strcpy(req_msg.dni, buffer);
            printF("DNI is valid.\n");
        } else {
            printF("Invalid DNI. Please try again.\n");
        }
        free(buffer);
    } while (!valid);

    while (1) {
        printF("Requesting available times...\n");
        req_msg.id_msg = REQUEST_TIMES;  
        if (msgsnd(id_queue, &req_msg, sizeof(RequestMessage) - sizeof(long), 0) == -1) {
            printF("Error sending request\n");
            exit(-1);
        }

        if (msgrcv(id_queue, &resp_msg, sizeof(ResponseMessage) - sizeof(long), TIMES_RESPONSE, 0) == -1) {
            if (errno == EINTR) {
                printF("Request interrupted. Please try again.\n");
                continue;
            }
            printF("Error receiving response\n");
            exit(-1);
        }

        printF("Available times:\n");
        for (int hour = 9; hour <= 16; hour++) {
            char hour_str[20];
            snprintf(hour_str, sizeof(hour_str), "%d) %02d:00\n", hour-8, hour);
            printF(hour_str);
        }

        int valid_slot = 0;
        while (!valid_slot) {
            printF("Select a time slot: ");
            buffer = readUntil(0, '\n');
            if (buffer != NULL) {
                int slot = atoi(buffer);
                if (slot >= 1 && slot <= 8) {
                    valid_slot = 1;
                    req_msg.hour = slot + 8;
                    free(buffer);
                    buffer = NULL;

                    char msg[50];
                    snprintf(msg, sizeof(msg), "Reserving appointment at %02d:00\n", req_msg.hour);
                    printF(msg);

                    req_msg.id_msg = RESERVE;
                    msgsnd(id_queue, &req_msg, sizeof(RequestMessage) - sizeof(long), 0);

                    msgrcv(id_queue, &resp_msg, sizeof(ResponseMessage) - sizeof(long), RESERVE_RESPONSE, 0);

                    if (resp_msg.response) {
                        printF("Appointment successfully reserved.\n");
                        printF("Thank you!\n");
                        return 0;
                    } else {
                        printF("Could not reserve the appointment. Please try again.\n");
                        break;
                    }
                } else {
                    printF("Invalid time slot. Please select a number between 1 and 8.\n");
                    free(buffer);
                    buffer = NULL;
                }
            }
        }
    }

    return 0;
}