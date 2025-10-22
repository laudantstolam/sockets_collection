#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
// ------------------------------[INIT]--------------------

#define PORT 8888
#define MAX_CLIENTS 5
#define BUFFER_SIZE 1024

typedef enum {
    ROOM_NONE = 0,
    ROOM_A,
    ROOM_B,
    ROOM_C
} Room;

typedef struct {
    int socket;
    int id;
    Room room;
    struct sockaddr_in address;
    int active;
} Client;

// ------------------------------[END INIT]--------------------


Client clients[MAX_CLIENTS];
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
int client_count = 0;

const char* room_name(Room room) {
    switch(room) {
        case ROOM_A: return "RoomA";
        case ROOM_B: return "RoomB";
        case ROOM_C: return "RoomC";
        default: return "None";
    }
}

void broadcast_to_room(Room room, const char* message, int sender_id) {
    pthread_mutex_lock(&clients_mutex);
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(clients[i].active && clients[i].room == room && clients[i].id != sender_id) {
            send(clients[i].socket, message, strlen(message), 0);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void broadcast_to_all(const char* message) {
    pthread_mutex_lock(&clients_mutex);
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(clients[i].active) {
            send(clients[i].socket, message, strlen(message), 0);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void* handle_client(void* arg) {
    int client_index = *(int*)arg;
    free(arg);
    
    int client_sock = clients[client_index].socket;
    int client_id = clients[client_index].id;
    char buffer[BUFFER_SIZE];
    char message[BUFFER_SIZE];
    
    printf("Client %d connected\n", client_id);
    
    // Ask client to choose a room
    const char* welcome = "Welcome! Choose a room (A/B/C): ";
    send(client_sock, welcome, strlen(welcome), 0);
    
    while(1) {
        memset(buffer, 0, BUFFER_SIZE);
        int read_size = recv(client_sock, buffer, BUFFER_SIZE - 1, 0);
        
        if(read_size <= 0) {
            break;
        }
        
        buffer[read_size] = '\0';
        
        // Remove newline if present
        char* newline = strchr(buffer, '\n');
        if(newline) *newline = '\0';
        
        // Check for EXIT command
        if(strcmp(buffer, "EXIT!") == 0) {
            printf("Client %d requested exit\n", client_id);
            break;
        }
        
        // Check for room selection/change
        if(strlen(buffer) == 1 && (buffer[0] == 'A' || buffer[0] == 'B' || buffer[0] == 'C')) {
            Room old_room = clients[client_index].room;
            Room new_room = ROOM_NONE;
            
            if(buffer[0] == 'A') new_room = ROOM_A;
            else if(buffer[0] == 'B') new_room = ROOM_B;
            else if(buffer[0] == 'C') new_room = ROOM_C;
            
            pthread_mutex_lock(&clients_mutex);
            clients[client_index].room = new_room;
            pthread_mutex_unlock(&clients_mutex);
            
            if(old_room != ROOM_NONE) {
                snprintf(message, BUFFER_SIZE, "[Server] Client %d left %s\n", 
                         client_id, room_name(old_room));
                broadcast_to_room(old_room, message, client_id);
            }
            
            snprintf(message, BUFFER_SIZE, "[Server] You joined %s\n", room_name(new_room));
            send(client_sock, message, strlen(message), 0);
            
            snprintf(message, BUFFER_SIZE, "[Server] Client %d joined %s\n", 
                     client_id, room_name(new_room));
            broadcast_to_room(new_room, message, client_id);
            
            printf("Client %d joined %s\n", client_id, room_name(new_room));
        }
        else {
            // Regular message - broadcast to room
            if(clients[client_index].room != ROOM_NONE) {
                // Limit buffer to prevent truncation warning
                buffer[BUFFER_SIZE - 20] = '\0';
                snprintf(message, BUFFER_SIZE, "[Client %d]: %s\n", client_id, buffer);
                broadcast_to_room(clients[client_index].room, message, client_id);
                printf("Client %d in %s: %s\n", client_id, 
                       room_name(clients[client_index].room), buffer);
            }
            else {
                const char* msg = "[Server] Please join a room first (A/B/C)\n";
                send(client_sock, msg, strlen(msg), 0);
            }
        }
    }
    
    // Client disconnected
    pthread_mutex_lock(&clients_mutex);
    if(clients[client_index].room != ROOM_NONE) {
        snprintf(message, BUFFER_SIZE, "[Server] Client %d left %s\n", 
                 client_id, room_name(clients[client_index].room));
        broadcast_to_room(clients[client_index].room, message, client_id);
    }
    clients[client_index].active = 0;
    client_count--;
    pthread_mutex_unlock(&clients_mutex);
    
    printf("Client %d disconnected. Active clients: %d\n", client_id, client_count);
    
    close(client_sock);
    return NULL;
}

// ------------------------------[MAIN]--------------------

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int next_id = 1;
    
    // init clients array
    memset(clients, 0, sizeof(clients));
    
    // Create socket
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if(server_sock < 0) {
        perror("Socket creation failed");
        exit(1);
    }
    
    // Set socket options
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Configure server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);
    
    // Bind socket
    if(bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_sock);
        exit(1);
    }
    
    // Listen
    if(listen(server_sock, 10) < 0) {
        perror("Listen failed");
        close(server_sock);
        exit(1);
    }
    
    printf("TCP Echo Server started on port %d\n", PORT);
    printf("Waiting for connections...\n");

    // ------------[Start Listening]-----------------
    while(1) {
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_len);
        if(client_sock < 0) {
            perror("Accept failed");
            continue;
        }
        
        pthread_mutex_lock(&clients_mutex);
        
        if(client_count >= MAX_CLIENTS) {
            const char* msg = "Server is full!\n";
            send(client_sock, msg, strlen(msg), 0);
            close(client_sock);
            printf("Connection rejected - server full\n");
            pthread_mutex_unlock(&clients_mutex);
            continue;
        }
        
        // Find empty slot
        int slot = -1;
        for(int i = 0; i < MAX_CLIENTS; i++) {
            if(!clients[i].active) {
                slot = i;
                break;
            }
        }
        
        if(slot >= 0) {
            clients[slot].socket = client_sock;
            clients[slot].id = next_id++;
            clients[slot].room = ROOM_NONE;
            clients[slot].address = client_addr;
            clients[slot].active = 1;
            client_count++;
            
            int* arg = malloc(sizeof(int));
            *arg = slot;
            
            pthread_t thread;
            pthread_create(&thread, NULL, handle_client, arg);
            pthread_detach(thread);
        }
        
        pthread_mutex_unlock(&clients_mutex);
    }
    
    close(server_sock);
    return 0;
}