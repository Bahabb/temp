#include "common.h"
#include "client_mgmt.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>

// Define the global mutexes and condition variable
pthread_mutex_t hash_mutex;
pthread_mutex_t queue_mutex;
pthread_cond_t queue_cond;

hashMap* clientHash;

void init_mutexes() {
    if (pthread_mutex_init(&hash_mutex, NULL) != 0) {
        perror("[ERROR] hash_mutex init failed\n");
        exit(1);
    }
    if (pthread_mutex_init(&queue_mutex, NULL) != 0) {
        perror("[ERROR] queue_mutex init failed\n");
        exit(1);
    }
    if (pthread_cond_init(&queue_cond, NULL) != 0) {
        perror("[ERROR] queue_cond init failed\n");
        exit(1);
    }
}

void destroy_mutexes() {
    pthread_mutex_destroy(&hash_mutex);
    pthread_mutex_destroy(&queue_mutex);
    pthread_cond_destroy(&queue_cond);
}



hashMap* hash_init(int size) {
    hashMap* map = malloc(sizeof(hashMap));
    if (map == NULL) return NULL;

    pthread_mutex_lock(&hash_mutex);
    map->size = size;
    map->buckets = malloc(size * sizeof(Node*));
    if (map->buckets == NULL) {
        pthread_mutex_unlock(&hash_mutex);
        free(map);
        return NULL;
    }

    for (int i = 0; i < size; i++) map->buckets[i] = NULL;
    pthread_mutex_unlock(&hash_mutex);
    return map;
}

int id_isValid(char* id) {
    char enforced[3] = "cli";
    if (strlen(id) < 4) return 0;

    for (int i = 0; i < 3; i++) {
        if (id[i] != enforced[i]) return 0;
    }
    return 1;
}

unsigned int hash_func(char* id, int mapsize) {
    unsigned int key = 'c' + 'l' + 'i';
    if (!id_isValid(id)) return UINT_MAX; // Use UINT_MAX instead of -1 for unsigned
    for (int i = 3; id[i] != '\0'; i++) key += id[i];
    key = key % mapsize;
    return key;
}

Node* hash_createNode(client* c) {
    Node* node = (Node*)malloc(sizeof(struct Node));
    if (node == NULL) return NULL;
    node->next = NULL;
    node->client = c;
    return node;
}

void linkedList_add(Node* array, Node* node) {
    if (array == NULL) {
        printf("[ERROR] linkedList_add: Node* array (bucket) is NULL, failure at mem allocation\n");
        return;
    }
    Node* p = array;

    while (p->next != NULL) p = p->next;

    if (!p) {
        printf("[ERROR] linkedList_add : Unexpected: p is NULL after traversal\n");
        return;
    }

    p->next = node;
}

int hash_put(hashMap* hash, client* client) {
    unsigned int key = hash_func(client->id, hash->size);
    if (key == UINT_MAX) return -1;

    Node* newNode = hash_createNode(client);
    if (newNode == NULL) {
        fprintf(stderr, "[ERROR] hash_put : Failed to create  new hash node\n");
        return -1;
    }
    pthread_mutex_lock(&hash_mutex);
    if (hash->buckets[key] == NULL) {
        hash->buckets[key] = newNode; // Initialize the bucket with the first node
    } else {
        linkedList_add(hash->buckets[key], newNode); // Append to existing list
    }
    printf("hash_put : Added %s to clientHash at bucket %u\n", client->id, key);
    pthread_mutex_unlock(&hash_mutex);
    return 1;
}

void delete_client(client* cli) {
    queue_destroy(cli->command_queue);
    cli->command_queue = NULL;
    free(cli->id);
    cli->id = NULL;
    free(cli->ip);
    cli->ip = NULL;
    free(cli);
}


int hash_remove(hashMap* hash, char* id) {
    unsigned int key = hash_func(id, hash->size);
    if (key == UINT_MAX) return -1;
    pthread_mutex_lock(&hash_mutex);
    Node* prev = NULL;
    Node* p = hash->buckets[key];
    for (; p && strcmp(p->client->id, id); prev = p, p = p->next);
    if (!p) {
        pthread_mutex_unlock(&hash_mutex);
        return -1;
    }
    if (prev) prev->next = p->next;
    else hash->buckets[key] = p->next;
    delete_client(p->client);
    free(p);
    pthread_mutex_unlock(&hash_mutex);
    return 0;
}


client* hash_grab(hashMap* hash, char* id) {
    int key = hash_func(id, HASH_SIZE);
    pthread_mutex_lock(&hash_mutex);
    Node* n = hash->buckets[key];
    while (n != NULL) {
        if (strcmp(n->client->id, id) == 0) {
            client* cl = n->client;
            pthread_mutex_unlock(&hash_mutex);
            return cl;
        }
        n = n->next;
    }
    pthread_mutex_unlock(&hash_mutex);
    return NULL; // Client not found
}

void hash_destroy(hashMap* hash) {
    pthread_mutex_lock(&hash_mutex);
    for (int i = 0; i < hash->size; i++) {
        Node* listHead = hash->buckets[i];
        Node* p = NULL;
        while(listHead != NULL) {
            free(listHead->client->id);
            free(listHead->client->ip);
            free(listHead->client);
            p = listHead;
            listHead = listHead->next;
            free(p);
            p = NULL; // against dangling pointer
        }
    }
    free(hash->buckets);
    pthread_mutex_unlock(&hash_mutex);
}


