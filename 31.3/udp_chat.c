#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#define MAX_LEN 1024

int main(int argc, char *argv[]) {
    if (argc != 4) {
        printf("Usage: %s <port_s> <ip_d> <port_d>\n", argv[0]);
        printf("Example: %s 8080 127.0.0.1 9090\n", argv[0]);
        return 1;
    }

    int port_s = atoi(argv[1]);
    char *ip_d = argv[2];
    int port_d = atoi(argv[3]);

    int sockfd;
    struct sockaddr_in my_addr, dest_addr;
    char buffer[MAX_LEN];

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("Socket creation error");
        return 1;
    }

    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_addr.s_addr = INADDR_ANY;
    my_addr.sin_port = htons(port_s);

    if (bind(sockfd, (const struct sockaddr *)&my_addr, sizeof(my_addr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        return 1;
    }

    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port_d);
    if (inet_pton(AF_INET, ip_d, &dest_addr.sin_addr) <= 0) {
        perror("Invalid destination IP address");
        close(sockfd);
        return 1;
    }

    printf("=== UDP CHAT ===\n");
    printf("- Listening on port: %d\n", port_s);
    printf("- Ready to send to IP: %s, Port: %d\n", ip_d, port_d);
    printf("----------------------------------------\n");
    printf("Type a message and press Enter to send (Type 'exit' to quit)...\n\n");

    fd_set readfds;
    int max_fd = sockfd > STDIN_FILENO ? sockfd : STDIN_FILENO;

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(sockfd, &readfds);

        int activity = select(max_fd + 1, &readfds, NULL, NULL, NULL);

        if (activity < 0) {
            perror("Select error");
            break;
        }

        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            memset(buffer, 0, MAX_LEN);
            if (fgets(buffer, MAX_LEN, stdin) != NULL) {
                buffer[strcspn(buffer, "\n")] = 0;

                if (strcmp(buffer, "exit") == 0) {
                    printf("Chat ended.\n");
                    break;
                }

                if (strlen(buffer) > 0) {
                    sendto(sockfd, buffer, strlen(buffer), 0,
                           (const struct sockaddr *)&dest_addr, sizeof(dest_addr));
                }
            }
        }

        if (FD_ISSET(sockfd, &readfds)) {
            memset(buffer, 0, MAX_LEN);
            struct sockaddr_in sender_addr;
            socklen_t sender_len = sizeof(sender_addr);
            
            int n = recvfrom(sockfd, buffer, MAX_LEN - 1, 0,
                             (struct sockaddr *)&sender_addr, &sender_len);
            
            if (n > 0) {
                buffer[n] = '\0';
                printf("\n[Message from %s:%d]: %s\n> ", 
                       inet_ntoa(sender_addr.sin_addr), 
                       ntohs(sender_addr.sin_port), 
                       buffer);
                fflush(stdout); 
            }
        }
    }

    close(sockfd);
    return 0;
}