#ifndef WEBSOCKET_H
#define WEBSOCKET_H

#include <libwebsockets.h>
#include "client_mgmt.h"
#include <signal.h>

typedef struct websocket_service {
    struct lws_context* context;
    struct lws* wsi;
    struct lws_client_connect_info* ccinfo:
    volatile sig_atomic_t* running;
    Queue* output_queue;
    hashMap* client_hash;
} websocket_service;

extern websocket_service* websocket_global_wss;

int websocket_send_connectionsList(websocket_service* ws, hashMap* hash);

websocket_service* websocket_init(volatile sig_atomic_t* server_running, Queue* output_queue, hashMap* client_hash);
void websocket_destroy(websocket_service* service);
int websocket_send(websocket_service* service, const char* message);
void* websocket_thread(void* arg);

#endif
