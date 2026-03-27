#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/stat.h>

int main(int argc, char *argv[]){
    if(argc != 3){
        printf("Sử dụng: %s <server_ip> <server_port>\n", argv[0]);
        return 1;
    }
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock < 0){
        perror("Lỗi tạo socket");
        return 1;
    }
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[2]));
    inet_pton(AF_INET, argv[1], &server_addr.sin_addr);

    if(connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
        perror("Lỗi kết nối đến server");
        close(sock);
        return 1;
    }
    char cwd[1024];
    if(getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("Lỗi lấy đường dẫn thư mục");
        close(sock);
        return 1;
    }
    DIR *dir = opendir(cwd);
    if(dir == NULL){
        perror("Lỗi mở thư mục");
        close(sock);
        return 1;
    }
    char buffer[65536]; 
    int offset = 0;     
    int cwd_len = strlen(cwd); 
    memcpy(buffer + offset, &cwd_len, sizeof(int));
    offset += sizeof(int);
    memcpy(buffer + offset, cwd, cwd_len);
    offset += cwd_len;
    struct dirent *entry;
    struct stat st;
    char filepath[2048];

    while((entry = readdir(dir)) != NULL){
        if(entry->d_type == DT_REG){ 
            snprintf(filepath, sizeof(filepath), "%s/%s", cwd, entry->d_name);
            
            if (stat(filepath, &st) == 0) {
                int filename_len = strlen(entry->d_name);
                if (offset + sizeof(int) + filename_len + sizeof(long long) > sizeof(buffer)) {
                    send(sock, buffer, offset, 0);
                    offset = 0; 
                }
                memcpy(buffer + offset, &filename_len, sizeof(int));
                offset += sizeof(int);
                memcpy(buffer + offset, entry->d_name, filename_len);
                offset += filename_len;
                long long filesize = st.st_size;
                memcpy(buffer + offset, &filesize, sizeof(long long));
                offset += sizeof(long long);
            }
        }
    }

    closedir(dir);
    if (offset > 0) {
        send(sock, buffer, offset, 0);
    }

    close(sock);
    return 0;
}