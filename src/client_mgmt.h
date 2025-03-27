#ifndef CLIENT_MGMT_H_INCLUDED
#define CLIENT_MGMT_H_INCLUDED


#ifndef CLIENT_MGMT // Include guard
#define CLIENT_MGMT

#include "common.h"

#define HASH_SIZE 100

extern pthread_mutex_t hash_mutex;

extern pthread_mutex_t queue_mutex;

extern pthread_cond_t queue_cond;


void init_mutexes();

void destroy_mutexes();

/* * * * * * * * * * * * * * * * * */

typedef struct queueNode {
    char* bffr;
    struct queueNode* next;
} queueNode;

typedef struct queue {
    queueNode* head;
    queueNode* tail;
    int size;
} Queue;

/* * * * * * * * * * * * * * * * * */

typedef struct client {
    int socket_desc;
    char* ip;
    char* id; // eg: cli1, cli2
    Queue* command_queue;
    #ifdef _WIN32
    HANDLE thread;
    #else
    pthread_t thread;
    #endif
} client;

client* createClient(int socket_desc, char* ip, int* currClient_ID);

typedef struct Node {
    struct Node* next;
    client* client;
} Node;

typedef struct hashMap {
    int size;
    Node** buckets;
} hashMap;

extern hashMap* clientHash;

hashMap* hash_init(int size);

unsigned int hash_func(char* id, int mapsize);

Node* hash_createNode(client* c);

void linkedList_add(Node* array, Node* node);

int hash_put(hashMap* hash, client* client);

int hash_remove(hashMap* hash, char* id);

client* hash_grab(hashMap* hash, char* id);

void hash_destroy(hashMap* hash);


/* * * * * * * * * * * * * * * * * */


void queue_init(Queue* q);

queueNode* queue_createNode(char* buffer);

int queue_isEmpty(Queue* q);

int queue_push(Queue* q, queueNode* qN);

char* queue_pop(Queue* q);

void queue_destroy(Queue* q);

// Any other common definitions, structs, or function prototypes can go here

#endif

#endif // CLIENT_MGMT_H_INCLUDED
