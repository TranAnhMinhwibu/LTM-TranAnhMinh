#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        return 1;
    }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        return 1;
    }

    struct sockaddr_in server_addr, client_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[1]));
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(sock);
        return 1;
    }

    printf("UDP Echo Server listening on port %s...\n", argv[1]);

    char buffer[BUFFER_SIZE];
    socklen_t client_len = sizeof(client_addr);

    while (1) {
        int bytes_received = recvfrom(sock, buffer, BUFFER_SIZE - 1, 0, 
                                      (struct sockaddr *)&client_addr, &client_len);

        if (bytes_received > 0) {
            buffer[bytes_received] = '\0';
            printf("Received %d bytes from %s:%d\n", 
                   bytes_received, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

            sendto(sock, buffer, bytes_received, 0, 
                   (struct sockaddr *)&client_addr, client_len);
        }
    }

    close(sock);
    return 0;
}