void queue_init(Queue* q) {
    pthread_mutex_lock(&queue_mutex);
    q->head = NULL;
    q->tail = NULL;
    q->size = 0;
    pthread_mutex_unlock(&queue_mutex);
}

queueNode* queue_createNode(char* output) {
    queueNode* node = malloc(sizeof(queueNode));
    if (!node) {
        printf("[ERROR] queue_createNode malloc failure\n");
        return NULL;
    }
    node->next = NULL;
    node->bffr = malloc(strlen(output) + 1);
    if (!node->bffr) {
        printf("[ERROR] queue_createNode : new_queueNode-->output malloc error\n");
        node->bffr = NULL;
        free(node);
        return NULL;
    }
    strcpy(node->bffr, output);
    return node;
}

int queue_isEmpty(Queue* q) {
    pthread_mutex_lock(&queue_mutex);
    int b = !(q->size);
    pthread_mutex_unlock(&queue_mutex);
    return b;
}

int queue_push(Queue* q, queueNode* qN) {
    if (qN == NULL) {
        printf("[ERROR] queue_push : Unable to push queueNode holding your output to queue. queueNode == NULL\n");
        return 1;
    }
    int succ = 0;
    pthread_mutex_lock(&queue_mutex);
    if (q->size != 0 && q->tail != NULL) {
        q->tail->next = qN;
        q->tail = qN;
        succ++;
    } else if (q->size == 0) {
        q->head = qN;
        q->tail = qN;
        succ++;
    } else {
        printf("[ERROR] queue_push : Unable to push your queueNode. Current queue-->tail is NULL & queue is NOT empty; error\n");
    }
    succ ? printf("Pushed to queue: %s, new queue size: %d\n", qN->bffr, ++q->size) : printf("Failed to push queue");
    pthread_cond_signal(&queue_cond);
    pthread_mutex_unlock(&queue_mutex);
    return 0;
}

char* queue_pop(Queue* q) {
    pthread_mutex_lock(&queue_mutex);
    struct timeval tv;
    if (gettimeofday(&tv, NULL) != 0) {
        perror("gettimeofday failed");
        pthread_mutex_unlock(&queue_mutex);
        return NULL;
    }

    struct timespec ts;
    ts.tv_sec = tv.tv_sec;
    long long nsec = (long long)tv.tv_usec * 1000 + 5000000; // 5ms in nanoseconds
    if (nsec < 0 || tv.tv_usec < 0 || tv.tv_usec >= 1000000) {
        fprintf(stderr, "Invalid tv.tv_usec: %ld\n", tv.tv_usec);
        nsec = 5000000; // Fallback to 5ms
        ts.tv_sec = time(NULL); // Fallback to current time
    }
    ts.tv_nsec = nsec % 1000000000;
    ts.tv_sec += nsec / 1000000000;

    //printf("queue_pop : ts.tv_sec=%ld, ts.tv_nsec=%ld\n", ts.tv_sec, ts.tv_nsec); // Debug

    while (q->head == NULL) {
        int result = pthread_cond_timedwait(&queue_cond, &queue_mutex, &ts);
        if (result == ETIMEDOUT) {
            pthread_mutex_unlock(&queue_mutex);
            return NULL; // Timeout, no message
        }
        if (result != 0) {
            fprintf(stderr, "[ERROR] pthread_cond_timedwait failed: %d\n", result);
            pthread_mutex_unlock(&queue_mutex);
            return NULL;
        }
    }

    char* output = malloc(strlen(q->head->bffr) + 1);
    if (!output) {
        perror("[ERROR] malloc output in queue_pop failed\n");
        pthread_mutex_unlock(&queue_mutex);
        return NULL;
    }
    strcpy(output, q->head->bffr);
    queueNode* oldhead = q->head;
    q->head = q->head->next;
    if (q->head == NULL) q->tail = NULL;
    q->size--;
    free(oldhead->bffr);
    free(oldhead);
    pthread_mutex_unlock(&queue_mutex);
    return output;
}

void queue_destroy(Queue* q) {
    char* output;
    queueNode* p = q->head;
    queueNode* p2 = q->head;
    for(int i = 0; i < q->size; i++) {
        output = queue_pop(q);
        free(output);
        p = p2;
        p2 = p->next;
        free(p);
    }
    free(q);
}



client* createClient(int socket_desc, char* ip, int* currClient_ID) {

    client* newClient = malloc(sizeof(client));
    if (!newClient) {
        fprintf(stderr, "[ERROR] [client_mgmt/createClient] Error allocating memory for client struct\n");
        free(ip);
        close(socket_desc);
        #ifdef _WIN32
        WSACleanup();
        #endif
        return NULL;
    }
    newClient->ip = ip;
    newClient->socket_desc = socket_desc;

    char* id = malloc(10);
    if (!id) {
        fprintf(stderr, "[ERROR] [client_mgmt/createClient] Error allocating memory for client ID\n");
        free(newClient);
        free(ip);
        close(socket_desc);
        #ifdef _WIN32
        WSACleanup();
        #endif
        return NULL;
    }
    strncpy(id, "cli", 3);
    snprintf(id + 3, 7, "%d", ++(*currClient_ID));
    newClient->id = id;

    Queue* cmd_queue = malloc(sizeof(Queue));
    if (!cmd_queue) {
        fprintf(stderr, "[ERROR] [client_mgmt/createClient] Error allocating memory for cmd_queue\n");
    }
    queue_init(cmd_queue);

    newClient->command_queue = cmd_queue;

    return newClient;
}
