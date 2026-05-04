#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <poll.h>

#define PORT 9000
#define MAX_CLIENTS 100
#define MAX_TOPICS 10
#define BUF_SIZE 1024

typedef struct {
    int fd;
    char topics[MAX_TOPICS][64];
    int num_topics;
} ClientInfo;

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    
    ClientInfo clients[MAX_CLIENTS];
    struct pollfd fds[MAX_CLIENTS + 1];
    int nfds = 1;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = -1;
        clients[i].num_topics = 0;
    }

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 5) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    memset(fds, 0, sizeof(fds));
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;

    printf("Pub/Sub Server running on port %d...\n", PORT);

    while (1) {
        int poll_count = poll(fds, nfds, -1);
        if (poll_count < 0) {
            perror("Poll error");
            break;
        }

        if (fds[0].revents & POLLIN) {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) >= 0) {
                if (nfds < MAX_CLIENTS + 1) {
                    printf("New client connected, fd: %d\n", new_socket);
                    fds[nfds].fd = new_socket;
                    fds[nfds].events = POLLIN;
                    
                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        if (clients[i].fd == -1) {
                            clients[i].fd = new_socket;
                            clients[i].num_topics = 0;
                            break;
                        }
                    }
                    nfds++;
                    
                    char *welcome = "Connected successfully. Supported commands: 'SUB <topic>' or 'PUB <topic> <msg>'\n";
                    send(new_socket, welcome, strlen(welcome), 0);
                } else {
                    char *reject = "Server is full.\n";
                    send(new_socket, reject, strlen(reject), 0);
                    close(new_socket);
                }
            }
        }

        for (int i = 1; i < nfds; i++) {
            if (fds[i].revents & POLLIN) {
                char buffer[BUF_SIZE] = {0};
                int valread = read(fds[i].fd, buffer, BUF_SIZE);

                if (valread <= 0) {
                    printf("Client (fd: %d) disconnected.\n", fds[i].fd);
                    int current_fd = fds[i].fd;
                    
                    for (int j = 0; j < MAX_CLIENTS; j++) {
                        if (clients[j].fd == current_fd) {
                            clients[j].fd = -1;
                            clients[j].num_topics = 0;
                            break;
                        }
                    }
                    close(current_fd);
                    
                    fds[i] = fds[nfds - 1];
                    nfds--;
                    i--;
                } else {
                    buffer[strcspn(buffer, "\r\n")] = 0;
                    if (strlen(buffer) == 0) continue;

                    int client_idx = -1;
                    for (int j = 0; j < MAX_CLIENTS; j++) {
                        if (clients[j].fd == fds[i].fd) {
                            client_idx = j;
                            break;
                        }
                    }

                    char topic[64];
                    char msg[BUF_SIZE];

                    if (strncmp(buffer, "SUB ", 4) == 0) {
                        if (sscanf(buffer, "SUB %63s", topic) == 1) {
                            if (clients[client_idx].num_topics < MAX_TOPICS) {
                                int already_subbed = 0;
                                for (int k = 0; k < clients[client_idx].num_topics; k++) {
                                    if (strcmp(clients[client_idx].topics[k], topic) == 0) {
                                        already_subbed = 1;
                                        break;
                                    }
                                }
                                
                                if (!already_subbed) {
                                    strcpy(clients[client_idx].topics[clients[client_idx].num_topics], topic);
                                    clients[client_idx].num_topics++;
                                    
                                    char resp[128];
                                    snprintf(resp, sizeof(resp), "Subscribed to topic: %s\n", topic);
                                    send(fds[i].fd, resp, strlen(resp), 0);
                                    printf("Client %d subscribed to topic: %s\n", fds[i].fd, topic);
                                }
                            } else {
                                char *resp = "reached the maximum number of subscribed topics.\n";
                                send(fds[i].fd, resp, strlen(resp), 0);
                            }
                        }
                    } 
                    else if (strncmp(buffer, "PUB ", 4) == 0) {
                        if (sscanf(buffer, "PUB %63s %[^\n]", topic, msg) == 2) {
                            char forward_msg[BUF_SIZE + 128];
                            snprintf(forward_msg, sizeof(forward_msg), "[%s] %s\n", topic, msg);
                            
                            int sent_count = 0;
                            for (int j = 0; j < MAX_CLIENTS; j++) {
                                if (clients[j].fd > 0) {
                                    for (int k = 0; k < clients[j].num_topics; k++) {
                                        if (strcmp(clients[j].topics[k], topic) == 0) {
                                            send(clients[j].fd, forward_msg, strlen(forward_msg), 0);
                                            sent_count++;
                                            break;
                                        }
                                    }
                                }
                            }
                            printf("Client %d PUB to topic '%s' (%d recipients)\n", fds[i].fd, topic, sent_count);
                        } else {
                            char *resp = "Invalid syntax. Please use: PUB <topic> <msg>\n";
                            send(fds[i].fd, resp, strlen(resp), 0);
                        }
                    } else {
                        char *resp = "Invalid command. Supported: SUB, PUB\n";
                        send(fds[i].fd, resp, strlen(resp), 0);
                    }
                }
            }
        }
    }

    close(server_fd);
    return 0;
}