/*******************************************************************************
 * @file    poll_client.c
 * @brief   Mô tả ngắn gọn về chức năng của file
 * @date    2026-04-14 08:13
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

int main() {
    int client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client == -1) {
        perror("socket() failed");
        return 1;
    }
    
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = htons(8080);
    
    if (connect(client, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("connect() failed");
        close(client);
        return 1;
    }
    
    // Client is now connected to the server
    printf("Connected to server on port 8080...\n");
    
    struct pollfd fds[2];

    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
    fds[1].fd = client;
    fds[1].events = POLLIN;

    char buf[256];

    while (1) {
        int ret = poll(fds, 2, 5000);
        if (ret < 0) {
            perror("poll() failed");
            break;
        }

        if (ret == 0) {
            printf("Timed out.\n");
            continue;
        }
        
        if (fds[0].revents & POLLIN) {
            // Su kien ban phim
            fgets(buf, sizeof(buf), stdin);
            send(client, buf, strlen(buf), 0);
        }

        if (fds[1].revents && POLLIN) {
            ret = recv(client, buf, sizeof(buf), 0);
            if (ret <= 0) {
                break;
            }

            buf[ret] = 0;
            printf("Received: %s", buf);
        }
    }

    close(client);
    return 0;
}