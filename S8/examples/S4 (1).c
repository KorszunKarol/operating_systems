/*
* Solution S2 Operating Systems - Sockets I
* Curs 2024-25
*
* @author: Cristina Martí
*
*/


#define _GNU_SOURCE

#include <stdio.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>

// ANSI Color Codes for styling
#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define YEL   "\x1B[33m"
#define BLU   "\x1B[34m"
#define MAG   "\x1B[35m"
#define CYN   "\x1B[36m"
#define WHT   "\x1B[37m"
#define RESET "\x1B[0m"

#define ERROR_MESSAGE "Usage: ./client <server_ip> <port>\n"


char *readUntil(int fd, char cEnd) {
    int i = 0;
    ssize_t chars_read;
    char c = 0;
    char *buffer = NULL;

    while (1) {
        chars_read = read(fd, &c, sizeof(char));  
        if (chars_read == 0) {         
            if (i == 0) {              
                return NULL;
            }
            break;                     
        } else if (chars_read < 0) {   
            free(buffer);
            return NULL;
        }

        if (c == cEnd) {              
            break;
        }
        buffer = (char *)realloc(buffer, i + 2);
        buffer[i++] = c;                
    }

    buffer[i] = '\0';  // Null-terminate the string
    return buffer;
}


void displayClientBanner() {
    const char *banner =
        "\033[1;34m" // Blue color start
        "\n"
        "⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⣀⣀⣀⣀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n"
        "⠀⠀⠀⠀⠲⣶⣾⣿⣿⣷⣄⠀⢸⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣦⡀⠀⠀⠀⠀\n"
        "⠀⠀⠀⢻⣿⣿⣿⣿⣿⣿⣿⣷⣼⣿⣿⣿⡿⠁⢻⣿⣿⣿⣿⣿⣿⣿⠀⠀⠀⠀\n"
        "⠀⠀⠀⠘⣿⣿⠛⣿⣿⣿⣿⣿⣿⡿⠁⠛⢀⣶⡈⡿⠋⢻⣿⣿⡿⠋⠀⠀⠀⠀\n"
        "⠀⠀⠀⠀⣿⣿⣶⣿⣅⣽⣿⣿⣿⠃⣰⡀⢺⣿⡇⢀⣴⡀⠻⣿⣶⣶⡆⠀⠀⠀\n"
        "⠀⠀⠀⠀⣿⣿⣿⣿⣿⣦⣼⣿⣇⣰⣿⣷⣾⣏⣠⣾⣿⣿⣄⣽⣿⣿⣷⠀⠀⠀\n"
        "⠀⠀⠀⠀⣿⣿⣿⣿⣧⣬⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⡿⠋⠀⠀⠀\n"
        "⠀⠀⠀⢠⣿⣿⣿⣯⣤⣿⣿⣿⣿⣿⣿⣿⣿⣿⠛⣿⠛⣿⣿⣿⣿⣷⣤⠀⠀⠀\n"
        "⠀⠀⠀⢸⣿⣿⣿⣿⣁⣿⠿⣿⠿⣿⠛⣿⣁⣽⣿⣿⣿⡏⣹⣿⣿⣿⣿⠀⠀⠀\n"
        "⠀⠀⠀⣼⣿⣿⣿⣿⣿⣿⣶⣷⣴⣿⣾⣿⣿⣿⣿⣿⣿⣿⣋⣿⣿⣿⣿⠀⠀⠀\n"
        "⠀⠀⠀⢿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⠉⠙⠻⢿⡿⠋⠛⣿⡀⠀⠀\n"
        "⠀⠀⠀⠈⠻⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣿⣷⣶⣤⠀⠀⠀⣼⣿⡇⠀⠀\n"
        "⠀⠀⠀⠀⠀⠈⠻⣿⣿⣿⣿⠛⣿⡿⠟⣿⣿⣿⣿⣿⣋⣡⣴⣾⣶⣌⣿⣿⠀⠀\n"
        "⠀⠀⠀⠀⠀⠀⠀⠈⠻⠟⠋⠀⠀⠀⠀⠛⠛⠛⠛⠛⠛⠛⠛⠿⠿⠿⠿⠿⠇⠀\n"
        "⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀⠀\n"
        "\033[0m" // Reset to default color
        "Welcome to RiddleQuest. Prepare to unlock the secrets and discover the treasure!\n";

    write(STDOUT_FILENO, banner, strlen(banner));
}


