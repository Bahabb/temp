#include "websocket.h"
#include "protocolhandler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int websocket_send_connectionsList(websocket_service* ws, hashMap* hash) {
    cJSON* clients = cJSON_CreateArray();
    if (!clients) {
        fprintf(stderr, "[ERROR] [websocket/websocket_send_connectionsList] cJSON_CreateArray fail");
        return 1;
    }
    pthread_mutex_lock(&hash_mutex);
    for (int i = 0; i < hash->size; i++) {
        Node* node = hash->buckets[i];
        while (node) {
            cJSON* client = cJSON_CreateObject();
            if (!client) {
                fprintf(stderr, "[ERROR] [websocket/websocket_send_connectionsList] cJSON_CreateObject fail");
                return 1;
            }
            cJSON_AddStringToObject(client, "id", node->client->id);
            cJSON_AddStringToObject(client, "ip", node->client->ip);
            cJSON_AddItemToArray(clients, client);
            node = node->next;
        }
    }
    pthread_mutex_unlock(&hash_mutex);
    char* payload = cJSON_PrintUnformatted(clients);

    PROTOCOL_MESSAGE* msg = protocol_create_msg(LIST_UPDATE, CONNECTION_LIST, REACTFRONT, CSERVER, NULL, payload, -1, strlen(payload));
    if (!msg) {
        fprintf(stderr, "[ERROR] [websocket/websocket_send_connectionsList] protocol_create_msg fail");
        return 1;
    }

    char* jsonMsg = protocol_create_jsonMsg(msg);
    if (!jsonMsg) {
        fprintf(stderr, "[ERROR] [websocket/websocket_send_connectionsList] protocol_create_jsonMsg fail");
        return 1;
    }

    if (websocket_send(ws, jsonMsg) != 0) {
        fprintf(stderr, "[ERROR] [websocket/websocket_send_connectionsList] websocket_send fail");
        return 1;
    }

    free(jsonMsg);
    free(payload);
    delete_protocol_msg(msg);
    cJSON_Delete(clients);
    return 0;
}

static const struct lws_extension exts[] = {
    { NULL, NULL, NULL }
};

static void generate_masking_key(unsigned char* key) {
    for (int i = 0; i < 4; i++) {
        key[i] = rand() % 256;
    }
}

static void apply_mask(unsigned char* data, size_t len, unsigned char* mask) {
    for (size_t i = 0; i < len; i++) {
        data[i] ^= mask[i % 4];
    }
}

static int send_raw_message(struct lws* wsi, const char* message) {
    int fd = lws_get_socket_fd(wsi);
    if (fd < 0) {
        fprintf(stderr, "[ERROR] Invalid socket fd\n");
        return -1;
    }

    size_t payload_len = strlen(message);
    unsigned char frame[14 + payload_len];
    unsigned char* p = frame;
    unsigned char mask_key[4];

    generate_masking_key(mask_key);

    *p = 0x81;
    p++;

    if (payload_len < 126) {
        *p = (unsigned char)payload_len | 0x80;
        p++;
    } else if (payload_len <= 65535) {
        *p = 126 | 0x80;
        p++;
        *p = (payload_len >> 8) & 0xFF;
        p++;
        *p = payload_len & 0xFF;
        p++;
    } else {
        *p = 127 | 0x80;
        p++;
        memset(p, 0, 7);
        p += 7;
        *p = (payload_len >> 56) & 0xFF;
        p++;
        *p = (payload_len >> 48) & 0xFF;
        p++;
        *p = (payload_len >> 40) & 0xFF;
        p++;
        *p = (payload_len >> 32) & 0xFF;
        p++;
        *p = (payload_len >> 24) & 0xFF;
        p++;
        *p = (payload_len >> 16) & 0xFF;
        p++;
        *p = (payload_len >> 8) & 0xFF;
        p++;
        *p = payload_len & 0xFF;
        p++;
    }

    memcpy(p, mask_key, 4);
    p += 4;

    memcpy(p, message, payload_len);
    p += payload_len;

    apply_mask(p - payload_len, payload_len, mask_key);

    ssize_t sent = write(fd, frame, p - frame);
    if (sent < 0) {
        perror("[ERROR] write in send_raw_message failed");
        return -1;
    }
    printf("Sent raw message: %s (bytes: %zd, frame size: %zu)\n", message, sent, p - frame);
    return 0;
}

