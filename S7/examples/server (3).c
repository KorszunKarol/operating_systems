#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>

#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define YEL   "\x1B[33m"
#define BLU   "\x1B[34m"
#define MAG   "\x1B[35m"
#define CYN   "\x1B[36m"
#define WHT   "\x1B[37m"
#define RESET "\x1B[0m"
#define MAX_CHALLENGES 10

typedef struct {
    int sockfd;
    int challenge_number;
    char *current_challenge;
    char* question;
    char* answer;
    char* name;
    char* hint;
} ClientState;

typedef struct {
    char* question;
    char* answer;
    char* hint;
} Challenge;


Challenge* challenges;
int totalChallenges = 0;

void startServer( int port);
void handleClientConnection(int client_sockfd);
void sendChallenge(int client_sockfd, const char *message);
void handleChallengeResponse(ClientState *clientState, char *response);
char* getHint(int challenge_number);
void clearBuffer(char *buffer, int size);
void loadChallenges();
void parseChallenge(char *line, int index);


void sortir() {
    write(STDOUT_FILENO, RED "Exiting server...\n" RESET, strlen(RED "Exiting server...\n" RESET));
    
    for (int i = 0; i < MAX_CHALLENGES; i++) {
        free(challenges[i].question);
        free(challenges[i].answer);
        free(challenges[i].hint);
    }
    free(challenges);

    signal(SIGINT, SIG_DFL);
    raise(SIGINT);
}

void displayBanner() {
    const char *banner =
        "\033[1;36m" // Cyan color start
        "\n"
        " __________\n"
        "\\/\\____;;___\\\n"
        "| /         /\n"
        "`. ())oo() .\n"
        " |\\(%()*^^()^\\\n"
        " | |-%-------|\n"
        "  \\ | %  ))   |\n"
        "   \\|%________|\n"
        "\033[0m" // Reset to default color
        "\nWelcome to the Guardian of Enigmas Server. Prepare to embark on a journey of puzzles and mysteries!\n";

    write(STDOUT_FILENO, banner, strlen(banner));
}



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

void loadChallenges() {
    int fd = open("challenges.txt", O_RDONLY);
    if (fd < 0) {
        perror(RED "Failed to open challenge file" RESET);
    }else{
        challenges = (Challenge *)malloc(sizeof(Challenge));
        challenges[totalChallenges].question = readUntil(fd, '|');

        while(challenges[totalChallenges].question != NULL){
            challenges[totalChallenges].answer = readUntil(fd, '&');
            challenges[totalChallenges].hint = readUntil(fd, '\n');

            totalChallenges++;
            challenges = (Challenge *)realloc(challenges, sizeof(Challenge) * (totalChallenges + 1));
            challenges[totalChallenges].question = readUntil(fd, '|');
        }

        totalChallenges--;
    }

    close(fd);
}

// This will handle connection for each client
void *client_handler(void *socket_desc) {
    int sock = *(int*)socket_desc; 
    handleClientConnection(sock);
    
    return 0;
}


void startServer(int port) {
    int sockfd, client_sockfd;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen;
    pthread_t thread_id;  

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror(RED "Error opening socket" RESET);
        exit(EXIT_FAILURE);
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror(RED "Error on binding" RESET);
        exit(EXIT_FAILURE);
    }

    listen(sockfd, 10);  // Increase backlog to queue more incoming connection requests
    clilen = sizeof(cli_addr);

    while (1) {
        client_sockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &clilen);
        if (client_sockfd < 0) {
            perror(RED "Error on accept" RESET);
            continue;
        }
        
        // Create a new thread for each client
        if(pthread_create(&thread_id, NULL, client_handler, (void*) &client_sockfd) < 0) {
            perror("Could not create thread");
            return;
        }

        // Optionally detach the thread - lets the thread release resources on termination
        pthread_detach(thread_id);
    }
    close(sockfd);
}



