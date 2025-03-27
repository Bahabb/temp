#ifndef PROTOCOLHANDLER_H_INCLUDED
#define PROTOCOLHANDLER_H_INCLUDED


#ifndef PROTOCOLHANDLER // Include guard
#define PROTOCOLHANDLER


#include "common.h"
#include "websocket.h"


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


void delete_protocol_msg(PROTOCOL_MESSAGE* msg);

PROTOCOL_MESSAGE* parse_message(char* jsonString);

void handle_received_message(char* received_jsonMsg);

int protocol_handle_command(PROTOCOL_MESSAGE* msg);

int create_error_msg(char* error_msg);

PROTOCOL_MESSAGE* protocol_create_msg(enum PROTOCOL_MESSAGE_TYPES type,
                                      enum PROTOCOl_CONTENT_TYPE content_type,
                                      char* dest, char* src,
                                      char* clientID, char* payload,
                                      int clientID_size, int payload_size);

char* protocol_create_jsonMsg(PROTOCOL_MESSAGE* msg);

int protocol_send_error(websocket_service* wss, char* error_msg);


#endif

#endif // PROTOCOLHANDLER_H_INCLUDED
