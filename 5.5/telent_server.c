#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <time.h>

struct shared_data {
    char message[2048];
    pid_t sender_pid;
};

struct shared_data *shm;
volatile sig_atomic_t has_new_message = 0;

void sig_handler(int signo) {
    if (signo == SIGUSR1) {
        has_new_message = 1;
    }
}

int main() {
    shm = mmap(NULL, sizeof(struct shared_data), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shm == MAP_FAILED) exit(1);

    signal(SIGUSR1, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8080);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("Error: Failed to bind socket.\n");
        exit(1);
    }

    if (listen(server_fd, 10) < 0) exit(1);

    printf("Server listening on port 8080\n");

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

        if (client_fd < 0) continue;

        pid_t pid = fork();
        
        if (pid == 0) {
            close(server_fd);
            signal(SIGUSR1, SIG_IGN);

            char client_name[100];
            char auth_buf[256];
            
            while (1) {
                send(client_fd, "Please enter id: ", 17, 0);
                
                memset(auth_buf, 0, sizeof(auth_buf));
                int i = 0;
                unsigned char c;
                
                while (i < sizeof(auth_buf) - 1) {
                    int n = recv(client_fd, &c, 1, 0);
                    if (n <= 0) exit(0);
                    if (c == 255) {
                        unsigned char temp;
                        recv(client_fd, &temp, 1, 0);
                        recv(client_fd, &temp, 1, 0);
                        continue;
                    }
                    if (c == '\r') continue;
                    if (c == '\n') break;
                    auth_buf[i++] = c;
                }
                auth_buf[i] = '\0';
                
                if (strlen(auth_buf) == 0) continue;

                if (strncmp(auth_buf, "client_id: ", 11) == 0 && strlen(auth_buf) > 11) {
                    strcpy(client_name, auth_buf + 11);
                    send(client_fd, "Accepted\n", 9, 0);
                    break;
                }
            }
            struct sigaction sa;
            sa.sa_handler = sig_handler;
            sigemptyset(&sa.sa_mask);
            sa.sa_flags = 0; 
            sigaction(SIGUSR1, &sa, NULL);

            while (1) {
                char msg_buf[1024];
                memset(msg_buf, 0, sizeof(msg_buf));
                int i = 0;
                unsigned char c;
                
                while (i < sizeof(msg_buf) - 1) {
                    int n = recv(client_fd, &c, 1, 0);
                    if (n < 0) {
                        if (errno == EINTR) {
                            if (has_new_message) {
                                if (shm->sender_pid != getpid()) {
                                    send(client_fd, shm->message, strlen(shm->message), 0);
                                }
                                has_new_message = 0;
                            }
                            continue;
                        }
                        exit(0);
                    }
                    if (n == 0) exit(0);
                    
                    if (c == 255) {
                        unsigned char temp;
                        recv(client_fd, &temp, 1, 0);
                        recv(client_fd, &temp, 1, 0);
                        continue;
                    }
                    if (c == '\r') continue;
                    if (c == '\n') break;
                    msg_buf[i++] = c;
                }
                msg_buf[i] = '\0';

                if (strlen(msg_buf) == 0) continue;

                time_t t = time(NULL);
                struct tm *tm_info = localtime(&t);
                char time_str[30];
                strftime(time_str, 30, "%Y/%m/%d %I:%M:%S%p", tm_info);

                char full_msg[2048];
                snprintf(full_msg, sizeof(full_msg), "%s %s: %s\n", time_str, client_name, msg_buf);

                strcpy(shm->message, full_msg);
                shm->sender_pid = getpid();
                kill(0, SIGUSR1);
            }
        } else {
            close(client_fd);
        }
    }
    return 0;
}