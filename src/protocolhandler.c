#include "cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "client_mgmt.h"
#include "websocket.h"
#include "common.h"

#define CSERVER "MAIN"
#define WEBSERVER "WEBSOCK"
#define REACTFRONT "FRONTEND"


enum PROTOCOL_MESSAGE_TYPES {
    CONNECT = 1, // Sent by CLIENT to C SERVER, C SERVER sends LIST_UPDATE
    BEACON, // Sent by CLIENT
    DISCONNECT, // Sent by CLIENT to C SERVER, C SERVER sends LIST_UPDATE

    REQUEST,
    RESPONSE,

    SELECT_CLIENT, // Sent by REACT FRONTEND to C SERVER
    COMMAND, // Sent by REACT FRONTEND to C SERVER
    LIST_UPDATE, // Sent by C SERVER to REACT FRONTEND
};

enum PROTOCOl_CONTENT_TYPE {
    CMD_OUTPUT = 1,
    CONNECTION_LIST,
};

typedef struct PROTOCOL_MESSAGE {
    enum PROTOCOL_MESSAGE_TYPES msg_type;
    enum PROTOCOl_CONTENT_TYPE content_type;

    char* destination;
    char* source;

    char* payload;
    int payload_size;

    char* specifiedClient_id;
    int clientID_size;
} PROTOCOL_MESSAGE;


typedef struct contentMap {
    char* str;
    enum PROTOCOl_CONTENT_TYPE enu;
} contentMap;

typedef struct messageTypeMap {
    char* str;
    enum PROTOCOL_MESSAGE_TYPES enu;
} messageTypeMap;

contentMap contentTypes[] = {
    {"CMD_OUTPUT", CMD_OUTPUT},
    {"CONNECTION_LIST", CONNECTION_LIST},
    {"NULL", 0},
};

messageTypeMap msgTypes[] = {
    {"CONNECT", CONNECT},
    {"BEACON", BEACON},
    {"DISCONNECT", DISCONNECT},
    {"REQUEST", REQUEST},
    {"RESPONSE", RESPONSE},
    {"SELECT_CLIENT", SELECT_CLIENT},
    {"COMMAND", COMMAND},
    {"LIST_UPDATE", LIST_UPDATE},
    {"NULL", 0},
};

void delete_protocol_msg(PROTOCOL_MESSAGE* msg) {
    if (msg) {
        free(msg->destination);
        msg->destination = NULL; // prevent use-after-free
        free(msg->source);
        msg->source = NULL;
        free(msg->specifiedClient_id);
        msg->specifiedClient_id = NULL;
        free(msg->payload);
        msg->payload = NULL;
        free(msg);
    }
}

