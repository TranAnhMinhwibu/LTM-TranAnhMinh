/*******************************************************************************
 * @file    poll_server.c
 * @brief   Mô tả ngắn gọn về chức năng của file
 * @date    2026-04-14 08:36
 *******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <poll.h>

#define MAX_CLIENTS 1000

int main() {
    
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1) {
        perror("socket() failed");
        return 1;
    }
    
    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int))) {
        perror("setsockopt() failed");
        close(listener);
        return 1;
    }
    
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(8080);
    
    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("bind() failed");
        close(listener);
        return 1;
    }
    
    if (listen(listener, 5)) {
        perror("listen() failed");
        close(listener);
        return 1;
    }
    
    // Server is now listening for incoming connections
    printf("Server is listening on port 8080...\n");
    
    struct pollfd fds[MAX_CLIENTS];
    int nfds = 1;

    fds[0].fd = listener;
    fds[0].events = POLLIN;

    char buf[256];

    while (1) {
        int ret = poll(fds, nfds, -1);
        if (ret < 0) {
            perror("poll() failed");
            break;
        }

        // Kiem tra socket listener
        if (fds[0].revents && POLLIN) {
            int client = accept(listener, NULL, NULL);

            if (nfds < MAX_CLIENTS) {
                printf("New client connected: %d\n", client);

                fds[nfds].fd = client;
                fds[nfds].events = POLLIN;
                nfds++;
            } else {
                close(client);
            }
        }

        // Kiem tra cac socket client
        for (int i = 1; i < MAX_CLIENTS; i++)
            if (fds[i].revents && POLLIN) {
                int len = recv(fds[i].fd, buf, sizeof(buf), 0);
                if (len <= 0) {
                    // TODO: xoa khoi mang fds
                    continue;
                }

                buf[len] = 0;
                printf("Received from %d: %s", fds[i].fd, buf);
            }
    }

    close(listener);
    return 0;
}