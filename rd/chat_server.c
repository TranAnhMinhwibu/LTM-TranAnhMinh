/*******************************************************************************
 * @file    chat_server.c
 * @brief   Yeu cau client dang nhap va chuyen tiep tin nhan.
 * @date    2026-04-07 08:43
 *******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/select.h>

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
    
    fd_set fdread, fdtest;
    FD_ZERO(&fdread);
    FD_SET(listener, &fdread);

    char buf[256];

    char *ids[FD_SETSIZE] = {NULL};

    while (1) {
        // Reset tap fdtest
        fdtest = fdread;

        int ret = select(FD_SETSIZE, &fdtest, NULL, NULL, NULL);
        if (ret < 0) {
            perror("select() failed");
            break;
        }

        for (int i = 0; i < FD_SETSIZE; i++) {
            if (FD_ISSET(i, &fdtest)) {
                if (i == listener) {
                    int client = accept(listener, NULL, NULL);
                    if (client < FD_SETSIZE) {
                        printf("New client connected %d\n", client);
                        FD_SET(client, &fdread);
                        char *msg = "Xin chao. Hay dang nhap!\n";
                        send(client, msg, strlen(msg), 0);
                    } else {
                        close(client);
                    }
                } else {
                    ret = recv(i, buf, sizeof(buf), 0);
                    if (ret <= 0) {
                        printf("Client %d disconnected\n", i);    
                        FD_CLR(i, &fdread);
                        free(ids[i]);
                        ids[i] = NULL;
                    } else {
                        buf[ret] = 0;
                        printf("Received from %d: %s\n", i, buf);

                        // Xu ly dang nhap
                        if (ids[i] == NULL) {
                            // Chua dang nhap
                            // Kiem tra cu phap "client_id: client_name"
                            char cmd[32], id[32], tmp[32];
                            int n = sscanf(buf, "%s%s%s", cmd, id, tmp);
                            if (n != 2) {
                                char *msg = "Error. Thua hoac thieu tham so!\n";
                                send(i, msg, strlen(msg), 0);
                            } else {
                                if (strcmp(cmd, "client_id:") != 0) {
                                    char *msg = "Error. Sai cu phap!\n";
                                    send(i, msg, strlen(msg), 0);
                                } else {
                                    char *msg = "OK. Hay nhap tin nhan!\n";
                                    send(i, msg, strlen(msg), 0);
                                    
                                    ids[i] = malloc(strlen(id) + 1);
                                    strcpy(ids[i], id);
                                }
                            }
                        } else {
                            // Da dang nhap
                            char target[32];
                            int n = sscanf(buf, "%s", target);
                            if (n == 0) 
                                continue;
                            
                            if (strcmp(target, "all") == 0) {
                                // Chuyen tiep tin nhan cho tat ca client
                                for (int j = 0; j < FD_SETSIZE; j++)
                                    if (ids[j] != NULL && i != j) {
                                        send(j, ids[i], strlen(ids[i]), 0);
                                        send(j, ": ", 2, 0);
                                        char *pos = buf + strlen(target) + 1;
                                        send(j, pos, strlen(pos), 0);
                                    }
                            } else {
                                int j = 0;
                                for (;j < FD_SETSIZE; j++)
                                    if (ids[j] != NULL && strcmp(target, ids[j]) == 0)
                                        break;
                                if (j < FD_SETSIZE) {
                                    // Tim thay
                                    send(j, ids[i], strlen(ids[i]), 0);
                                    send(j, ": ", 2, 0);
                                    char *pos = buf + strlen(target) + 1;
                                    send(j, pos, strlen(pos), 0);
                                } else {
                                    // Khong tim thay
                                    continue;
                                }
                            }                                  
                        }
                    }
                }            
            }
        }
    }

    close(listener);
    return 0;
}