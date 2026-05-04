#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#define PORT 9000
#define MAX_CLIENTS 100
#define MAX_TOPICS_PER_CLIENT 20
#define MAX_TOPIC_LEN 64
#define BUFFER_SIZE 1024

typedef struct {
    int fd;
    char topics[MAX_TOPICS_PER_CLIENT][MAX_TOPIC_LEN];
    int topic_count;
} Client;

Client clients[MAX_CLIENTS];

void init_clients() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].fd = 0;
        clients[i].topic_count = 0;
    }
}

void handle_sub(Client *client, const char *topic) {
    if (client->topic_count >= MAX_TOPICS_PER_CLIENT) {
        char *err_msg = "Error: Maximum number of topics reached.\n";
        send(client->fd, err_msg, strlen(err_msg), 0);
        return;
    }

    for (int i = 0; i < client->topic_count; i++) {
        if (strcmp(client->topics[i], topic) == 0) {
            char *msg = "Info: You are already subscribed to this topic.\n";
            send(client->fd, msg, strlen(msg), 0);
            return;
        }
    }

    strncpy(client->topics[client->topic_count], topic, MAX_TOPIC_LEN - 1);
    client->topics[client->topic_count][MAX_TOPIC_LEN - 1] = '\0';
    client->topic_count++;
    
    printf("Client FD %d subscribed to topic: %s\n", client->fd, topic);
    char *success_msg = "Subscribed successfully!\n";
    send(client->fd, success_msg, strlen(success_msg), 0);
}

void handle_unsub(Client *client, const char *topic) {
    int found = 0;
    for (int i = 0; i < client->topic_count; i++) {
        if (strcmp(client->topics[i], topic) == 0) {
            found = 1;
            for (int j = i; j < client->topic_count - 1; j++) {
                strncpy(client->topics[j], client->topics[j + 1], MAX_TOPIC_LEN);
            }
            client->topic_count--;
            break;
        }
    }

    if (found) {
        printf("Client FD %d unsubscribed from topic: %s\n", client->fd, topic);
        char *success_msg = "Unsubscribed successfully!\n";
        send(client->fd, success_msg, strlen(success_msg), 0);
    } else {
        char *err_msg = "Error: You are not subscribed to this topic.\n";
        send(client->fd, err_msg, strlen(err_msg), 0);
    }
}

void handle_pub(const char *topic, const char *msg, int sender_fd) {
    char send_buf[BUFFER_SIZE];
    snprintf(send_buf, sizeof(send_buf), "[%s] %s\n", topic, msg);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd > 0 && clients[i].fd != sender_fd) {
            int is_subscribed = 0;
            for (int j = 0; j < clients[i].topic_count; j++) {
                if (strcmp(clients[i].topics[j], topic) == 0) {
                    is_subscribed = 1;
                    break;
                }
            }
            
            if (is_subscribed) {
                send(clients[i].fd, send_buf, strlen(send_buf), 0);
            }
        }
    }
}

int main() {
    int server_fd, new_socket, max_sd, sd, activity;
    struct sockaddr_in address;
    fd_set readfds;

    init_clients();

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("Setsockopt failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d...\n", PORT);

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        max_sd = server_fd;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = clients[i].fd;
            if (sd > 0) FD_SET(sd, &readfds);
            if (sd > max_sd) max_sd = sd;
        }

        activity = select(max_sd + 1, &readfds, NULL, NULL, NULL);

        if ((activity < 0)) {
            perror("Select error");
        }

        if (FD_ISSET(server_fd, &readfds)) {
            int addrlen = sizeof(address);
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
                perror("Accept error");
                exit(EXIT_FAILURE);
            }

            printf("New connection! Socket FD: %d, IP: %s, Port: %d\n",
                   new_socket, inet_ntoa(address.sin_addr), ntohs(address.sin_port));

            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].fd == 0) {
                    clients[i].fd = new_socket;
                    clients[i].topic_count = 0;
                    break;
                }
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            sd = clients[i].fd;

            if (FD_ISSET(sd, &readfds)) {
                char buffer[BUFFER_SIZE] = {0};
                int valread = read(sd, buffer, BUFFER_SIZE - 1);

                if (valread == 0) {
                    printf("Client FD %d disconnected.\n", sd);
                    close(sd);
                    clients[i].fd = 0;
                    clients[i].topic_count = 0;
                } else {
                    buffer[valread] = '\0';
                    buffer[strcspn(buffer, "\r\n")] = 0;

                    char topic[MAX_TOPIC_LEN] = {0};
                    char msg[BUFFER_SIZE] = {0};
                    
                    if (strncmp(buffer, "SUB ", 4) == 0) {
                        if (sscanf(buffer, "SUB %s", topic) == 1) {
                            handle_sub(&clients[i], topic);
                        }
                    } 
                    else if (strncmp(buffer, "UNSUB ", 6) == 0) {
                        if (sscanf(buffer, "UNSUB %s", topic) == 1) {
                            handle_unsub(&clients[i], topic);
                        }
                    }
                    else if (strncmp(buffer, "PUB ", 4) == 0) {
                        if (sscanf(buffer, "PUB %s %[^\n]", topic, msg) == 2) {
                            printf("Client FD %d published to [%s]: %s\n", sd, topic, msg);
                            handle_pub(topic, msg, sd);
                        }
                    } else {
                        char *err = "Invalid command. Please use: SUB <topic>, UNSUB <topic>, or PUB <topic> <msg>\n";
                        send(sd, err, strlen(err), 0);
                    }
                }
            }
        }
    }
    return 0;
}