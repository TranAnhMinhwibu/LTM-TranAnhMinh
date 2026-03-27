#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8888
#define BUFFER_SIZE 1024

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Socket creation failed");
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr));
    listen(server_sock, 5);
    
    printf("Server listening on port %d...\n", PORT);
    client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_len);
    printf("Client connected!\n\n");

    char target[] = "0123456789";
    int target_len = 10;
    int state = 0;
    int total_count = 0;
    int bytes_received;

    while ((bytes_received = recv(client_sock, buffer, BUFFER_SIZE, 0)) > 0) {
        buffer[bytes_received] = '\0';
        printf("Received data: %s\n", buffer);

        for (int i = 0; i < bytes_received; i++) {
            if (buffer[i] == target[state]) {
                state++;
                if (state == target_len) {
                    total_count++;
                    state = 0;
                }
            } else {
                if (buffer[i] == target[0]) {
                    state = 1;
                } else {
                    state = 0;
                }
            }
        }
        
        printf("Total: %d\n\n", total_count);
    }

    printf("Client disconnected.\n");
    close(client_sock);
    close(server_sock);
    return 0;
}