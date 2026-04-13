#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <time.h>

#define PORT 8080
#define MAX_CLIENTS 30
#define BUF_SIZE 1024

typedef struct {
    int fd;
    int is_registered;
    char name[64];
} Client;

void get_timestamp(char *buf, size_t size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    strftime(buf, size, "%Y/%m/%d %I:%M:%S%p", t);
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    
    Client clients[MAX_CLIENTS];
    fd_set readfds;
    
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
    
    printf("Server listening on port %d...\n", PORT);

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        int max_sd = server_fd;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = clients[i].fd;
            if (sd > 0) {
                FD_SET(sd, &readfds);
            }
            if (sd > max_sd) {
                max_sd = sd;
            }
        }

        int activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);
        if (activity < 0) {
            perror("Select error");
            continue;
        }

        if (FD_ISSET(server_fd, &readfds)) {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
                perror("Accept failed");
                continue;
            }

            printf("New connection, socket fd is %d, ip is: %s, port: %d\n", 
                   new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].fd == -1) {
                    clients[i].fd = new_socket;
                    clients[i].is_registered = 0;
                    
                    char *msg = "Please enter your name with syntax: 'client_id: client_name'\n";
                    send(new_socket, msg, strlen(msg), 0);
                    break;
                }
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = clients[i].fd;

            if (sd > 0 && FD_ISSET(sd, &readfds)) {
                char buffer[BUF_SIZE] = {0};
                int valread = read(sd, buffer, BUF_SIZE);

                if (valread == 0) {
                    printf("Client (fd: %d) disconnected.\n", sd);
                    close(sd);
                    clients[i].fd = -1;
                } else {
                    buffer[strcspn(buffer, "\r\n")] = 0;

                    if (strlen(buffer) == 0) continue;

                    if (clients[i].is_registered == 0) {
                        char temp_name[64];
                        if (sscanf(buffer, "client_id: %63s", temp_name) == 1) {
                            strcpy(clients[i].name, temp_name);
                            clients[i].is_registered = 1;
                            
                            char success_msg[128];
                            snprintf(success_msg, sizeof(success_msg), "Registration successful. Hello %s!\n", clients[i].name);
                            send(sd, success_msg, strlen(success_msg), 0);
                            printf("Client (fd: %d) registered name: %s\n", sd, clients[i].name);
                        } else {
                            char *err_msg = "Invalid syntax! Please try again: 'client_id: client_name'\n";
                            send(sd, err_msg, strlen(err_msg), 0);
                        }
                    } else {
                        char time_str[64];
                        get_timestamp(time_str, sizeof(time_str));
                        
                        char broadcast_msg[BUF_SIZE + 256];
                        snprintf(broadcast_msg, sizeof(broadcast_msg), "%s %s: %s\n", time_str, clients[i].name, buffer);
                        
                        for (int j = 0; j < MAX_CLIENTS; j++) {
                            int dest_sd = clients[j].fd;
                            if (dest_sd > 0 && dest_sd != sd && clients[j].is_registered == 1) {
                                send(dest_sd, broadcast_msg, strlen(broadcast_msg), 0);
                            }
                        }
                    }
                }
            }
        }
    }

    return 0;
}