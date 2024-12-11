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
#include <signal.h>
#include <errno.h>
#include <stdio.h>

#define printF(x) write(1, x, strlen(x))
#define ERROR_MSG_QUEUE "Error creating queue\n"
#define ERROR_MSG_KEY "Error creating key\n"
#define REQUEST_TIMES 1
#define TIMES_RESPONSE 2
#define RESERVE 3
#define RESERVE_RESPONSE 4
#define CONFIRMED "CONFIRMED\n"
#define NOT_AVAILABLE "NOT_AVAILABLE\n"

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

typedef struct {
    int hour;
    char dni[9];
} Appointment;

int running = 1;

void save_appointments(Appointment *appointments, int count) {
    int fd = open("appointments.dat", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd != -1) {
        size_t size = sizeof(Appointment) * count;
        ssize_t written = write(fd, appointments, size);
        if (written < 0 || (size_t)written != size) {
            printF("Error writing appointments\n");
        }
        close(fd);
    }
}

int load_appointments(Appointment *appointments) {
    int count = 0;
    int fd = open("appointments.dat", O_RDONLY);
    if (fd != -1) {
        count = read(fd, appointments, sizeof(Appointment) * 16) / sizeof(Appointment);
        close(fd);
    }
    return count;
}

void cleanup(int id_queue) {
    msgctl(id_queue, IPC_RMID, NULL);
}

void nothing() {
    running = 0;
    signal(SIGINT, nothing);
}

int check_bookings(Appointment *appointments, int count, int hour) {
    int bookings = 0;
    for (int i = 0; i < count; i++) {
        if (appointments[i].hour == hour) bookings++;
    }
    return bookings;
}

int init_queue() {
    key_t key = ftok(".", 'S');
    if (key != (key_t)-1) {
        int old_queue = msgget(key, 0600);
        if (old_queue != -1) {
            msgctl(old_queue, IPC_RMID, NULL);
        }
    }

    key = ftok(".", 'S');
    if (key == (key_t)-1) {
        printF(ERROR_MSG_KEY);
        exit(-1);
    }

    int id_queue = msgget(key, 0600 | IPC_CREAT);
    if (id_queue == -1) {
        printF(ERROR_MSG_QUEUE);
        exit(-1);
    }
    return id_queue;
}

void handle_reservation(Appointment *appointments, int *count, RequestMessage *req_msg, ResponseMessage *resp_msg) {
    int bookings = check_bookings(appointments, *count, req_msg->hour);
    resp_msg->id_msg = RESERVE_RESPONSE;

    if (bookings < 2) {
        printF("Time reserved.\n");
        appointments[*count].hour = req_msg->hour;
        strcpy(appointments[*count].dni, req_msg->dni);
        (*count)++;
        resp_msg->response = 1;
        save_appointments(appointments, *count);
    } else {
        printF("Time not available.\n");
        resp_msg->response = 0;
    }
}

int main() {
    int id_queue;
    RequestMessage req_msg = {0};
    ResponseMessage resp_msg = {0};
    Appointment appointments[16] = {0};
    int appointment_count;

    appointment_count = load_appointments(appointments);
    id_queue = init_queue();
    signal(SIGINT, nothing);
    printF("Administration process started.\n");

    while (running) {
        if (msgrcv(id_queue, &req_msg, sizeof(RequestMessage) - sizeof(long), REQUEST_TIMES, 0) == -1) {
            if (errno == EINTR) continue;
            break;
        }

        resp_msg.id_msg = TIMES_RESPONSE;
        resp_msg.available_hours = 0;

        for (int hour = 9; hour <= 16; hour++) {
            int bookings = check_bookings(appointments, appointment_count, hour);
            if (bookings < 2) {
                resp_msg.available_hours |= (1 << (hour - 9));
            }
        }

        if (msgsnd(id_queue, &resp_msg, sizeof(ResponseMessage) - sizeof(long), 0) == -1) {
            printF("Error sending response\n");
            continue;
        }

        if (msgrcv(id_queue, &req_msg, sizeof(RequestMessage) - sizeof(long), RESERVE, 0) == -1) {
            if (errno == EINTR) continue;
            break;
        }

        printF("Reservation request received.\n");
        char msg[100];
        snprintf(msg, sizeof(msg), "Person %s requested appointment at %02d:00.\n",
                req_msg.dni, req_msg.hour);
        printF(msg);

        handle_reservation(appointments, &appointment_count, &req_msg, &resp_msg);
        msgsnd(id_queue, &resp_msg, sizeof(ResponseMessage) - sizeof(long), 0);
    }

    cleanup(id_queue);
    return 0;
}