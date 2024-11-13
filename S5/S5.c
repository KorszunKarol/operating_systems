// Name: Karol
// Last Name: Korszun

#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <stdio.h>
#include <signal.h>
#include <arpa/inet.h>
#include <errno.h>

#define MAX_CLIENTS 10
#define MAX_CHALLENGES 20
#define BUFFER_SIZE 2048
#define MSG_SIZE 256
#define COORDINATES "The treasure is at coordinates: X:42.3456, Y:-71.0987\n"

#define ERROR_MSG_ARGS "Usage: ./S5 <ip_address> <port>\n"
#define ERROR_MSG_FILE "Error opening challenges file\n"
#define ERROR_MSG_SOCKET "Error creating socket\n"
#define ERROR_MSG_BIND "Error binding socket\n"
#define ERROR_MSG_LISTEN "Error listening\n"

#define WELCOME_MSG "Welcome to the Guardian of Enigmas Server!\n"

#define C_RESET   "\033[0m"
#define C_RED     "\033[31m"
#define C_GREEN   "\033[32m"
#define C_YELLOW  "\033[33m"
#define C_BLUE    "\033[34m"
#define C_MAGENTA "\033[35m"

#define PRINT_MSG(msg) write(1, msg "\n", strlen(msg) + 1)
#define PRINT_COLOR(color, msg) { \
    write(1, color, strlen(color)); \
    write(1, msg, strlen(msg)); \
    write(1, C_RESET "\n", strlen(C_RESET) + 1); \
}

#define CMD_CHALLENGE '1'
#define CMD_ANSWER    '2'
#define CMD_HINT      '3'
#define CMD_STATUS    '4'
#define CMD_EXIT      '5'

typedef struct {
    char question[256];
    char answer[256];
    char hint[256];
} Challenge;

typedef struct {
    Challenge challenges[MAX_CHALLENGES];
    int total_challenges;
    int current_challenge[MAX_CLIENTS];
    int completed[MAX_CLIENTS];
    int socket_fd[MAX_CLIENTS];
    int running;
} GameState;

typedef struct {
    pthread_t *threads;
    int thread_count;
    int server_fd;
    GameState *game;
} ServerContext;

typedef struct {
    GameState *game;
    int sock;
    int id;
    char *name;
} ClientArgs;

void cleanup_server(ServerContext *ctx);
void *handle_client(void *arg);
void load_challenges(GameState *game, const char *filename);

static ServerContext *global_ctx = NULL;

void signal_handler(int signum) {
    if (signum == SIGINT || signum == SIGTERM) {
        PRINT_COLOR(C_RED, "Exiting server...");
        if (global_ctx) {
            cleanup_server(global_ctx);
        }
        exit(0);
    }
}

void load_challenges(GameState *game, const char *filename) {
    int fd = open(filename, O_RDONLY);
    if (fd == -1) {
        write(1, ERROR_MSG_FILE, strlen(ERROR_MSG_FILE));
        exit(1);
    }

    char line[BUFFER_SIZE];
    int pos = 0;
    char c;
    game->total_challenges = 0;

    while (read(fd, &c, 1) > 0 && game->total_challenges < MAX_CHALLENGES) {
        if (c != '\n') {
            line[pos++] = c;
        } else {
            line[pos] = '\0';
            pos = 0;

            char *question = strtok(line, "|");
            char *answer = strtok(NULL, "&");
            char *hint = strtok(NULL, "\n");

            if (question && answer && hint) {
                strncpy(game->challenges[game->total_challenges].question, question, 255);
                strncpy(game->challenges[game->total_challenges].answer, answer, 255);
                strncpy(game->challenges[game->total_challenges].hint, hint, 255);
                game->challenges[game->total_challenges].question[255] = '\0';
                game->challenges[game->total_challenges].answer[255] = '\0';
                game->challenges[game->total_challenges].hint[255] = '\0';
                game->total_challenges++;
            }
        }
    }
    close(fd);
}

