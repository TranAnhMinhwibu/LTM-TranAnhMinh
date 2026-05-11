#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>

#define PORT 8080
#define POOL_SIZE 5

int main() {
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(listener, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("Error: Failed to bind socket on port %d.\n", PORT);
        exit(1);
    }
    if (listen(listener, 10) < 0) {
        exit(1);
    }
    printf("HTTP Server listening on port %d\n", PORT);
    for (int i = 0; i < POOL_SIZE; i++) {
        pid_t pid = fork();
        
        if (pid == 0) { 
            while (1) {
                int client = accept(listener, NULL, NULL);
                if (client < 0) continue;
                printf("[Worker PID: %d] New client connected: %d\n", getpid(), client);
                char buf[256];
                int ret = recv(client, buf, sizeof(buf) - 1, 0);
                if (ret >= 0) {
                    buf[ret] = 0;
                    puts(buf);
                }
                char *msg = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\nWelcome to the drift\n";
                send(client, msg, strlen(msg), 0);
                close(client);
            }
            exit(0);
        }
    }
    while (wait(NULL) > 0);

    return 0;
}