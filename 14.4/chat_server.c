#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <poll.h>
#include <time.h>

#define PORT 8080
#define MAX_CLIENTS 32
#define BUF_SIZE 1024

typedef struct {
    int fd;
    int is_registered;
    char name[64];
} ClientInfo;

void get_timestamp(char *buf, size_t size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buf, size, "%Y/%m/%d %I:%M:%S%p", t);
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    
    ClientInfo clients[MAX_CLIENTS];
    struct pollfd fds[MAX_CLIENTS + 1];
    int nfds = 1;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = -1;
        clients[i].is_registered = 0;
    }

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    memset(fds, 0, sizeof(fds));
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;

    printf("Server listening on port %d ...\n", PORT);

    while (1) {
        int poll_count = poll(fds, nfds, -1);
        if (poll_count < 0) {
            perror("Poll error");
            break;
        }

        if (fds[0].revents & POLLIN) {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) >= 0) {
                if (nfds < MAX_CLIENTS + 1) {
                    printf("New connection, fd: %d, ip: %s\n", new_socket, inet_ntoa(address.sin_addr));
                    fds[nfds].fd = new_socket;
                    fds[nfds].events = POLLIN;
                    
                    for (int i = 0; i < MAX_CLIENTS; i++) {
                        if (clients[i].fd == -1) {
                            clients[i].fd = new_socket;
                            clients[i].is_registered = 0;
                            break;
                        }
                    }
                    nfds++;
                    char *msg = "Please enter your name with syntax: 'client_id: client_name'\n";
                    send(new_socket, msg, strlen(msg), 0);
                } else {
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
                            clients[j].is_registered = 0;
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

                    if (clients[client_idx].is_registered == 0) {
                        char temp_name[64];
                        if (sscanf(buffer, "client_id: %63s", temp_name) == 1) {
                            strcpy(clients[client_idx].name, temp_name);
                            clients[client_idx].is_registered = 1;
                            char success_msg[128];
                            snprintf(success_msg, sizeof(success_msg), "Registration successful. Hello %s!\n", clients[client_idx].name);
                            send(fds[i].fd, success_msg, strlen(success_msg), 0);
                        } else {
                            char *err_msg = "Invalid syntax! Please try again: 'client_id: client_name'\n";
                            send(fds[i].fd, err_msg, strlen(err_msg), 0);
                        }
                    } else {
                        char time_str[64];
                        get_timestamp(time_str, sizeof(time_str));
                        char broadcast_msg[BUF_SIZE + 256];
                        snprintf(broadcast_msg, sizeof(broadcast_msg), "%s %s: %s\n", time_str, clients[client_idx].name, buffer);
                        
                        for (int j = 0; j < MAX_CLIENTS; j++) {
                            if (clients[j].fd > 0 && clients[j].fd != fds[i].fd && clients[j].is_registered == 1) {
                                send(clients[j].fd, broadcast_msg, strlen(broadcast_msg), 0);
                            }
                        }
                    }
                }
            }
        }
    }

    close(server_fd);
    return 0;
}