#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define SERVER_IP "10.61.1.118"

int main() {
    int sockfd;
    struct sockaddr_in serv_addr;
    char name[32];
    char buffer[BUFFER_SIZE];

    printf("Enter your name: ");
    fgets(name, sizeof(name), stdin);
    name[strcspn(name, "\n")] = 0;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr);

    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("Connection Failed!\n");
        return -1;
    }

    send(sockfd, name, strlen(name), 0);
    printf("=== Connected to Chat Server ===\n");

    struct pollfd fds[2];

    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;

    fds[1].fd = sockfd;
    fds[1].events = POLLIN;

    printf("%s : ", name);
    fflush(stdout);

    while (1) {
        int poll_count = poll(fds, 2, -1);

        if (poll_count < 0) {
            perror("Poll error");
            break;
        }

        if (fds[1].revents & POLLIN) {
            char message[BUFFER_SIZE];
            int receive = recv(sockfd, message, sizeof(message) - 1, 0);

            if (receive > 0) {
                message[receive] = '\0';
                printf("\r\33[2K%s", message);
                printf("%s : ", name);
                fflush(stdout);
            } else if (receive == 0) {
                printf("\nDisconnected from server.\n");
                break;
            } else {
                break;
            }
        }

        if (fds[0].revents & POLLIN) {
            if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
                if (strlen(buffer) > 0 && strcmp(buffer, "\n") != 0) {
                    send(sockfd, buffer, strlen(buffer), 0);
                }
                printf("%s : ", name);
                fflush(stdout);
            }
        }
    }

    close(sockfd);
    return 0;
}