static int callback(struct lws* wsi [[maybe_unused]], enum lws_callback_reasons reason, void* user [[maybe_unused]], void* in, size_t len [[maybe_unused]]) {

    switch (reason) {
        case LWS_CALLBACK_CLIENT_APPEND_HANDSHAKE_HEADER:
            printf("Appending handshake headers\n");
            break;
        case LWS_CALLBACK_WSI_CREATE:
            printf("WebSocket client WSI created\n");
            break;
        case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
            fprintf(stderr, "WebSocket connection failed: %s\n", in ? (char*)in : "No error details");
            *websocket_global_wss->running = 0;
            break;
        case LWS_CALLBACK_ESTABLISHED:
            printf("WebSocket connected\n");
            break;
        case LWS_CALLBACK_CLIENT_WRITEABLE:
            printf("Connection is writeable\n");
            break;
        case LWS_CALLBACK_CLOSED:
            printf("WebSocket disconnected\n");
            *websocket_global_wss->running = 0;
            break;
        case LWS_CALLBACK_CLIENT_RECEIVE:
            printf("Received from web server: %.*s\n", (int)len, (char*)in);
            handle_received_message((char*)in);
            break;
        default:
            break;
    }
    return 0;
}

static struct lws_protocols protocols[] = {
    {"", callback, 0, 0, 0},
    {NULL, NULL, 0, 0, 0}
};

websocket_service* websocket_init(volatile sig_atomic_t* server_running, Queue* output_queue, hashMap* client_hash) {
    websocket_service* service = malloc(sizeof(websocket_service));
    if (!service) {
        fprintf(stderr, "[ERROR] [websocket/websocket_init] Failed to allocate memory for websocket_service\n");
        return NULL;
    }
    service->running = server_running;
    service->output_queue = output_queue;
    service->client_hash = client_hash;

    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    info.port = CONTEXT_PORT_NO_LISTEN;
    info.protocols = protocols;
    info.extensions = exts;
    info.options = LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    info.user = service;

    service->context = lws_create_context(&info);
    if (!service->context) {
        fprintf(stderr, "[ERROR] [websocket/websocket_init] lws_create_context failed\n");
        free(service);
        return NULL;
    }

    struct lws_client_connect_info ccinfo = {0};
    ccinfo.context = service->context;
    ccinfo.address = "localhost";
    ccinfo.port = 8080;
    ccinfo.path = "/";
    ccinfo.host = ccinfo.address;
    ccinfo.origin = ccinfo.address;
    ccinfo.protocol = protocols[0].name;

    service->wsi = lws_client_connect_via_info(&ccinfo);
    if (!service->wsi) {
        fprintf(stderr, "[ERROR] [websocket/websocket_init] WebSocket client connect failed\n");
        lws_context_destroy(service->context);
        free(service);
        return NULL;
    }

    return service;
}

void websocket_destroy(websocket_service* service) {
    if (service) {
        lws_context_destroy(service->context);
        free(service);
    }
}

int websocket_send(websocket_service* service, const char* message) {
    if (!service || !service->wsi || lws_get_socket_fd(service->wsi) < 0) {
        fprintf(stderr, "[ERROR] [websocket/websocket_send] Invalid WebSocket service or connection\n");
        return -1;
    }
    return send_raw_message(service->wsi, message);
}

void* websocket_thread(void* arg) {
    websocket_service* service = (websocket_service*)arg;

    struct lws_client_connect_info ccinfo = {0};
    ccinfo.context = service->context;
    ccinfo.address = "localhost";
    ccinfo.port = 8080;
    ccinfo.path = "/";
    ccinfo.host = ccinfo.address;
    ccinfo.origin = ccinfo.address;
    ccinfo.protocol = protocols[0].name;


    while (*service->running) {

        lws_service(service->context, 5);

        if (!service->wsi || lws_get_socket_fd(service->wsi) < 0) {
            fprintf(stderr, "[INFO] WebSocket connection lost, attempting to reconnect...\n");
            service->wsi = lws_client_connect_via_info(&ccinfo);
            if (!service->wsi) {
                fprintf(stderr, "[ERROR] WebSocket reconnect failed\n");
                sleep(5); // Wait before retrying
                continue;
            }
            fprintf(stderr, "[INFO] WebSocket reconnected\n");
        }

        char* output = queue_pop(service->output_queue);
        if (output) {
            if (websocket_send(service, output) < 0) {
                fprintf(stderr, "[ERROR] [websocket/websocket_thread] Failed to send raw message\n");
            } else {
                printf("Successfully sent raw message\n");
            }
            free(output);
        } else {
            printf("No message in queue, serviced libwebsockets\n");
        }
    }
    return NULL;
}
