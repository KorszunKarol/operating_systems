#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define printF(x) write(1, x, strlen(x))

char* read_response(int sock) {
    char* response = NULL;
    int i = 0;
    char c;
    int retries = 0;
    const int MAX_RETRIES = 100;  // 10 seconds total wait time

    while (retries < MAX_RETRIES) {
        ssize_t bytes_read = read(sock, &c, 1);
        if (bytes_read > 0) {
            if (c == '\n') break;
            response = realloc(response, i + 2);
            response[i++] = c;
        } else if (bytes_read == 0) {
            // Connection closed
            free(response);
            return NULL;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // No data available, wait and retry
            usleep(100000);  // 100ms delay
            retries++;
            continue;
        } else {
            // Error occurred
            free(response);
            return NULL;
        }
    }

    if (response) {
        response[i] = '\0';
        printF("Received: ");
        printF(response);
        printF("\n");
    } else {
        printF("No response received after timeout\n");
    }
    return response;
}

void send_message(int sock, char* msg) {
    char* buffer;
    asprintf(&buffer, "%s\n", msg);

    ssize_t bytes_written = write(sock, buffer, strlen(buffer));
    if (bytes_written < 0) {
        printF("Error sending message: ");
        printF(msg);
        printF("\n");
        free(buffer);
        return;
    }

    printF("Sent: ");
    printF(msg);
    printF("\n");
    free(buffer);

    // Add small delay after sending
    usleep(50000); // 50ms delay

    // Read and print server response
    char* response = read_response(sock);
    if (response) {
        free(response);
    } else {
        printF("Failed to get response for message: ");
        printF(msg);
        printF("\n");
    }
}

void* connect_client(char* ip, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in serv_addr;

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    serv_addr.sin_addr.s_addr = inet_addr(ip);

    if (connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        printF("Connection Failed\n");
        return NULL;
    }

    return (void*)(long)sock;
}

int main() {
    printF("Starting test client...\n");

    int sock1 = (int)(long)connect_client("127.0.0.1", 3000);
    if (sock1 < 0) {
        printF("Failed to connect client 1\n");
        return 1;
    }
    printF("Client 1 connected successfully\n");

    // Add delay between connections
    usleep(500000); // 500ms delay

    int sock2 = (int)(long)connect_client("127.0.0.1", 3000);
    if (sock2 < 0) {
        printF("Failed to connect client 2\n");
        close(sock1);
        return 1;
    }
    printF("Client 2 connected successfully\n");

    // Send initial names with delay between them
    send_message(sock1, "Client1");
    usleep(100000); // 100ms delay
    send_message(sock2, "Client2");
    usleep(100000); // 100ms delay

    // Test concurrent dictionary modifications
    printF("\n=== Testing concurrent add operations ===\n");
    send_message(sock1, "A*test1*definition1");
    usleep(50000); // 50ms delay
    send_message(sock2, "A*test1*definition2"); // Should fail

    // Test concurrent searches
    printF("\n=== Testing concurrent search operations ===\n");
    send_message(sock1, "C*test1");
    usleep(50000); // 50ms delay
    send_message(sock2, "C*test1");

    // Test list while adding
    printF("\n=== Testing list while adding ===\n");
    send_message(sock1, "L*");
    usleep(50000); // 50ms delay
    send_message(sock2, "A*test2*definition2");

    // Final verification
    printF("\n=== Final verification ===\n");
    send_message(sock1, "L*");
    send_message(sock2, "L*");

    close(sock1);
    close(sock2);
    return 0;
}