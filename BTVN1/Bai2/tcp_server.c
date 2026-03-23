#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define BUFFER_SIZE 1024

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Using: %s <Port> <Hello Message> <data file>\n", argv[0]);
        exit(1);
    }

    int server_port = atoi(argv[1]);
    char *greeting_file = argv[2];
    char *output_file = argv[3];

    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    char buffer[BUFFER_SIZE];

    FILE *f_greet = fopen(greeting_file, "r");
    if (!f_greet) {
        perror("Error opening greeting file");
        exit(1);
    }
    char greeting[BUFFER_SIZE];
    size_t greet_len = fread(greeting, 1, sizeof(greeting) - 1, f_greet);
    greeting[greet_len] = '\0';
    fclose(f_greet);

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Error creating socket");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(server_port);

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error binding socket");
        close(server_sock);
        exit(1);
    }

    if (listen(server_sock, 5) < 0) {
        perror("Error listening");
        close(server_sock);
        exit(1);
    }

    printf("Server listening on port %d...\n", server_port);

    while (1) {
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_sock < 0) {
            perror("Error accepting connection");
            continue;
        }

        printf("\n[+] Client connected with IP: %s\n", inet_ntoa(client_addr.sin_addr));

        send(client_sock, greeting, strlen(greeting), 0);

        FILE *f_out = fopen(output_file, "a");
        if (!f_out) {
            perror("Error: Cannot open data file");
            close(client_sock);
            continue;
        }

        int bytes_received;
        while ((bytes_received = recv(client_sock, buffer, BUFFER_SIZE, 0)) > 0) {
            fwrite(buffer, 1, bytes_received, f_out);
            fflush(f_out);
        }

        printf("[-] Client disconnected. data saved to '%s'.\n", output_file);
        fclose(f_out);
        close(client_sock);
    }

    close(server_sock);
    return 0;
}