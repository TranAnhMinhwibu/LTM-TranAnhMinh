#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <poll.h>
#include <fcntl.h>

#define PORT 8080
#define MAX_CLIENTS 4
#define BUF_SIZE 1024

void encrypt(char *str) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] >= 'a' && str[i] <= 'z') {
            str[i] = (str[i] == 'z') ? 'a' : str[i] + 1;
        } 
        else if (str[i] >= 'A' && str[i] <= 'Z') {
            str[i] = (str[i] == 'Z') ? 'A' : str[i] + 1;
        } 
        else if (str[i] >= '0' && str[i] <= '9') {
            str[i] = '9' - (str[i] - '0');
        }
    }
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    
    struct pollfd fds[MAX_CLIENTS + 1];
    int nfds = 1;
    int active_clients = 0;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    int flags = fcntl(server_fd, F_GETFL, 0);
    fcntl(server_fd, F_SETFL, flags | O_NONBLOCK);

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

    for (int i = 0; i <= MAX_CLIENTS; i++) {
        fds[i].fd = -1;
        fds[i].events = POLLIN;
    }

    fds[0].fd = server_fd;

    printf("Server running on %d ...\n", PORT);

    while (1) {
        int poll_count = poll(fds, nfds, -1);
        if (poll_count < 0) {
            perror("Poll error");
            break;
        }

        if (fds[0].revents & POLLIN) {
            while ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) >= 0) {
                int client_flags = fcntl(new_socket, F_GETFL, 0);
                fcntl(new_socket, F_SETFL, client_flags | O_NONBLOCK);

                if (nfds < MAX_CLIENTS + 1) {
                    fds[nfds].fd = new_socket;
                    fds[nfds].events = POLLIN;
                    nfds++;
                    active_clients++;

                    char greeting[BUF_SIZE];
                    snprintf(greeting, sizeof(greeting), "Hello, There are %d clients connected.\n", active_clients);
                    send(new_socket, greeting, strlen(greeting), 0);
                } 
                else {
                    char *reject = "Server full.\n";
                    send(new_socket, reject, strlen(reject), 0);
                    close(new_socket);
                }
            }
        }

        for (int i = 1; i < nfds; i++) {
            if (fds[i].fd > 0 && (fds[i].revents & POLLIN)) {
                char buffer[BUF_SIZE] = {0};
                int valread = read(fds[i].fd, buffer, BUF_SIZE);

                if (valread <= 0) {
                    close(fds[i].fd);
                    fds[i].fd = -1;
                    active_clients--;
                } 
                else {
                    buffer[strcspn(buffer, "\r\n")] = 0;

                    if (strcmp(buffer, "exit") == 0) {
                        char *bye = "Goodbye.\n";
                        send(fds[i].fd, bye, strlen(bye), 0);
                        close(fds[i].fd);
                        fds[i].fd = -1;
                        active_clients--;
                    } else {
                        encrypt(buffer);
                        char response[BUF_SIZE + 2];
                        snprintf(response, sizeof(response), "%s\n", buffer);
                        send(fds[i].fd, response, strlen(response), 0);
                    }
                }
            }
        }
    }

    close(server_fd);
    return 0;
}