void sendToServer(int sockfd, char *message) {

    char* buffer;
    int len = asprintf(&buffer, "%s\n", message);

    if (write(sockfd, buffer, len) < 0) {
        write(STDOUT_FILENO, RED"Error writing to server"RESET, strlen(RED"Error writing to server"RESET));
        exit(EXIT_FAILURE);
    }

    free(buffer);
}


int connectToServer(const char *server_ip, int port) {
    int sockfd;
    struct sockaddr_in serv_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror(RED"Cannot open socket"RESET);
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = inet_addr(server_ip);

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror(RED"Cannot connect to the server"RESET);
        return -1;
    }

    return sockfd;
}

void interactWithServer(int sockfd) {
    char* buffer;
    char* userInput;
    int exit = 0;

    write(STDOUT_FILENO, MAG"Enter your name: "RESET, strlen(MAG"Enter name name: "RESET));
    buffer = readUntil(0, '\n');
    sendToServer(sockfd, buffer);
    free(buffer);
    while (!exit) {
        char *menu = "\n====================================\n"
                     "||      Navigation Menu      ||\n"
                     "====================================\n"
                     "1. Receive Current Challenge\n"
                     "2. Send Response to Challenge\n"
                     "3. Request Hint\n"
                     "4. View Current Mission Status\n"
                     "5. Terminate Connection and Exit\n"
                     "Select an option: " ;
        write(STDOUT_FILENO, menu, strlen(menu));
        userInput = readUntil(0, '\n');

        int option = atoi(userInput);
        switch (option) {
            case 1:
                sendToServer(sockfd, "1");
                buffer = readUntil(sockfd, '\n');
                write(STDOUT_FILENO, MAG"Challenge: "RESET, strlen(MAG"Challenge: "RESET));
                write(STDOUT_FILENO, buffer, strlen(buffer));
                write(STDOUT_FILENO, "\n", 1);
                free(buffer);
                break;
            case 2:
                sendToServer(sockfd, "2");
                write(STDOUT_FILENO, MAG"Enter your response to the challenge: "RESET, strlen(MAG"Enter your response to the challenge: "RESET));
                buffer = readUntil(0, '\n');
                sendToServer(sockfd, buffer);
                free(buffer);
                buffer = NULL;
                buffer = readUntil(sockfd, '\n');
                write(STDOUT_FILENO, buffer, strlen(buffer));
                write(STDOUT_FILENO, "\n", 1);
                free(buffer);
                break;
            case 3:
                sendToServer(sockfd, "3");
                write(STDOUT_FILENO, MAG"Your hint is comming...\n"RESET, strlen(MAG"Your hint is comming...\n"RESET));
                buffer = readUntil(sockfd, '\n');
                write(STDOUT_FILENO, buffer, strlen(buffer));
                write(STDOUT_FILENO, "\n", 1);
                free(buffer);
                break;
            case 4:
                sendToServer(sockfd, "4");
                write(STDOUT_FILENO, MAG"Current mission status: \n"RESET, strlen(MAG"Your hint is comming...\n"RESET));
                buffer = readUntil(sockfd, '\n');
                write(STDOUT_FILENO, buffer, strlen(buffer));
                write(STDOUT_FILENO, "\n", 1);
                if (strcmp(buffer,GRN "Congratulations! You've found the treasure at coordinates: X:100, Y:200. Disconnecting."RESET) == 0){
                    sendToServer(sockfd, "5");
                    exit = 1;
                }
                free(buffer);
                break;
            case 5:
                sendToServer(sockfd, "5");
                exit = 1;
                write(STDOUT_FILENO, YEL"Terminating connection...\n"RESET, strlen(YEL"Terminating connection...\n"RESET));
                break;
            default:
                write(STDOUT_FILENO, RED"Opción no válida. Por favor intente de nuevo.\n"RESET, strlen(RED"Opción no válida. Por favor intente de nuevo.\n"RESET));
        }

        free(userInput);
        userInput = NULL;
    }
}

void nothing(int sig) {
    signal(sig, nothing);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        write(STDOUT_FILENO, ERROR_MESSAGE, strlen(ERROR_MESSAGE));
        return EXIT_FAILURE;
    }

    int sockfd = connectToServer(argv[1], atoi(argv[2]));
    if (sockfd < 0) {
        char *error_message = "Failed to connect to the server.\n";
        write(STDOUT_FILENO, error_message, strlen(error_message));
    }else{
        signal(SIGINT, nothing);

        displayClientBanner();
        interactWithServer(sockfd);
        close(sockfd);
    }

    return 0;
}