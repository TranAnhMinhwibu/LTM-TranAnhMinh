#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include <signal.h>
#include <fcntl.h>

#define DIR_PATH "./files"

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
    mkdir(DIR_PATH, 0777);

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

    printf("File Server is listening on port 8080\n");
    printf("Directory set to: %s\n", DIR_PATH);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

        if (client_fd < 0) continue;

        pid_t pid = fork();
        
        if (pid == 0) {
            close(server_fd);
            
            DIR *d = opendir(DIR_PATH);
            struct dirent *dir;
            int file_count = 0;
            char file_list[8192] = {0};

            if (d) {
                while ((dir = readdir(d)) != NULL) {
                    if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0) continue;
                    
                    char path[1024];
                    snprintf(path, sizeof(path), "%s/%s", DIR_PATH, dir->d_name);
                    
                    struct stat st;
                    if (stat(path, &st) == 0 && S_ISREG(st.st_mode)) {
                        file_count++;
                        strcat(file_list, dir->d_name);
                        strcat(file_list, "\r\n");
                    }
                }
                closedir(d);
            }

            if (file_count == 0) {
                char *err_msg = "ERRORNofiles to download \r\n";
                send(client_fd, err_msg, strlen(err_msg), 0);
                close(client_fd);
                exit(0);
            }

            char header[256];
            snprintf(header, sizeof(header), "OK %d\r\n", file_count);
            send(client_fd, header, strlen(header), 0);
            send(client_fd, file_list, strlen(file_list), 0);
            send(client_fd, "\r\n", 2, 0);

            char filename[256];
            while (1) {
                memset(filename, 0, sizeof(filename));
                int n = read_line(client_fd, filename, sizeof(filename));
                
                if (n <= 0) break;
                if (strlen(filename) == 0) continue;

                char filepath[1024];
                snprintf(filepath, sizeof(filepath), "%s/%s", DIR_PATH, filename);

                struct stat st;
                if (stat(filepath, &st) == 0 && S_ISREG(st.st_mode)) {
                    char ok_msg[256];
                    snprintf(ok_msg, sizeof(ok_msg), "OK %ld\r\n", st.st_size);
                    send(client_fd, ok_msg, strlen(ok_msg), 0);

                    int fd = open(filepath, O_RDONLY);
                    if (fd >= 0) {
                        char buffer[4096];
                        int bytes_read;
                        while ((bytes_read = read(fd, buffer, sizeof(buffer))) > 0) {
                            send(client_fd, buffer, bytes_read, 0);
                        }
                        close(fd);
                    }
                    break;
                } else {
                    char *not_found = "Error: File not found. Please send filename again.\r\n";
                    send(client_fd, not_found, strlen(not_found), 0);
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