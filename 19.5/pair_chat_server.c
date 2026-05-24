#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <poll.h>

int waiting_client = -1;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void *chat_session(void *arg) {
    int *fds = (int *)arg;
    int c1 = fds[0];
    int c2 = fds[1];
    free(fds);

    char *match_msg = "Matched! You are now chatting with a partner.\n";
    send(c1, match_msg, strlen(match_msg), 0);
    send(c2, match_msg, strlen(match_msg), 0);

    struct pollfd pfds[2];
    pfds[0].fd = c1;
    pfds[0].events = POLLIN;
    pfds[1].fd = c2;
    pfds[1].events = POLLIN;

    char buf[2048];

    while (1) {
        int p = poll(pfds, 2, -1);
        if (p < 0) break;

        if (pfds[0].revents & POLLIN) {
            int n = recv(c1, buf, sizeof(buf), 0);
            if (n <= 0) break;
            send(c2, buf, n, 0);
        }

        if (pfds[1].revents & POLLIN) {
            int n = recv(c2, buf, sizeof(buf), 0);
            if (n <= 0) break;
            send(c1, buf, n, 0);
        }

        if ((pfds[0].revents & (POLLERR | POLLHUP)) || 
            (pfds[1].revents & (POLLERR | POLLHUP))) {
            break;
        }
    }

    char *dc_msg = "Partner disconnected. Closing session.\n";
    send(c1, dc_msg, strlen(dc_msg), 0);
    send(c2, dc_msg, strlen(dc_msg), 0);

    close(c1);
    close(c2);
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

    printf("Pair Chat Server is listening on port 8080\n");

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

        if (client_fd < 0) continue;

        pthread_mutex_lock(&mutex);
        
        if (waiting_client == -1) {
            waiting_client = client_fd;
            char *wait_msg = "Waiting for a partner...\n";
            send(client_fd, wait_msg, strlen(wait_msg), 0);
            pthread_mutex_unlock(&mutex);
        } else {
            int *fds = malloc(2 * sizeof(int));
            fds[0] = waiting_client;
            fds[1] = client_fd;
            waiting_client = -1;
            pthread_mutex_unlock(&mutex);

            pthread_t tid;
            pthread_create(&tid, NULL, chat_session, fds);
            pthread_detach(tid);
        }
    }
    
    return 0;
}