PROTOCOL_MESSAGE* parse_message(char* jsonString) {
    PROTOCOL_MESSAGE* msgStruct = malloc(sizeof(PROTOCOL_MESSAGE));
    if (!msgStruct) {
        fprintf(stderr, "[ERROR] [protocolhandler/parse_message] Error at allocating memory for msgStruct\n");
        return NULL;
    }
    msgStruct->destination = NULL;
    msgStruct->source = NULL;
    msgStruct->specifiedClient_id = NULL;
    msgStruct->payload = NULL;

    cJSON* jsonStruct = cJSON_Parse(jsonString);
    if (!jsonStruct) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr) printf("JSON Parse Error: %s\n", error_ptr);
        delete_protocol_msg(msgStruct);
        return NULL;
    }

    cJSON* type = cJSON_GetObjectItem(jsonStruct, "type");
    if (cJSON_IsString(type)) {
        int found = 0;
        for (int i = 0; msgTypes[i].str != NULL; i++) {
            if (strcmp(msgTypes[i].str, type->valuestring) == 0) {
                msgStruct->msg_type = msgTypes[i].enu;
                found = 1;
                break;
            }
        }
        if (!found) {
            fprintf(stderr, "[ERROR] [protocolhandler/parse_message] Invalid message type: %s\n", type->valuestring);
            delete_protocol_msg(msgStruct);
            cJSON_Delete(jsonStruct);
            return NULL;
        }
    } else {
        fprintf(stderr, "[ERROR] [protocolhandler/parse_message] Missing or invalid 'type'\n");
        delete_protocol_msg(msgStruct);
        cJSON_Delete(jsonStruct);
        return NULL;
    }


    cJSON* content = cJSON_GetObjectItem(jsonStruct, "content");
    if (cJSON_IsString(content)) {
        int found = 0;
        for (int i = 0; contentTypes[i].str != NULL; i++) {
            if (strcmp(contentTypes[i].str, content->valuestring) == 0) {
                msgStruct->content_type = contentTypes[i].enu;
                found = 1;
                break;
            }
        }
        if (!found) {
            fprintf(stderr, "[ERROR] [protocolhandler/parse_message] Invalid content type: %s\n", content->valuestring);
            delete_protocol_msg(msgStruct);
            cJSON_Delete(jsonStruct);
            return NULL;
        }
    }

    cJSON* destination = cJSON_GetObjectItem(jsonStruct, "destination");
    if (cJSON_IsString(destination)) {
        msgStruct->destination = malloc(strlen(destination->valuestring) + 1);
        if (!msgStruct->destination){
            fprintf(stderr, "[ERROR] [protocolhandler/parse_message] Error at allocating memory for msgStruct->destination\n");
            delete_protocol_msg(msgStruct);
            return NULL;
        }
        snprintf(msgStruct->destination, strlen(destination->valuestring) + 1, "%s", destination->valuestring);
    }

    cJSON* source = cJSON_GetObjectItem(jsonStruct, "source");
    if (cJSON_IsString(source)) {
        msgStruct->source = malloc(strlen(source->valuestring) + 1);
        if (!msgStruct->source){
            fprintf(stderr, "[ERROR] [protocolhandler/parse_message] Error at allocating memory for msgStruct->source\n");
            delete_protocol_msg(msgStruct);
            return NULL;
        }

        snprintf(msgStruct->source, strlen(source->valuestring) + 1, "%s", source->valuestring);
    }

    cJSON* selectedClient = cJSON_GetObjectItem(jsonStruct, "selectedClient");
    if (cJSON_IsString(selectedClient)) {
        msgStruct->specifiedClient_id = malloc(strlen(selectedClient->valuestring) + 1);
        if (!msgStruct->specifiedClient_id){
            fprintf(stderr, "[ERROR] [protocolhandler/parse_message] Error at allocating memory for msgStruct->specifiedClient_id\n");
            delete_protocol_msg(msgStruct);
            return NULL;
        }

        snprintf(msgStruct->specifiedClient_id, strlen(selectedClient->valuestring) + 1, "%s", selectedClient->valuestring);
    }

    cJSON* payload_size = cJSON_GetObjectItem(jsonStruct, "payload_size");
    if (cJSON_IsNumber(payload_size)) {
        msgStruct->payload_size = payload_size->valueint;
    }

    cJSON* clientID_size = cJSON_GetObjectItem(jsonStruct, "client_size");
    if (cJSON_IsNumber(clientID_size)) {
        msgStruct->clientID_size = clientID_size->valueint;
    }

    cJSON* payload = cJSON_GetObjectItem(jsonStruct, "payload");
    if (cJSON_IsString(payload)) {
        msgStruct->payload = malloc(BUFFER_SIZE);
        if (msgStruct->payload_size >= BUFFER_SIZE)
            printf("[POSSIBLE ERROR] Received payload size may cause buffer overflow. [payload_size = %d | %d]", msgStruct->payload_size, BUFFER_SIZE);
        if (!msgStruct->payload){
            fprintf(stderr, "[ERROR] [protocolhandler/parse_message] Error at allocating memory for msgStruct->payload\n");
            delete_protocol_msg(msgStruct);
            return NULL;
        }

        int n = snprintf(msgStruct->payload, BUFFER_SIZE, "%s", payload->valuestring);
        if (n >= BUFFER_SIZE) {
            fprintf(stderr, "[ERROR] [protocolhandler/parse_message] Payload exceeds buffer size: %d >= %d\n", n, BUFFER_SIZE);
            printf("TRUNCATED PAYLOAD: \n %s \n", msgStruct->payload);
            delete_protocol_msg(msgStruct);
            cJSON_Delete(jsonStruct);
            return NULL;
        }
    }

    switch(msgStruct->msg_type) {
        case COMMAND:
            if (!msgStruct->specifiedClient_id || !msgStruct->payload) {
                fprintf(stderr, "[ERROR] [protocolhandler/parse_message] COMMAND message missing required fields\n");
                delete_protocol_msg(msgStruct);
                cJSON_Delete(jsonStruct);
                return NULL;
            }
            break;
        default:
            break;
    }

    cJSON_Delete(jsonStruct);
    return msgStruct;
}



