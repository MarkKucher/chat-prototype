#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <termios.h>
#include <signal.h>

#define MAX_MESSAGE 512
#define MAX_WORD 21

int active = 0;
int sockfd = -1;
struct sockaddr_in server_addr;
socklen_t addr_len = sizeof(server_addr);

char buffer[MAX_MESSAGE];

struct termios orig_termios;

char input_buf[MAX_MESSAGE];
int input_len = 0;

void trim_newline(char *str) {
    size_t len = strlen(str);
    if(len > 0 && str[len - 1] == '\n') {
        str[len - 1] = '\0';
    }
}

void disable_custom_mode() {
    tcsetattr(0, TCSAFLUSH, &orig_termios);
}

// Allows to intercept each character typed in terminal and turns off automatic echo
void enable_custom_mode() {
    tcgetattr(0, &orig_termios);
    atexit(disable_custom_mode);

    struct termios raw;
    tcgetattr(STDIN_FILENO, &raw);
    raw.c_lflag &= ~ICANON;
    raw.c_lflag &= ~ECHO;

    tcsetattr(0, TCSAFLUSH, &raw);
}

void disconnect(int sig) {
    if(sockfd >= 0) {
        buffer[0] = '\n';
        buffer[1] = '\0';
        sendto(sockfd, buffer, strlen(buffer), 0, (struct sockaddr*)&server_addr, addr_len);
    }
    close(sockfd);
    printf("\n");
}

int main(int argc, char *argv[]) {
    if(argc != 3) {
        printf("Invalid arguments!");
        exit(1);
    }
    fd_set readfds;

    // Create socket
    if((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket");
        exit(1);
    }

    // Send server a message to free username
    signal(SIGINT, disconnect);
    printf("Use: \"/register <username>\" to join chat\n\n");

    // Set up server address
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[2]));
    if(inet_aton(argv[1], &server_addr.sin_addr) == 0) {
        perror("inet_aton");
        exit(1);
    }

    enable_custom_mode();

    // Listening to server and input from terminal
    while(1) {
        FD_ZERO(&readfds);
        FD_SET(0, &readfds); // stdin
        FD_SET(sockfd, &readfds); // socket

        if(select(sockfd + 1, &readfds, NULL, NULL, NULL) < 0) {
            break;
        }

        // User input
        if(FD_ISSET(0, &readfds)) {
            char c;
            if(read(0, &c, 1) <= 0) {
                break;
            }

            if(c == '\n') { // Send message to server
                if(input_len == 0) {
                    continue;
                }
                printf("\n");
                if(!active) {
                    printf("\n");
                }
                input_buf[input_len++] = '\n';
                input_buf[input_len] = '\0';
                sendto(sockfd, input_buf, input_len, 0, (struct sockaddr *)&server_addr, addr_len);
                input_buf[0] = '\0';
                input_len = 0;
            } else if(c == 127) { // Erase one character
                if(input_len != 0) {
                    input_buf[--input_len] = '\0';
                    printf("\b \b");
                }
            } else if(input_len < MAX_MESSAGE - 2 && isprint(c)) { // Save one character
                printf("%c", c);
                input_buf[input_len++] = c;
                input_buf[input_len] = '\0';
            }
            fflush(stdout); // Because we do not necessarily have \n at the end
        }

        // Incoming message
        if(FD_ISSET(sockfd, &readfds)) {
            int n = recvfrom(sockfd, buffer, sizeof(buffer) - 1, 0, NULL, NULL);
            if(n <= 0) {
                perror("recvfrom");
                break;
            }
            buffer[n] = '\0';
            char * success = "You are now connected to the chat\n";
            if(strcmp(buffer, success) == 0) {
                active = 1;
            }
            // Move cursor to the beginning of line, print message, then reprint input
            printf("\r\033[K%s\n%s", buffer, input_buf); // \r sets cursor at the beginning of the line, \033[K clears the line
            fflush(stdout);
        }
    }

    close(sockfd);
    return 0;
}
