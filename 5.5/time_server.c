#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>

int read_line(int sock, char *buf, int size) {
    int i = 0;
    unsigned char c;
    while (i < size - 1) {
        int n = recv(sock, &c, 1, 0);
        if (n <= 0) return n;
        
        if (c == 255) {
            unsigned char temp;
            recv(sock, &temp, 1, 0);
            recv(sock, &temp, 1, 0);
            continue;
        }

        if (c == '\r') continue;
        if (c == '\n') break;
        
        buf[i++] = c;
    }
    buf[i] = '\0';
    return i;
}

int main() {
    signal(SIGCHLD, SIG_IGN);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8080);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("Error: Port 8080 is already in use.\n");
        exit(1);
    }

    if (listen(server_fd, 10) < 0) exit(1);

    printf("Time Server is listening on port 8080\n");

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

        if (client_fd < 0) continue;

        pid_t pid = fork();
        
        if (pid == 0) {
            close(server_fd);
            
            char buf[256];
            while (1) {
                memset(buf, 0, sizeof(buf));
                int n = read_line(client_fd, buf, sizeof(buf));
                
                if (n <= 0) break;
                if (strlen(buf) == 0) continue;

                if (strncmp(buf, "GET_TIME ", 9) == 0) {
                    char *format = buf + 9;
                    time_t t = time(NULL);
                    struct tm *tm_info = localtime(&t);
                    char time_str[100];
                    int is_valid = 1;

                    if (strcmp(format, "dd/mm/yyyy") == 0) {
                        strftime(time_str, sizeof(time_str), "%d/%m/%Y\n", tm_info);
                    } else if (strcmp(format, "dd/mm/yy") == 0) {
                        strftime(time_str, sizeof(time_str), "%d/%m/%y\n", tm_info);
                    } else if (strcmp(format, "mm/dd/yyyy") == 0) {
                        strftime(time_str, sizeof(time_str), "%m/%d/%Y\n", tm_info);
                    } else if (strcmp(format, "mm/dd/yy") == 0) {
                        strftime(time_str, sizeof(time_str), "%m/%d/%y\n", tm_info);
                    } else {
                        is_valid = 0;
                    }

                    if (is_valid) {
                        send(client_fd, time_str, strlen(time_str), 0);
                    } else {
                        send(client_fd, "Error: Unsupported time format\n", 31, 0);
                    }
                } else {
                    send(client_fd, "Error: Invalid command. Use GET_TIME [format]\n", 46, 0);
                }
            }
            
            close(client_fd);
            exit(0);
        } else {
            close(client_fd);
        }
    }
    return 0;
}