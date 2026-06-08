#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>

#define PORT 8081
#define BUFFER_SIZE 8192

const char *get_mime_type(const char *filename) {
    const char *ext = strrchr(filename, '.');
    if (!ext) return "application/octet-stream";
    if (strcmp(ext, ".html") == 0) return "text/html";
    if (strcmp(ext, ".txt") == 0) return "text/plain";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".mp4") == 0) return "video/mp4";
    if (strcmp(ext, ".mp3") == 0) return "audio/mpeg";
    return "application/octet-stream";
}

void serve_file_or_dir(int client_sock, char *path) {
    char local_path[512];
    if (strcmp(path, "/") == 0) strcpy(local_path, ".");
    else snprintf(local_path, sizeof(local_path), ".%s", path);

    struct stat path_stat;
    if (stat(local_path, &path_stat) != 0) {
        char *not_found = "HTTP/1.1 404 Not Found\r\n\r\n404 File Not Found";
        send(client_sock, not_found, strlen(not_found), 0);
        return;
    }

    if (S_ISDIR(path_stat.st_mode)) {
        DIR *dir = opendir(local_path);
        if (dir) {
            char header[] = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\n\r\n"
                            "<html><body><h2>Directory listing: %s</h2><ul>";
            char buffer[4096];
            snprintf(buffer, sizeof(buffer), header, path);
            send(client_sock, buffer, strlen(buffer), 0);

            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (strcmp(entry->d_name, ".") == 0) continue;

                char full_entry_path[1024];
                snprintf(full_entry_path, sizeof(full_entry_path), "%s/%s", local_path, entry->d_name);
                
                struct stat entry_stat;
                stat(full_entry_path, &entry_stat);

                char url_path[1024];
                if (strcmp(path, "/") == 0) snprintf(url_path, sizeof(url_path), "/%s", entry->d_name);
                else snprintf(url_path, sizeof(url_path), "%s/%s", path, entry->d_name);

                if (S_ISDIR(entry_stat.st_mode)) {
                    snprintf(buffer, sizeof(buffer), "<li><a href=\"%s\"><b>%s/</b></a></li>", url_path, entry->d_name);
                } else {
                    snprintf(buffer, sizeof(buffer), "<li><a href=\"%s\"><i>%s</i></a></li>", url_path, entry->d_name);
                }
                send(client_sock, buffer, strlen(buffer), 0);
            }
            closedir(dir);
            char *footer = "</ul></body></html>";
            send(client_sock, footer, strlen(footer), 0);
        }
    } else {
        int fd = open(local_path, O_RDONLY);
        if (fd >= 0) {
            const char *mime = get_mime_type(local_path);
            char header[256];
            snprintf(header, sizeof(header), 
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: %s\r\n"
                "Content-Length: %ld\r\n"
                "Connection: close\r\n\r\n", mime, path_stat.st_size);
            send(client_sock, header, strlen(header), 0);

            char file_buf[BUFFER_SIZE];
            ssize_t bytes_read;
            while ((bytes_read = read(fd, file_buf, sizeof(file_buf))) > 0) {
                send(client_sock, file_buf, bytes_read, 0);
            }
            close(fd);
        }
    }
}

int main() {
    int server = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = { .sin_family = AF_INET, .sin_port = htons(PORT), .sin_addr.s_addr = INADDR_ANY };
    bind(server, (struct sockaddr *)&addr, sizeof(addr));
    listen(server, 10);

    printf("File Server is running at http://localhost:%d\n", PORT);

    while (1) {
        int client = accept(server, NULL, NULL);
        char buf[BUFFER_SIZE];
        int bytes = recv(client, buf, sizeof(buf) - 1, 0);
        
        if (bytes > 0) {
            buf[bytes] = '\0';
            char method[10], path[1024];
            sscanf(buf, "%s %s", method, path);
            
            if (strcmp(method, "GET") == 0) {
                serve_file_or_dir(client, path);
            }
        }
        close(client);
    }
    return 0;
}