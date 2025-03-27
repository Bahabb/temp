#ifndef COMMON_H  // Include guard
#define COMMON_H

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32")
#define close(s) closesocket(s) // to simply use close() in all the code
#define ERRNO WSAGetLastError()
#else
#include <pthread.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#define ERRNO errno
#endif
#include <libwebsockets.h>
#include "cJSON.h"
#include "cJSON_Utils.h"

#define MAX_QUEUE 5
#define BUFFER_SIZE 2048
#define SERVER_PORT 3333
#define SERVER_IP "127.0.0.1"


// Any other common definitions, structs, or function prototypes can go here

#endif
