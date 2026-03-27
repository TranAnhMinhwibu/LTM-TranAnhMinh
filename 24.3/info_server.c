#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

int main(int argc, char *argv[]) {
    if(argc != 2){
        printf("Sử dụng: %s <port>\n", argv[0]);
        return 1;
    }
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if(server_sock < 0){
        perror("Lỗi tạo socket");
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[1]));
    server_addr.sin_addr.s_addr = INADDR_ANY; 

    if(bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
        perror("Lỗi bind");
        return 1;
    }

    if(listen(server_sock, 5) < 0){
        perror("Lỗi listen");
        return 1;
    }

    printf("Server đang lắng nghe trên cổng %s...\n", argv[1]);
    while(1){
        int client_sock = accept(server_sock, NULL, NULL);
        if(client_sock < 0){
            perror("Lỗi accept");
            continue;
        }

        printf("\nCó Client mới kết nối.\n");
        int cwd_len;
        if (recv(client_sock, &cwd_len, sizeof(int), MSG_WAITALL) <= 0) {
            close(client_sock);
            continue; 
        }
        if (cwd_len < 0 || cwd_len >= 2048) {
            printf("Lỗi: Độ dài thư mục không hợp lệ.\n");
            close(client_sock);
            continue;
        }
        char cwd[2048];
        if (recv(client_sock, cwd, cwd_len, MSG_WAITALL) <= 0) {
            close(client_sock);
            continue;
        }
        cwd[cwd_len] = '\0'; 
        
        printf("Thư mục hiện tại: %s\n", cwd);
        while (1) {
            int filename_len;
            int bytes_read = recv(client_sock, &filename_len, sizeof(int), MSG_WAITALL);
            
            if (bytes_read <= 0) break; 
            if (filename_len < 0 || filename_len >= 1024) {
                printf("Lỗi: Độ dài tên file không hợp lệ, ngắt kết nối.\n");
                break;
            }
            char filename[1024];
            if (recv(client_sock, filename, filename_len, MSG_WAITALL) <= 0) break;
            filename[filename_len] = '\0'; 
            long long filesize;
            if (recv(client_sock, &filesize, sizeof(long long), MSG_WAITALL) <= 0) break;
            printf(" - %s - %lld bytes\n", filename, filesize);
        }

        printf("Client đã ngắt kết nối.\n");
        close(client_sock);
    }

    close(server_sock);
    return 0;
}