#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <signal.h>

typedef struct {
    char mssv[20];
    char ho_ten[50];
    char ngay_sinh[20];
    float diem_tb;
} SinhVien;

void get_current_time(char *time_str, size_t max_size) {
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    strftime(time_str, max_size, "%Y-%m-%d %H:%M:%S", tm_info);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <Port> <Log file>\n", argv[0]);
        exit(1);
    }

    signal(SIGCHLD, SIG_IGN);

    int server_port = atoi(argv[1]);
    char *log_file = argv[2];

    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(server_port);

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(1);
    }

    if (listen(server_sock, 5) < 0) {
        perror("Listen failed");
        exit(1);
    }

    printf("Server listening on port %d...\n", server_port);

    while (1) {
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_sock < 0) {
            perror("Accept failed");
            continue;
        }

        char *client_ip = inet_ntoa(client_addr.sin_addr);
        printf("Client connected from IP: %s\n", client_ip);

        if (fork() == 0) {
            close(server_sock); 
            SinhVien sv;
            
            FILE *f_log = fopen(log_file, "a");
            if (!f_log) {
                perror("Log file open failed");
                close(client_sock);
                exit(1);
            }

            int bytes_received;
            while ((bytes_received = recv(client_sock, &sv, sizeof(SinhVien), 0)) > 0) {
                if (bytes_received == sizeof(SinhVien)) {
                    char time_str[64];
                    get_current_time(time_str, sizeof(time_str));
                    
                    printf("Received data from:");
                    printf("%s %s %s %s %s %.2f\n", 
                            client_ip, time_str, sv.mssv, sv.ho_ten, sv.ngay_sinh, sv.diem_tb);

                    fprintf(f_log, "%s %s %s %s %s %.2f\n", 
                            client_ip, time_str, sv.mssv, sv.ho_ten, sv.ngay_sinh, sv.diem_tb);
                    
                    fflush(f_log);
                }
            }

            fclose(f_log);
            close(client_sock);
            exit(0);
        } else {
            close(client_sock);
        }
    }

    close(server_sock);
    return 0;
}