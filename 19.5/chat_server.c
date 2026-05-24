#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <time.h>

#define MAX_CLIENTS 100

typedef struct {
    int fd;
    char name[100];
} client_info;

client_info clients[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

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

void broadcast(char *msg, int sender_fd) {
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < client_count; i++) {
        if (clients[i].fd != sender_fd) {
            send(clients[i].fd, msg, strlen(msg), 0);
        }
    }
    pthread_mutex_unlock(&mutex);
}

void remove_client(int fd) {
    pthread_mutex_lock(&mutex);
    for (int i = 0; i < client_count; i++) {
        if (clients[i].fd == fd) {
            for (int j = i; j < client_count - 1; j++) {
                clients[j] = clients[j + 1];
            }
            client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&mutex);
}

void *handle_client(void *arg) {
    int client_fd = *(int *)arg;
    free(arg);

    char client_name[100];
    char auth_buf[256];

    while (1) {
        send(client_fd, "Please enter client_id: ", 24, 0);
        memset(auth_buf, 0, sizeof(auth_buf));

        int n = read_line(client_fd, auth_buf, sizeof(auth_buf));
        if (n <= 0) {
            close(client_fd);
            return NULL;
        }

        if (strlen(auth_buf) == 0) continue;

        if (strncmp(auth_buf, "client_id: ", 11) == 0 && strlen(auth_buf) > 11) {
            strcpy(client_name, auth_buf + 11);
            send(client_fd, "Accepted\n", 9, 0);
            break;
        } else {
            send(client_fd, "Invalid format\n", 15, 0);
        }
    }

    pthread_mutex_lock(&mutex);
    clients[client_count].fd = client_fd;
    strcpy(clients[client_count].name, client_name);
    client_count++;
    pthread_mutex_unlock(&mutex);

    char msg_buf[1024];
    while (1) {
        memset(msg_buf, 0, sizeof(msg_buf));
        int n = read_line(client_fd, msg_buf, sizeof(msg_buf));

        if (n <= 0) break;
        if (strlen(msg_buf) == 0) continue;

        time_t t = time(NULL);
        struct tm *tm_info = localtime(&t);
        char time_str[30];
        strftime(time_str, 30, "%Y/%m/%d %I:%M:%S%p", tm_info);

        char full_msg[2048];
        snprintf(full_msg, sizeof(full_msg), "%s %s: %s\n", time_str, client_name, msg_buf);

        broadcast(full_msg, client_fd);
    }

    remove_client(client_fd);
    close(client_fd);
    return NULL;
}

int main() {
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

    printf("Chat Server (Multithreaded) listening on port 8080\n");

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int *client_fd = malloc(sizeof(int));
        *client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

        if (*client_fd < 0) {
            free(client_fd);
            continue;
        }

        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, client_fd);
        pthread_detach(tid);
    }

    return 0;
}