void *handle_client(void *arg) {
    ClientArgs *args = (ClientArgs*)arg;
    int sock = args->sock;
    GameState *game = args->game;
    int id = args->id;
    char *name = args->name;
    free(args);

    char buf[BUFFER_SIZE];
    int first_command = 1;
    int n;

    while ((n = read(sock, buf, BUFFER_SIZE - 1)) > 0) {
        buf[n] = '\0';
        Challenge *curr = &game->challenges[game->current_challenge[id]];
        char cmd = buf[0];

        char *msg = malloc(MSG_SIZE + strlen(name) + 50);
        if (!msg) continue;

        switch(cmd) {
            case CMD_CHALLENGE:
                snprintf(msg, MSG_SIZE + strlen(name) + 50, "%s - request challenge...", name);
                PRINT_COLOR(C_BLUE, msg);
                if (first_command) {
                    first_command = 0;
                    write(sock, curr->question, strlen(curr->question));
                    write(sock, "\n", 1);
                } else if (game->current_challenge[id] < game->total_challenges) {
                    write(sock, curr->question, strlen(curr->question));
                    write(sock, "\n", 1);
                }
                free(msg);
                break;

            case CMD_ANSWER: {
                snprintf(msg, MSG_SIZE + strlen(name) + 50, "%s - sending answer...", name);
                PRINT_COLOR(C_BLUE, msg);
                free(msg);

                char *answer = malloc(BUFFER_SIZE);
                if (!answer) continue;

                n = read(sock, answer, BUFFER_SIZE - 1);
                if (n > 0) {
                    answer[n] = '\0';
                    if (answer[n-1] == '\n') answer[n-1] = '\0';

                    PRINT_COLOR(C_YELLOW, "Checking answer...");
                    if (strcmp(answer, curr->answer) == 0) {
                        game->completed[id]++;
                        game->current_challenge[id]++;
                        write(sock, "Correct!\n", 9);
                    } else {
                        write(sock, "Incorrect\n", 10);
                    }
                }
                free(answer);
                continue;
            }

            case CMD_HINT: {
                snprintf(msg, MSG_SIZE + strlen(name) + 50, "%s - request hint...", name);
                PRINT_COLOR(C_BLUE, msg);
                write(sock, curr->hint, strlen(curr->hint));
                write(sock, "\n", 1);
                PRINT_COLOR(C_GREEN, "Hint sent!");
                free(msg);
                break;
            }

            case CMD_STATUS: {
                snprintf(msg, MSG_SIZE + strlen(name) + 50, "%s - request to view current mission status...", name);
                PRINT_COLOR(C_BLUE, msg);
                free(msg);

                int remaining = game->total_challenges - game->completed[id];
                char status[256];

                if (game->completed[id] >= game->total_challenges) {
                    PRINT_COLOR(C_GREEN, "Congratulations! You've completed all challenges!");
                    write(sock, "Congratulations! You've found the treasure at coordinates: X:100, Y:200. Disconnecting.\n", 85);
                    PRINT_COLOR(C_MAGENTA, "Treasure coordinates sent!");
                    goto cleanup;
                } else {
                    int len = snprintf(status, sizeof(status), "Challenges remaining: %d\n", remaining);
                    write(sock, status, len);
                }
                break;
            }

            case CMD_EXIT: {
                snprintf(msg, MSG_SIZE + strlen(name) + 50, "%s - decide to terminate the connection", name);
                PRINT_COLOR(C_BLUE, msg);
                free(msg);
                goto cleanup;
            }
        }
    }

cleanup:
    free(name);
    PRINT_COLOR(C_RED, "Client disconnected");
    game->socket_fd[id] = -1;
    close(sock);
    return NULL;
}

void cleanup_server(ServerContext *ctx) {
    if (ctx->game) {
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (ctx->game->socket_fd[i] != -1) {
                close(ctx->game->socket_fd[i]);
            }
        }
    }
    if (ctx->server_fd != -1) {
        close(ctx->server_fd);
    }
    if (ctx->threads) {
        for (int i = 0; i < ctx->thread_count; i++) {
            pthread_cancel(ctx->threads[i]);
        }
        free(ctx->threads);
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        write(1, ERROR_MSG_ARGS, strlen(ERROR_MSG_ARGS));
        return 1;
    }

    ServerContext ctx = {
        .threads = malloc(sizeof(pthread_t) * MAX_CLIENTS),
        .thread_count = 0,
        .server_fd = -1,
        .game = NULL
    };

    global_ctx = &ctx;

    GameState game;
    ctx.game = &game;
    memset(&game, 0, sizeof(GameState));
    game.running = 1;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        game.socket_fd[i] = -1;
    }

    load_challenges(&game, "challenges.txt");

    ctx.server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (ctx.server_fd < 0) {
        write(1, ERROR_MSG_SOCKET, strlen(ERROR_MSG_SOCKET));
        cleanup_server(&ctx);
        return 1;
    }

    int opt = 1;
    if (setsockopt(ctx.server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                   &opt, sizeof(opt))) {
        write(1, "Error setting socket options\n", 28);
        close(ctx.server_fd);
        cleanup_server(&ctx);
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(atoi(argv[2]));
    if (inet_pton(AF_INET, argv[1], &addr.sin_addr) <= 0) {
        write(1, "Invalid IP address\n", 19);
        close(ctx.server_fd);
        cleanup_server(&ctx);
        return 1;
    }

    if (bind(ctx.server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        write(1, ERROR_MSG_BIND, strlen(ERROR_MSG_BIND));
        close(ctx.server_fd);
        cleanup_server(&ctx);
        return 1;
    }

    if (listen(ctx.server_fd, 5) < 0) {
        write(1, ERROR_MSG_LISTEN, strlen(ERROR_MSG_LISTEN));
        close(ctx.server_fd);
        cleanup_server(&ctx);
        return 1;
    }

    write(1, "Server is running on ", 20);
    write(1, argv[1], strlen(argv[1]));
    write(1, ":", 1);
    write(1, argv[2], strlen(argv[2]));
    write(1, "\n", 1);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    while (game.running) {
        int client_sock = accept(ctx.server_fd, NULL, NULL);
        if (client_sock < 0) continue;

        char *name = malloc(MSG_SIZE);
        if (!name) {
            close(client_sock);
            continue;
        }

        int n = read(client_sock, name, MSG_SIZE - 1);
        if (n > 0) {
            name[n] = '\0';
            if (name[n-1] == '\n') name[n-1] = '\0';
        }

        int id = -1;
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (game.socket_fd[i] == -1) {
                id = i;
                game.socket_fd[i] = client_sock;
                break;
            }
        }

        if (id == -1) {
            free(name);
            close(client_sock);
            continue;
        }

        ClientArgs *args = malloc(sizeof(ClientArgs));
        if (!args) {
            free(name);
            close(client_sock);
            game.socket_fd[id] = -1;
            continue;
        }

        args->game = &game;
        args->sock = client_sock;
        args->id = id;
        args->name = name;

        pthread_t thread;
        if (pthread_create(&thread, NULL, handle_client, args) != 0) {
            free(args);
            free(name);
            close(client_sock);
            game.socket_fd[id] = -1;
            continue;
        }

        ctx.threads[ctx.thread_count++] = thread;
        pthread_detach(thread);
    }

    cleanup_server(&ctx);
    return 0;
}