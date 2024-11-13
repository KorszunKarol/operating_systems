// Name: Karol
// Last Name: Korszun

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <signal.h>

#define BUFFER_SIZE 1024
#define printF(x) write(1, x, strlen(x))

typedef struct {
    int socket_fd;
} Client;

void handle_sigint(int sig) {
    (void)sig;
}

void init_client(Client *client, char *ip, int port) {
    struct sockaddr_in server_addr;

    client->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client->socket_fd < 0) {
        printF("Error creating socket\n");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip);

    if (connect(client->socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printF("Connection failed\n");
        exit(1);
    }
}

void send_message(Client *client, char *message) {
    write(client->socket_fd, message, strlen(message));
}

void receive_message(Client *client) {
    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);
    read(client->socket_fd, buffer, BUFFER_SIZE - 1);
    printF(buffer);
}

void get_user_input(char *buffer, int size) {
    memset(buffer, 0, size);
    int bytes_read = read(0, buffer, size - 2);
    if (bytes_read > 0) {
        if (buffer[bytes_read - 1] == '\n') {
            bytes_read--;
        }
        buffer[bytes_read] = '\n';
        buffer[bytes_read + 1] = '\0';
    }
}

void show_menu() {
    printF("\n===================\n");
    printF("|| Navigation Menu ||\n");
    printF("===================\n");
    printF("1. Receive Current Challenge\n");
    printF("2. Send Response to Challenge\n");
    printF("3. Request Hint\n");
    printF("4. View Current Mission Status\n");
    printF("5. Terminate Connection and Exit\n");
    printF("Select an option: ");
}

void handle_menu_option(Client *client, char option) {
    char response[BUFFER_SIZE];
    char menu_option[3];

    menu_option[0] = option;
    menu_option[1] = '\n';
    menu_option[2] = '\0';

    switch(option) {
        case '1':
            send_message(client, menu_option);
            receive_message(client);
            break;

        case '2':
            send_message(client, menu_option);
            printF("Enter your response: ");
            get_user_input(response, BUFFER_SIZE);
            send_message(client, response);
            receive_message(client);
            break;

        case '3':
            send_message(client, menu_option);
            receive_message(client);
            break;

        case '4':
            send_message(client, menu_option);
            receive_message(client);
            break;

        case '5':
            send_message(client, menu_option);
            break;
    }
}

int main(int argc, char *argv[]) {
    Client client;
    char input[BUFFER_SIZE];
    char option;

    if (argc != 3) {
        printF("Usage: ./S4 <IP> <port>\n");
        return 1;
    }

    signal(SIGINT, handle_sigint);

    init_client(&client, argv[1], atoi(argv[2]));

    printF("Welcome to RiddleQuest. Prepare to unlock the secrets and discover the treasure!\n");
    printF("Enter your name: ");
    get_user_input(input, BUFFER_SIZE);
    send_message(&client, input);

    while (1) {
        show_menu();
        get_user_input(input, BUFFER_SIZE);
        option = input[0];

        if (option < '1' || option > '5') {
            printF("Invalid option. Please try again.\n");
            continue;
        }

        handle_menu_option(&client, option);

        if (option == '5') {
            break;
        }
    }

    close(client.socket_fd);
    return 0;
}
