#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Using: %s <IP address> <Port>\n", argv[0]);
        exit(1);
    }

    char *server_ip = argv[1];
    int server_port = atoi(argv[2]);

    int client_sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];

    client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock < 0) {
        perror("Error creating socket");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        printf("Invalid IP address\n");
        close(client_sock);
        exit(1);
    }

    if (connect(client_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error connecting to server");
        close(client_sock);
        exit(1);
    }

    printf("Connected to server %s:%d\n", server_ip, server_port);

    char recv_buffer[BUFFER_SIZE];
    memset(recv_buffer, 0, BUFFER_SIZE);
    
    int bytes_received = recv(client_sock, recv_buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received > 0) {
        printf("%s\n", recv_buffer);
    } else if (bytes_received == 0) {
        printf("Server closed the connection.\n");
    } else {
        perror("Error receiving greeting from server");
    }

    while (1) {
        printf("> ");
        if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
            break;
        }

        int sent_bytes = send(client_sock, buffer, strlen(buffer), 0);
        if (sent_bytes < 0) {
            perror("Error sending data");
            break;
        }
    }
    close(client_sock);
    return 0;
}