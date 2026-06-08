/*******************************************************************************
 * @file    http_client_get.c
 * @brief   Mô tả ngắn gọn về chức năng của file
 * @date    2026-05-26 07:30
 *******************************************************************************/

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>

int main() {
    int client = socket(AF_INET, SOCK_STREAM, 0);
    struct addrinfo *res;
    getaddrinfo("httpbin.org", "80", NULL, &res);

    connect(client, res->ai_addr, res->ai_addrlen);

    char *request = "GET /get HTTP/1.1\r\n"
        "Host: httpbin.org\r\n"
        "Connection: close\r\n\r\n";
    send(client, request, strlen(request), 0);

    char buf[4096];
    int len = recv(client, buf, sizeof(buf) - 1, 0);
    if (len > 0) {
        buf[len] = '\0';
        printf("%s\n", buf);
    }

    close(client);
    freeaddrinfo(res);

    return 0;
}