void handle_received_message(char* received_jsonMsg) {
    PROTOCOL_MESSAGE* msg = parse_message(received_jsonMsg);
    if (!msg) {
        fprintf(stderr, "[ERROR] [protocolhandler/handle_received_message] Failed to parse message: %s\n", received_jsonMsg);
        // sendtowebsocket("message was dropped (failure to parse)")
        return;
    }

    if (strcmp(msg->source, CSERVER) == 0)
        return;

    // Some need enforcing payload field to be present
    switch (msg->msg_type) {
        case CONNECT:
            // protocol_handle_connect()
            break;
        case DISCONNECT:
            // protocol_handle_disconnect()
            break;
        case BEACON:
            // protocol_handle_beacon()
            break;
        case REQUEST:
            // protocol_handle_request()
            break;
        case RESPONSE:
            // protocol_handle_response()
            break;
        case SELECT_CLIENT:
            // protocol_handle_selectclient()
            break; // Fixed missing break
        case COMMAND:
            // protocol_handle_command()
            break;
        case LIST_UPDATE: // Shouldn't actually be received, only C SERVER sends LIST_UPDATE messages, REACTFRONT sends REQUEST with CONNECTION_LIST as content type
            // protocol_handle_listupdate()
            break;
        default:
            fprintf(stderr, "[ERROR] [protocolhandler/handle_received_message] Unknown message type: %d\n", msg->msg_type);
            break;
    }

    delete_protocol_msg(msg);
}

int protocol_handle_command(PROTOCOL_MESSAGE* msg) {

    if (!msg->specifiedClient_id) {
        fprintf(stderr, "[ERROR] [protocolhandler/protocol_handle_command] Received command frame does not specify a client\n");
        // Send error back
        // Eventually include code that resorts to executing the command for selectedClient that exists in server.c since the frame does not have one
        delete_protocol_msg(msg);
        return 1;
    }
    // Grab the specified client in the frame
    client* specifiedClient = hash_grab(clientHash, msg->specifiedClient_id);

    if (specifiedClient == NULL) {
        fprintf(stderr, "[ERROR] [protocolhandler/protocol_handle_command] Could not find specified client\n");
        // Send error back
        delete_protocol_msg(msg);
        return 1;
    }

    if (!msg->payload) {
        fprintf(stderr, "[ERROR] [protocolhandler/protocol_handle_command] Received command frame does not include a payload\n");
        // Send error back
        delete_protocol_msg(msg);
        return 1;
    }
    // Push received command to specified client's command queue
    queueNode* node = queue_createNode(msg->payload);

    if (!node) {
        fprintf(stderr, "[ERROR] [protocolhandler/protocol_handle_command] queue_createNode error\n");
        delete_protocol_msg(msg);
        return 1;
    }
    if (queue_push(specifiedClient->command_queue, node) != 0) {
        fprintf(stderr, "[ERROR] [protocolhandler/protocol_handle_command] queue_push error\n");
        delete_protocol_msg(msg);
        return 1;
    }

    delete_protocol_msg(msg);
    return 0;
}

int protocol_handle_listupdate() {
    if (websocket_send_connectionsList(websocket_global_wss, clientHash) != 0)
        return 1;
    return 0;
}

