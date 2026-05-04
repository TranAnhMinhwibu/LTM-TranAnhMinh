#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#define BUF_SIZE 1024

int main(int argc, char *argv[]) {

    int local_port = atoi(argv[1]);
    char *remote_ip = argv[2];
    int remote_port = atoi(argv[3]);

    int sockfd;
    struct sockaddr_in local_addr, remote_addr;
    char buffer[BUF_SIZE];

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = INADDR_ANY;
    local_addr.sin_port = htons(local_port);

    if (bind(sockfd, (const struct sockaddr *)&local_addr, sizeof(local_addr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    memset(&remote_addr, 0, sizeof(remote_addr));
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(remote_port);
    if (inet_pton(AF_INET, remote_ip, &remote_addr.sin_addr) <= 0) {
        fprintf(stderr, "Invalid IP address\n");
        exit(EXIT_FAILURE);
    }

    printf("using port %d\n", local_port);


    fd_set readfds;
    while (1) {
        FD_ZERO(&readfds);

        FD_SET(STDIN_FILENO, &readfds);
        FD_SET(sockfd, &readfds);

        int activity = select(sockfd + 1, &readfds, NULL, NULL, NULL);

        if (activity < 0) {
            perror("select failed");
            break;
        }
        if (FD_ISSET(STDIN_FILENO, &readfds)) {
            if (fgets(buffer, BUF_SIZE, stdin) != NULL) {
                sendto(sockfd, buffer, strlen(buffer), 0,
                       (struct sockaddr *)&remote_addr, sizeof(remote_addr));
            }
        }
        if (FD_ISSET(sockfd, &readfds)) {
            struct sockaddr_in sender_addr;
            socklen_t addr_len = sizeof(sender_addr);
            int n = recvfrom(sockfd, buffer, BUF_SIZE - 1, 0,
                             (struct sockaddr *)&sender_addr, &addr_len);
            
            if (n > 0) {
                buffer[n] = '\0';
                printf("Partner: %s", buffer);
            }
        }
    }

    close(sockfd);
    return 0;
}