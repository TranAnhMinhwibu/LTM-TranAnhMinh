#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <poll.h>

#define PORT 8080
#define MAX_CLIENTS 32
#define BUF_SIZE 1024

typedef struct {
    int fd;
    int is_authenticated;
} ClientInfo;

int check_login(const char *username, const char *password) {
    FILE *f = fopen("users.txt", "r");
    if (!f) {
        perror("Cannot open users.txt");
        return 0;
    }

    char f_user[64], f_pass[64];
    while (fscanf(f, "%63s %63s", f_user, f_pass) == 2) {
        if (strcmp(username, f_user) == 0 && strcmp(password, f_pass) == 0) {
            fclose(f);
            return 1;
        }
    }

    fclose(f);
    return 0;
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
        clients[i].is_authenticated = 0;
    }

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt failed");
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

    printf("Telnet Server running on port %d using poll()...\n", PORT);

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
                            clients[i].is_authenticated = 0;
                            break;
                        }
                    }
                    nfds++;
                    char *msg = "Please enter username and password (syntax: user pass): \n";
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
                    
                    char temp_file[64];
                    snprintf(temp_file, sizeof(temp_file), "out_%d.txt", fds[i].fd);
                    remove(temp_file);

                    int current_fd = fds[i].fd;
                    for (int j = 0; j < MAX_CLIENTS; j++) {
                        if (clients[j].fd == current_fd) {
                            clients[j].fd = -1;
                            clients[j].is_authenticated = 0;
                            break;
                        }
                    }
                    close(current_fd);
                    
                    fds[i] = fds[nfds - 1];
                    nfds--;
                    i--;
                } 
                else {
                    buffer[strcspn(buffer, "\r\n")] = 0;
                    if (strlen(buffer) == 0) continue;

                    int client_idx = -1;
                    for (int j = 0; j < MAX_CLIENTS; j++) {
                        if (clients[j].fd == fds[i].fd) {
                            client_idx = j;
                            break;
                        }
                    }

                    if (clients[client_idx].is_authenticated == 0) {
                        char user[64] = {0}, pass[64] = {0};
                        
                        if (sscanf(buffer, "%63s %63s", user, pass) == 2) {
                            if (check_login(user, pass)) {
                                clients[client_idx].is_authenticated = 1;
                                char *msg = "Login successful! Please enter a command to execute:\n";
                                send(fds[i].fd, msg, strlen(msg), 0);
                            } else {
                                char *msg = "Login failed. Incorrect username or password.\nPlease try again (user pass): \n";
                                send(fds[i].fd, msg, strlen(msg), 0);
                            }
                        } else {
                            char *msg = "Invalid syntax. Please enter: 'user pass'\n";
                            send(fds[i].fd, msg, strlen(msg), 0);
                        }
                    } 
                    else {
                        char cmd[BUF_SIZE + 128];
                        char out_file[64];
                        
                        snprintf(out_file, sizeof(out_file), "out_%d.txt", fds[i].fd);
                        snprintf(cmd, sizeof(cmd), "%s > %s 2>&1", buffer, out_file);
                        
                        system(cmd);

                        FILE *f = fopen(out_file, "r");
                        if (f) {
                            char file_buf[BUF_SIZE];
                            size_t bytes_read;
                            int has_output = 0;

                            while ((bytes_read = fread(file_buf, 1, sizeof(file_buf), f)) > 0) {
                                send(fds[i].fd, file_buf, bytes_read, 0);
                                has_output = 1;
                            }
                            fclose(f);
                            
                            if (!has_output) {
                                char *empty_msg = "(Command executed but produced no output)\n";
                                send(fds[i].fd, empty_msg, strlen(empty_msg), 0);
                            }
                        } else {
                            char *msg = "Error reading output file.\n";
                            send(fds[i].fd, msg, strlen(msg), 0);
                        }

                        char *prompt = "\nEnter next command:\n";
                        send(fds[i].fd, prompt, strlen(prompt), 0);
                    }
                }
            }
        }
    }

    close(server_fd);
    return 0;
}