#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

typedef struct {
    char mssv[20];
    char ho_ten[50];
    char ngay_sinh[20];
    float diem_tb;
} SinhVien;

void remove_newline(char *str) {
    int len = strlen(str);
    if (len > 0 && str[len - 1] == '\n') {
        str[len - 1] = '\0';
    }
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <Server IP> <Port>\n", argv[0]);
        exit(1);
    }

    char *server_ip = argv[1];
    int server_port = atoi(argv[2]);
    int client_sock;
    struct sockaddr_in server_addr;

    client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        printf("Invalid IP address\n");
        exit(1);
    }

    if (connect(client_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        exit(1);
    }

    printf("Connected to server %s:%d\n", server_ip, server_port);
    printf("Enter student information (Leave Student ID empty to exit):\n");

    SinhVien sv;
    char buffer[50];

    while (1) {
        printf("Student ID: ");
        fgets(sv.mssv, sizeof(sv.mssv), stdin);
        remove_newline(sv.mssv);
        if (strlen(sv.mssv) == 0) break;

        printf("Name: ");
        fgets(sv.ho_ten, sizeof(sv.ho_ten), stdin);
        remove_newline(sv.ho_ten);

        printf("DOB (YYYY-MM-DD): ");
        fgets(sv.ngay_sinh, sizeof(sv.ngay_sinh), stdin);
        remove_newline(sv.ngay_sinh);

        printf("GPA: ");
        fgets(buffer, sizeof(buffer), stdin);
        sv.diem_tb = atof(buffer);

        if (send(client_sock, &sv, sizeof(SinhVien), 0) < 0) {
            perror("Send failed");
            break;
        }
    }

    close(client_sock);
    return 0;
}