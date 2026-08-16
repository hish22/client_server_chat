#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>

#define PORT 8080
#define MAX_CLIENTS 100
#define BUFFER_SIZE 1024
#define MAX_HISTORY 50

typedef struct {
    int fd;
    char name[32];
} client_t;

typedef struct {
    char text[BUFFER_SIZE + 35];
} message_t;

client_t clients[MAX_CLIENTS];
struct pollfd fds[MAX_CLIENTS + 1];
int nfds = 1;

message_t msg_history[MAX_HISTORY];
int history_count = 0;

void broadcast_message(char *message, int sender_fd) {
    for (int i = 1; i < nfds; i++) {
        if (fds[i].fd != sender_fd && fds[i].fd != -1) {
            send(fds[i].fd, message, strlen(message), 0);
        }
    }
}

void broadcast_user_list() {
    char list_msg[BUFFER_SIZE] = "\n--- Active Users ---\n";
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd != -1) {
            strcat(list_msg, "- ");
            strcat(list_msg, clients[i].name);
            strcat(list_msg, "\n");
        }
    }
    strcat(list_msg, "--------------------\n");
    broadcast_message(list_msg, -1);
}

void remove_client(int index) {
    int target_fd = fds[index].fd;
    close(target_fd);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd == target_fd) {
            clients[i].fd = -1;
            memset(clients[i].name, 0, sizeof(clients[i].name));
            break;
        }
    }

    for (int i = index; i < nfds - 1; i++) {
        fds[i] = fds[i + 1];
    }

    nfds--;
}

void save_message(char *msg) {
    if (history_count < MAX_HISTORY) {
        strcpy(msg_history[history_count].text, msg);
        history_count++;
    } else {
        for (int i = 0; i < MAX_HISTORY - 1; i++) {
            msg_history[i] = msg_history[i + 1];
        }
        strcpy(msg_history[MAX_HISTORY - 1].text, msg);
    }
}

void send_history(int client_fd) {
    if (history_count == 0) return;

    char header[] = "\n--- Chat History ---\n";
    send(client_fd, header, strlen(header), 0);

    for (int i = 0; i < history_count; i++) {
        send(client_fd, msg_history[i].text, strlen(msg_history[i].text), 0);
    }

    char footer[] = "--------------------\n\n";
    send(client_fd, footer, strlen(footer), 0);
}

int main() {
    int listenfd, connfd;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE];

    for (int i = 0; i < MAX_CLIENTS; i++) clients[i].fd = -1;

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(PORT);

    bind(listenfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    listen(listenfd, 10);

    fds[0].fd = listenfd;
    fds[0].events = POLLIN;

    printf("=== Poll-based Chat Server Started on Port %d ===\n", PORT);

    while (1) {
        int poll_count = poll(fds, nfds, -1);

        if (poll_count < 0) {
            perror("Poll error");
            break;
        }

        if (fds[0].revents & POLLIN) {
            connfd = accept(listenfd, NULL, NULL);

            if (nfds < MAX_CLIENTS + 1) {
                fds[nfds].fd = connfd;
                fds[nfds].events = POLLIN;
                nfds++;

                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i].fd == -1) {
                        clients[i].fd = connfd;
                        break;
                    }
                }
            } else {
                printf("Server full! Rejected connection.\n");
                close(connfd);
            }
        }

        for (int i = 1; i < nfds; i++) {
            if (fds[i].revents & POLLIN) {
                int bytes_read = recv(fds[i].fd, buffer, sizeof(buffer) - 1, 0);

                int client_idx = -1;
                for (int c = 0; c < MAX_CLIENTS; c++) {
                    if (clients[c].fd == fds[i].fd) {
                        client_idx = c;
                        break;
                    }
                }

                if (bytes_read > 0) {
                    buffer[bytes_read] = '\0';
                    buffer[strcspn(buffer, "\r\n")] = 0;

                    if (strlen(clients[client_idx].name) == 0) {
                        strcpy(clients[client_idx].name, buffer);

                        send_history(fds[i].fd);

                        char join_msg[BUFFER_SIZE];
                        snprintf(join_msg, sizeof(join_msg), "%s joined the chat.\n", clients[client_idx].name);
                        printf("%s", join_msg);
                        broadcast_message(join_msg, fds[i].fd);
                        broadcast_user_list();
                    } else {
                        char msg_out[BUFFER_SIZE + 35];
                        snprintf(msg_out, sizeof(msg_out), "%s: %s\n", clients[client_idx].name, buffer);
                        
                        save_message(msg_out);
                        broadcast_message(msg_out, fds[i].fd);
                    }
                } else {
                    char leave_msg[BUFFER_SIZE];
                    snprintf(leave_msg, sizeof(leave_msg), "%s left the chat.\n", clients[client_idx].name);
                    printf("%s", leave_msg);

                    remove_client(i);
                    broadcast_message(leave_msg, -1);
                    broadcast_user_list();
                    i--;
                }
            }
        }
    }

    close(listenfd);
    return 0;
}