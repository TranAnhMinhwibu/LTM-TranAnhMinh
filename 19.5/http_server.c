#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

#define POOL_SIZE 5

int listener;
pthread_mutex_t accept_mutex = PTHREAD_MUTEX_INITIALIZER;

void *worker_thread(void *arg) {
    while (1) {
        pthread_mutex_lock(&accept_mutex);
        int client = accept(listener, NULL, NULL);
        pthread_mutex_unlock(&accept_mutex);

        if (client < 0) continue;

        printf("New client connected: %d\n", client);

        char buf[256];
        int ret = recv(client, buf, sizeof(buf) - 1, 0);
        
        if (ret >= 0) {
            buf[ret] = '\0';
            puts(buf);
        }

        char *msg = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<html><body><h1>Xin chao cac ban</h1></body></html>";
        send(client, msg, strlen(msg), 0);

        close(client);
    }
    return NULL;
}

int main() {
    listener = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8080);

    if (bind(listener, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("Error: Port 8080 is already in use.\n");
        exit(1);
    }

    if (listen(listener, 10) < 0) exit(1);

    printf("Prethreading HTTP Server listening on port 8080\n");

    pthread_t threads[POOL_SIZE];
    for (int i = 0; i < POOL_SIZE; i++) {
        pthread_create(&threads[i], NULL, worker_thread, NULL);
    }

    for (int i = 0; i < POOL_SIZE; i++) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}