void handleClientConnection(int client_sockfd) {
    char* buffer;
    ClientState clientState = {client_sockfd, 1, NULL, NULL, 0,0, 0};
    char* hint = NULL;
    char *buffer2 = NULL;
    // Read the client's name and save it in the clientState struct
    clientState.name = readUntil(client_sockfd, '\n');
    write(STDOUT_FILENO, "Welcome ", strlen("Welcome "));
    write(STDOUT_FILENO, clientState.name, strlen(clientState.name));
    write(STDOUT_FILENO, "!\n", strlen("!\n"));
   
 
    while (1) {
        buffer = readUntil(client_sockfd, '\n');
        switch (atoi(buffer)) {
            case 1:
                write(STDOUT_FILENO, clientState.name, strlen(clientState.name));
                asprintf(&buffer2, BLU" - request challenge...\n" RESET);
                write(STDOUT_FILENO, buffer2, strlen(buffer2));
                free(buffer2);
                if (clientState.challenge_number > MAX_CHALLENGES) {
                    sendChallenge(client_sockfd, BLU "No more challenges. Press 4 to get the treasure coordinates." RESET);
                    break;
                }
                sendChallenge(client_sockfd, challenges[clientState.challenge_number - 1].question);          
                
                break;
            case 2:
                write(STDOUT_FILENO, clientState.name, strlen(clientState.name));
                asprintf(&buffer2, BLU" - sending answer...\n" RESET);
                write(STDOUT_FILENO, buffer2, strlen(buffer2));
                free(buffer2);
                if (clientState.challenge_number > MAX_CHALLENGES) {
                    sendChallenge(client_sockfd, BLU "No more challenges. Press 4 to get the treasure coordinates." RESET);
                    break;
                } 
                buffer = readUntil(client_sockfd, '\n');
                asprintf(&buffer2, YEL"Checking answer... \n" RESET);
                write(STDOUT_FILENO, buffer2, strlen(buffer2));
                free(buffer2);
                handleChallengeResponse(&clientState, buffer);
  
                break;
            case 3:
               write(STDOUT_FILENO, clientState.name, strlen(clientState.name));
                asprintf(&buffer2, BLU" - request hint...\n" RESET);
                write(STDOUT_FILENO, buffer2, strlen(buffer2));
                free(buffer2);
                if (clientState.challenge_number > MAX_CHALLENGES) {
                    sendChallenge(client_sockfd, BLU "No more challenges. Press 4 to get the treasure coordinates." RESET);
                    break;
                }
                hint = getHint(clientState.challenge_number);
                sendChallenge(client_sockfd, hint);
                asprintf(&buffer2, YEL"Hint sent! \n" RESET);
                write(STDOUT_FILENO, buffer2, strlen(buffer2));
                free(buffer2);
                free(hint);
                break;
            case 4:
                write(STDOUT_FILENO, clientState.name, strlen(clientState.name));
                asprintf(&buffer2, BLU" - request to view current mission status...\n" RESET);
                write(STDOUT_FILENO, buffer2, strlen(buffer2));
                free(buffer2);
                if (clientState.challenge_number > MAX_CHALLENGES) {
                    sendChallenge(client_sockfd, GRN "Congratulations! You've found the treasure at coordinates: X:100, Y:200. Disconnecting." RESET);
                    return;
                } else {
                    asprintf(&buffer, MAG"You are currently on challenge %d out of %d." RESET, clientState.challenge_number, MAX_CHALLENGES);
                    sendChallenge(client_sockfd, buffer);
                }
                break;
            case 5: 
                write(STDOUT_FILENO, clientState.name, strlen(clientState.name));
                asprintf(&buffer2, BLU" - decide to terminate the connection\n" RESET);
                write(STDOUT_FILENO, buffer2, strlen(buffer2));
                free(buffer2);
                free(clientState.name);
                sendChallenge(client_sockfd, RED"Terminating connection."RESET);
                free(buffer);
                return;
            default:
                sendChallenge(client_sockfd, RED "Invalid option. Please try again." RESET);
                break;
        }
        free(buffer);
        buffer = NULL;
    }
}

void handleChallengeResponse(ClientState *clientState, char *response) {
    if (strcmp(response, challenges[clientState->challenge_number - 1].answer) == 0) {
        clientState->challenge_number++;
        if (clientState->challenge_number > MAX_CHALLENGES) {
            sendChallenge(clientState->sockfd, GRN"Congratulations! You've completed all challenges. Press 4 to get the treasure coordinates."RESET);
        } else {
            sendChallenge(clientState->sockfd, BLU"Correct answer! Proceeding to the next challenge."RESET);
        }
    } else {
        sendChallenge(clientState->sockfd, RED"Incorrect answer, try again."RESET);
    }
}

void sendChallenge(int client_sockfd, const char *message) {
    char *buffer;
    int len = asprintf(&buffer, "%s\n", message);  
    write(client_sockfd, buffer, len); 
    free(buffer);  

}

char* getHint(int challenge_number) {
    if (challenge_number < 1 || challenge_number > MAX_CHALLENGES) {
        char *error_msg;
        asprintf(&error_msg, "%sNo valid challenge for hint.%s", RED, RESET);
        free(error_msg);
        return error_msg;
    } else {
        char *hint;
        asprintf(&hint, "%s%s%s", CYN, challenges[challenge_number - 1].hint, RESET);
        return hint;
    }  
}




int main(int argc, char *argv[]) {

    if (argc != 3) {
        write(STDOUT_FILENO, RED "Usage: ./server <ip> <port>\n" RESET, strlen(RED "Usage: ./server <ip> <port>\n" RESET));
        exit(EXIT_FAILURE);
    }

    signal(SIGINT, sortir);

    loadChallenges();
    displayBanner();
    startServer(atoi(argv[2])); 
    return 0;
}