char* create_error_jsonMsg(const char* error_msg) {
    size_t jsonSize = strlen(error_msg) + 50; // Extra space for JSON structure
    char* jsonError = malloc(jsonSize);
    if (!jsonError) {
        fprintf(stderr, "[ERROR] [protocolhandler/create_error_jsonMsg] Failed to allocate memory for jsonError\n");
        return NULL;
    }
    snprintf(jsonError, jsonSize, "{\"type\": \"ERROR\", \"payload\": \"%s\"}", error_msg);
    return jsonError;
}
PROTOCOL_MESSAGE* protocol_create_msg(enum PROTOCOL_MESSAGE_TYPES type,
                                      enum PROTOCOl_CONTENT_TYPE content_type,
                                      char* dest, char* src,
                                      char* clientID, char* payload,
                                      int clientID_size, int payload_size)
{
    PROTOCOL_MESSAGE* msg = malloc(sizeof(PROTOCOL_MESSAGE));

    if (!msg) {
        fprintf(stderr, "[ERROR] [protocolhandler/protocol_create_msg] Failed to allocate memory for PROTOCOL_MESSAGE* msg");
    }

    msg->payload = malloc(payload_size + 1);
    if (!msg->payload) {
        fprintf(stderr, "[ERROR] [protocolhandler/protocol_create_msg] Failed to allocate memory for payload\n");
        free(msg);
        return NULL;
    }
    msg->specifiedClient_id = malloc(clientID_size + 1);
    if (!msg->specifiedClient_id) {
        fprintf(stderr, "[ERROR] [protocolhandler/protocol_create_msg] Failed to allocate memory for specifiedClient_id\n");
        free(msg->payload);
        free(msg->specifiedClient_id);
        free(msg);
        return NULL;
    }

    msg->destination = malloc(strlen(dest) + 1);
    if (!msg->destination) {
        fprintf(stderr, "[ERROR] [protocolhandler/protocol_create_msg] Failed to allocate memory for msg->destination\n");
        free(msg->payload);
        free(msg);
        return NULL;
    }
    msg->source = malloc(strlen(src) + 1);
    if (!msg->source) {
        fprintf(stderr, "[ERROR] [protocolhandler/protocol_create_msg] Failed to allocate memory for msg->source\n");
        free(msg->payload);
        free(msg->specifiedClient_id);
        free(msg->destination);
        free(msg);
        return NULL;
    }
    msg->msg_type = type;
    msg->content_type = content_type;
    msg->clientID_size = clientID_size;
    msg->source = src;
    msg->payload_size = payload_size;
    snprintf(msg->source, strlen(src) + 1, "%s", src);
    snprintf(msg->destination, strlen(dest) + 1, "%s", dest);
    snprintf(msg->payload, payload_size + 1, "%s", payload);
    snprintf(msg->specifiedClient_id, clientID_size + 1, "%s", clientID);

    return msg;
}

char* protocol_create_jsonMsg(PROTOCOL_MESSAGE* msg) {
    cJSON* json = cJSON_CreateObject();
    if (!json) {
        fprintf(stderr, "[ERROR] [protocolhandler/protocol_create_jsonMsg] Failed to create cJSON object\n");
        return NULL;
    }

    cJSON_AddStringToObject(json, "type", msgTypes[msg->msg_type - 1].str);
    cJSON_AddStringToObject(json, "content", contentTypes[msg->content_type - 1].str);
    if (msg->destination) cJSON_AddStringToObject(json, "destination", msg->destination);
    if (msg->source) cJSON_AddStringToObject(json, "source", msg->source);
    if (msg->specifiedClient_id) cJSON_AddStringToObject(json, "selectedClient", msg->specifiedClient_id);
    if (msg->payload) cJSON_AddStringToObject(json, "payload", msg->payload);
    cJSON_AddNumberToObject(json, "payload_size", msg->payload_size);
    cJSON_AddNumberToObject(json, "client_size", msg->clientID_size);

    char* jsonStr = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    if (!jsonStr) {
        fprintf(stderr, "[ERROR] [protocolhandler/protocol_create_jsonMsg] Failed to print cJSON to string\n");
        return NULL;
    }
    return jsonStr; // Caller must free
}

int protocol_send_error(websocket_service* wss, char* error_msg) {
    char * jsonError = create_error_jsonMsg(error_msg);
    if (!jsonError) {
        return 1;
    }

    int result = websocket_send(wss, jsonError);
    free(jsonError);
    return result;
} // To be made after refactoring server.c, so we can send_raw_message() in here by including the new header for refactored websocket functions

/*
 *
 * NOTES:
 *
 *  websocket callback func only runs handle_received_message()
 *  web thread will continue to read from the global response queue (has RESPONSE messages with command output in them from handle_client, meaning handle_client will need a
 *
 *
 */
