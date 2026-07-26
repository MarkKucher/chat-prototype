#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>

#define MAX_CLIENTS 100
#define MAX_MESSAGE 512
#define MAX_WORD 21

typedef struct {
    struct sockaddr_in addr;
    char username[MAX_WORD];
    int active;
} client_t;

client_t clients[MAX_CLIENTS];

void trim_newline(char *str) {
    size_t len = strlen(str);
    if(len > 0 && str[len - 1] == '\n') {
        str[len - 1] = '\0';
    }
}

int contains_whitespace(const char *str) {
    while(*str) {
        if (isspace(*str++)) return 1;
    }
    return 0;
}

int is_username_taken(const char *name) {
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(clients[i].active && strcmp(clients[i].username, name) == 0) {
            return 1;
        }
    }
    return 0;
}

// Whether addresses refer to the same client or not
int same_client(struct sockaddr_in *a, struct sockaddr_in *b) {
    return (a->sin_addr.s_addr == b->sin_addr.s_addr && a->sin_port == b->sin_port);
}

// Gets client's index in array by its address
int client_index(struct sockaddr_in *addr) {
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(clients[i].active && same_client(&clients[i].addr, addr)) {
            return i;
        }
    }
    return -1;
}

// Save client's info if there is still any place left
int register_client(struct sockaddr_in *addr, const char *username) {
    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(!clients[i].active) {
            clients[i].addr = *addr;
            strncpy(clients[i].username, username, MAX_WORD);
            clients[i].active = 1;
            return i;
        }
    }

    return -1; //server is full
}

// Broadcast message to all clients who previously reached out
void send_to_all(int sender_index, const char *msg, int server_fd) {
    char buffer[MAX_MESSAGE + MAX_WORD + 4];
    snprintf(buffer, sizeof(buffer), "%s: %s", clients[sender_index].username, msg);

    for(int i = 0; i < MAX_CLIENTS; i++) {
        if(clients[i].active && i != sender_index) {
            sendto(server_fd, buffer, strlen(buffer), 0, (struct sockaddr*)&clients[i].addr, sizeof(clients[i].addr));
        } else {
            char * message = "(sent)\n";
            sendto(server_fd, message, strlen(message), 0, (struct sockaddr*)&clients[i].addr, sizeof(clients[i].addr));
        }
    }
}

int main(int argc, char *argv[]) {
    if(argc < 2) {
        printf("Invalid arguments\n");
        exit(0);
    }

    int server_fd, port = atoi(argv[1]);
    struct sockaddr_in server_addr, client_addr;

    socklen_t addr_len = sizeof(client_addr);

    memset(&server_addr, 0, sizeof(server_addr));
    memset(clients, 0, sizeof(clients));

    // Initializing server
    server_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(server_fd < 0) {
        perror("socket");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if(bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        exit(2);
    }   

    printf("Server listening on port %d\n", port);

    // Listening to clients
    while(1) {
        char buffer[MAX_MESSAGE];
        int r = recvfrom(server_fd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&client_addr, &addr_len);

        if(r <= 0) {
            continue;
        }
        
        buffer[r] = '\0';

        // Only message about disconnection can start with \n otherwise they're ignored in client side
        if(buffer[0] == '\n') {
            int i = client_index(&client_addr);
            clients[i].active = 0;
            continue;
        }

        int i = client_index(&client_addr);
        char * message;

        // User is not registered
        if(i < 0) {
            trim_newline(buffer);
            if(strncmp(buffer, "/register ", 10) == 0) {
                char * username = buffer + 10;
                if(strlen(username) < 1 || strlen(username) >= MAX_WORD || contains_whitespace(username)) {
                    int limit = MAX_WORD - 1;
                    char error[MAX_MESSAGE];
                    snprintf(error, sizeof(error), "Username can't contain any whitespaces and has to be 1 to %d characters long\n", limit);
                    message = error;
                } else if(is_username_taken(username)) {
                    message = "This username is already taken, try another one\n";
                } else {
                    if(register_client(&client_addr, username) < 0) {
                        message = "Server is full\n";
                    } else {
                        message = "You are now connected to the chat\n";
                    }
                }
            } else {
                message = "Please register using: \"/register <username>\"\n";
            }
            sendto(server_fd, message, strlen(message), 0, (struct sockaddr*)&client_addr, addr_len);
        } else {
            send_to_all(i, buffer, server_fd);
        }
    }

    close(server_fd);
    return 0;
}
