#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#define PORT 8080
#define MAX_CLIENTS 30
#define BUF_SIZE 1024

typedef struct {
    int fd;
    int is_authenticated;
} Client;

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
    
    Client clients[MAX_CLIENTS];
    fd_set readfds;
    
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
    
    printf("Telnet Server running on port %d...\n", PORT);

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        int max_sd = server_fd;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = clients[i].fd;
            if (sd > 0) FD_SET(sd, &readfds);
            if (sd > max_sd) max_sd = sd;
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

            printf("New client connected, fd: %d\n", new_socket);

            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].fd == -1) {
                    clients[i].fd = new_socket;
                    clients[i].is_authenticated = 0;
                    
                    char *msg = "Please enter username and password (syntax: user pass): \n";
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

                if (valread <= 0) {
                    printf("Client (fd: %d) disconnected.\n", sd);
                    
                    char temp_file[64];
                    snprintf(temp_file, sizeof(temp_file), "out_%d.txt", sd);
                    remove(temp_file);

                    close(sd);
                    clients[i].fd = -1;
                } 
                else {
                    buffer[strcspn(buffer, "\r\n")] = 0;
                    if (strlen(buffer) == 0) continue;

                    if (clients[i].is_authenticated == 0) {
                        char user[64] = {0}, pass[64] = {0};
                        
                        if (sscanf(buffer, "%63s %63s", user, pass) == 2) {
                            if (check_login(user, pass)) {
                                clients[i].is_authenticated = 1;
                                char *msg = "Login successful! Please enter a command to execute:\n";
                                send(sd, msg, strlen(msg), 0);
                            } else {
                                char *msg = "Login failed. Incorrect username or password.\nPlease try again (user pass): \n";
                                send(sd, msg, strlen(msg), 0);
                            }
                        } else {
                            char *msg = "Invalid syntax. Please enter: 'user pass'\n";
                            send(sd, msg, strlen(msg), 0);
                        }
                    } 
                    else {
                        char cmd[BUF_SIZE + 128];
                        char out_file[64];
                        
                        snprintf(out_file, sizeof(out_file), "out_%d.txt", sd);
                        snprintf(cmd, sizeof(cmd), "%s > %s 2>&1", buffer, out_file);
                        
                        system(cmd);

                        FILE *f = fopen(out_file, "r");
                        if (f) {
                            char file_buf[BUF_SIZE];
                            size_t bytes_read;
                            int has_output = 0;

                            while ((bytes_read = fread(file_buf, 1, sizeof(file_buf), f)) > 0) {
                                send(sd, file_buf, bytes_read, 0);
                                has_output = 1;
                            }
                            fclose(f);
                            
                            if (!has_output) {
                                char *empty_msg = "(Command executed but produced no output)\n";
                                send(sd, empty_msg, strlen(empty_msg), 0);
                            }
                        } else {
                            char *msg = "Error reading output file.\n";
                            send(sd, msg, strlen(msg), 0);
                        }

                        char *prompt = "\nEnter next command:\n";
                        send(sd, prompt, strlen(prompt), 0);
                    }
                }
            }
        }
    }

    return 0;
}