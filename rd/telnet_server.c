/*******************************************************************************
 * @file    telnet_server.c
 * @brief   Client dang nhap va thuc hien lenh
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

    struct timeval tv;
    char buf[256];

    int login[FD_SETSIZE] = {0};

    while (1) {
        // Reset tap fdtest
        fdtest = fdread;

        // Reset timeout
        tv.tv_sec = 60;
        tv.tv_usec = 0;

        int ret = select(FD_SETSIZE, &fdtest, NULL, NULL, &tv);
        if (ret < 0) {
            perror("select() failed");
            break;
        }
        if (ret == 0) {
            printf("Timed out.\n");
            continue;
        }

        for (int i = 0; i < FD_SETSIZE; i++) {
            if (FD_ISSET(i, &fdtest)) {
                if (i == listener) {
                    int client = accept(listener, NULL, NULL);
                    if (client < FD_SETSIZE) {
                        printf("New client connected %d\n", client);
                        FD_SET(client, &fdread);
                    } else {
                        close(client);
                    }
                } else {
                    ret = recv(i, buf, sizeof(buf), 0);
                    if (ret <= 0) {
                        printf("Client %d disconnected\n", i);    
                        FD_CLR(i, &fdread);
                        login[i] = 0;
                    } else {
                        buf[ret] = 0;
                        if (buf[strlen(buf) - 1] == '\n')
                            buf[strlen(buf) - 1] = 0;
                        printf("Received from %d: %s\n", i, buf);

                        if (login[i] == 0) {
                            // Kiem tra ky tu enter
                            char user[32], pass[32], tmp[64];
                            int n = sscanf(buf, "%s%s%s", user, pass, tmp);
                            if (n != 2) {
                                char *msg = "Sai cu phap. Hay dang nhap lai.\n";
                                send(i, msg, strlen(msg), 0);
                            } else {
                                sprintf(tmp, "%s %s", user, pass);
                                int found = 0;
                                char line[64];
                                FILE *f = fopen("users.txt", "r");
                                while (fgets(line, sizeof(line), f) != NULL) {
                                    if (line[strlen(line) - 1] == '\n')
                                        line[strlen(line) - 1] = 0;
                                    if (strcmp(line, tmp) == 0) {
                                        found = 1;
                                        break;
                                    }
                                }
                                fclose(f);

                                if (found == 1) {
                                    char *msg = "OK. Hay nhap lenh.\n";
                                    send(i, msg, strlen(msg), 0);
                                    login[i] = 1;
                                } else {
                                    char *msg = "Sai username hoac password. Hay dang nhap lai.\n";
                                    send(i, msg, strlen(msg), 0);
                                }
                            }
                        } else {
                            // Thuc hien lenh tu client
                            char cmd[512];
                            sprintf(cmd, "%s > out.txt", buf);
                            system(cmd);
                            FILE *f = fopen("out.txt", "rb");
                            while (1) {
                                int len = fread(buf, 1, sizeof(buf), f);
                                if (len <= 0)
                                    break;
                                send(i, buf, len, 0);
                            }
                            fclose(f);
                        }
                    }
                }            
            }
        }
    }

    close(listener);
    return 0;
}