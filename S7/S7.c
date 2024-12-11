/*
name: Karol
lastname: Korszun
username: korszun.karol
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>

#define MAX_CLIENTS 10

#define RED     "\x1B[31m"
#define GRN     "\x1B[32m"
#define RESET   "\x1B[0m"

#define printF(x) write(1, x, strlen(x))

typedef struct {
    char* word;
    char* definition;
} DictionaryEntry;

typedef struct {
    DictionaryEntry* entries;
    int numWords;
    char* filename;
} Dictionary;

typedef struct {
    int socket;
    char* name;
} Client;

Dictionary dict;
int server_fd;
Client clients[MAX_CLIENTS] = {0};

pthread_mutex_t dict_mutex = PTHREAD_MUTEX_INITIALIZER;

void loadDictionary(char* filename);
void handleClientMessage(int client_fd, char* message);
void handleSearch(int client_fd, char* word);
void handleAdd(int client_fd, char* word, char* definition);
void handleList(int client_fd);
void freeResources();
void handleExit();
void startServer(char* ip, int port);
void updateDictionaryFile(char* filename);
char* readUntil(int fd, char end);

char* readUntil(int fd, char end) {
    if (fd < 0) return NULL;

    char* buffer = NULL;
    int size = 0;
    char c;

    while (1) {
        ssize_t bytes_read = read(fd, &c, 1);
        if (bytes_read <= 0) {
            if (size == 0) {
                free(buffer);
                return NULL;
            }
            break;
        }

        if (c == end) break;

        char* temp = realloc(buffer, size + 1);
        if (!temp) {
            free(buffer);
            return NULL;
        }
        buffer = temp;
        buffer[size++] = c;
    }

    if (buffer) {
        char* temp = realloc(buffer, size + 1);
        if (!temp) {
            free(buffer);
            return NULL;
        }
        buffer = temp;
        buffer[size] = '\0';
    }

    return buffer;
}

void handleClientMessage(int client_fd, char* message) {
    if (message == NULL) return;

    char* client_name = "Unknown";
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket == client_fd) {
            client_name = clients[i].name;
            break;
        }
    }

    pthread_mutex_lock(&dict_mutex);

    char* command = strtok(message, "*");
    char cmd_type = command[0];
    char* word = NULL;
    char* definition = NULL;

    char* msg;
    if (cmd_type == 'C') {
        word = strtok(NULL, "\n");
        asprintf(&msg, "User %s has requested search word command\n", client_name);
        printF(msg);
        free(msg);
        handleSearch(client_fd, word);
    } else if (cmd_type == 'A') {
        word = strtok(NULL, "*");
        definition = strtok(NULL, "\n");
        asprintf(&msg, "User %s has requested add word command\n", client_name);
        printF(msg);
        free(msg);
        handleAdd(client_fd, word, definition);
    } else if (cmd_type == 'L') {
        asprintf(&msg, "User %s has requested list words command\n", client_name);
        printF(msg);
        free(msg);
        handleList(client_fd);
    }

    pthread_mutex_unlock(&dict_mutex);
}

void loadDictionary(char* filename) {
    dict.filename = strdup(filename);
    int fd = open(filename, O_RDONLY);

    char* num_words_str = readUntil(fd, '\n');
    dict.numWords = atoi(num_words_str);
    free(num_words_str);

    dict.entries = malloc(sizeof(DictionaryEntry) * dict.numWords);

    for (int i = 0; i < dict.numWords; i++) {
        char* line = readUntil(fd, '\n');
        char* colon = strchr(line, ':');

        int word_len = colon - line;
        int def_len = strlen(colon + 1);

        dict.entries[i].word = malloc(word_len + 1);
        dict.entries[i].definition = malloc(def_len + 1);

        strncpy(dict.entries[i].word, line, word_len);
        dict.entries[i].word[word_len] = '\0';
        strcpy(dict.entries[i].definition, colon + 1);

        free(line);
    }

    close(fd);
}

void handleSearch(int client_fd, char* word) {
    char* msg;
    asprintf(&msg, "Searching for word -> %s\n", word);
    printF(msg);
    free(msg);

    for (int i = 0; i < dict.numWords; i++) {
        if (strcmp(dict.entries[i].word, word) == 0) {
            char* response;
            asprintf(&response, "D*%s*%s\n", word, dict.entries[i].definition);
            write(client_fd, response, strlen(response));
            free(response);
            return;
        }
    }

    char* error;
    asprintf(&error, "E* The word %s has not been found in the dictionary.\n", word);
    write(client_fd, error, strlen(error));
    free(error);
}

void handleAdd(int client_fd, char* word, char* definition) {
    for (int i = 0; i < dict.numWords; i++) {
        if (strcmp(dict.entries[i].word, word) == 0) {
            char* error;
            asprintf(&error, "E* The word %s is already in the dictionary.\n", word);
            write(client_fd, error, strlen(error));
            free(error);
            return;
        }
    }

    dict.entries = realloc(dict.entries, sizeof(DictionaryEntry) * (dict.numWords + 1));
    dict.entries[dict.numWords].word = strdup(word);
    dict.entries[dict.numWords].definition = strdup(definition);
    dict.numWords++;

    updateDictionaryFile(dict.filename);

    char* response;
    asprintf(&response, "OK* The word %s has been added to the dictionary.\n", word);
    write(client_fd, response, strlen(response));
    free(response);
}

void handleList(int client_fd) {
    char* response;
    asprintf(&response, "L*%d*", dict.numWords);
    write(client_fd, response, strlen(response));
    free(response);

    for (int i = 0; i < dict.numWords; i++) {
        write(client_fd, dict.entries[i].word, strlen(dict.entries[i].word));
        write(client_fd, "\n", 1);
    }
}

void updateDictionaryFile(char* filename) {
    int fd = open(filename, O_WRONLY | O_TRUNC);
    if (fd < 0) {
        printF(RED "Error updating dictionary file\n" RESET);
        return;
    }

    char* num_str;
    asprintf(&num_str, "%d\n", dict.numWords);
    write(fd, num_str, strlen(num_str));
    free(num_str);

    for (int i = 0; i < dict.numWords; i++) {
        char* entry;
        asprintf(&entry, "%s:%s\n", dict.entries[i].word, dict.entries[i].definition);
        write(fd, entry, strlen(entry));
        free(entry);
    }

    close(fd);
}

void freeResources() {
    for (int i = 0; i < dict.numWords; i++) {
        free(dict.entries[i].word);
        free(dict.entries[i].definition);
    }
    free(dict.entries);
    free(dict.filename);
}

void handleExit() {
    printF(RED "Server shutting down...\n" RESET);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket > 0) {
            close(clients[i].socket);
            if (clients[i].name) {
                free(clients[i].name);
                clients[i].name = NULL;
            }
            clients[i].socket = 0;
        }
    }

    pthread_mutex_destroy(&dict_mutex);
    freeResources();

    if (server_fd > 0) {
        close(server_fd);
    }

    exit(0);
}

void startServer(char* ip, int port) {
    struct sockaddr_in address;
    fd_set readfds;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        printF(RED "Socket creation failed\n" RESET);
        exit(1);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(ip);
    address.sin_port = htons(port);

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        printF(RED "Setsockopt failed\n" RESET);
        exit(1);
    }

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        printF(RED "Bind failed\n" RESET);
        exit(1);
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        printF(RED "Listen failed\n" RESET);
        exit(1);
    }

    printF(GRN "Server started. Waiting for connections...\n" RESET);
    printF("Dictionary server started\n");
    printF("Opening connections...\n");
    printF("Waiting for connection...\n");

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        int max_sd = server_fd;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = clients[i].socket;
            if (sd > 0) {
                FD_SET(sd, &readfds);
                max_sd = (sd > max_sd) ? sd : max_sd;
            }
        }

        if (select(max_sd + 1, &readfds, NULL, NULL, NULL) < 0) {
            if (errno == EINTR) continue;
            printF(RED "Select error\n" RESET);
            continue;
        }

        if (FD_ISSET(server_fd, &readfds)) {
            int new_socket = accept(server_fd, NULL, NULL);
            if (new_socket < 0) {
                printF(RED "Accept failed\n" RESET);
                continue;
            }

            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].socket == 0) {
                    clients[i].socket = new_socket;
                    char* name = readUntil(new_socket, '\n');
                    if (name) {
                        char* actual_name = name;
                        if (strncmp(name, "U*", 2) == 0) {
                            actual_name = name + 2;
                        }
                        clients[i].name = strdup(actual_name);
                        char* msg;
                        asprintf(&msg, "New user connected: %s\n", actual_name);
                        printF(msg);
                        free(msg);
                        free(name);
                    }
                    break;
                }
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = clients[i].socket;
            if (sd > 0 && FD_ISSET(sd, &readfds)) {
                char* message = readUntil(sd, '\n');
                if (message == NULL) {
                    char* msg;
                    asprintf(&msg, "New exit petition: %s has left the server\n",
                        clients[i].name ? clients[i].name : "Unknown");
                    printF(msg);
                    free(msg);

                    if (clients[i].name) {
                        free(clients[i].name);
                        clients[i].name = NULL;
                    }
                    close(clients[i].socket);
                    clients[i].socket = 0;
                } else {
                    handleClientMessage(sd, message);
                    free(message);
                }
            }
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        printF(RED "Usage: ./S7 <ip> <port> <dictionary_file>\n" RESET);
        return 1;
    }

    signal(SIGINT, handleExit);
    loadDictionary(argv[3]);
    startServer(argv[1], atoi(argv[2]));

    return 0;
}
