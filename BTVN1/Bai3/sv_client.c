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
        printf("Sử dụng: %s <Địa chỉ IP> <Cổng>\n", argv[0]);
        exit(1);
    }

    char *server_ip = argv[1];
    int server_port = atoi(argv[2]);
    int client_sock;
    struct sockaddr_in server_addr;

    client_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (client_sock < 0) {
        perror("Lỗi tạo socket");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        printf("Địa chỉ IP không hợp lệ!\n");
        close(client_sock);
        exit(1);
    }

    if (connect(client_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Kết nối tới server thất bại");
        close(client_sock);
        exit(1);
    }

    printf("Đã kết nối tới Server %s:%d thành công!\n", server_ip, server_port);
    printf("--- NHẬP THÔNG TIN SINH VIÊN (Nhấn Enter ở MSSV để thoát) ---\n");

    SinhVien sv;
    char buffer[100];

    while (1) {
        printf("\n1. MSSV: ");
        fgets(sv.mssv, sizeof(sv.mssv), stdin);
        remove_newline(sv.mssv);

        if (strlen(sv.mssv) == 0) {
            printf("Kết thúc quá trình nhập.\n");
            break;
        }

        printf("2. Họ và tên: ");
        fgets(sv.ho_ten, sizeof(sv.ho_ten), stdin);
        remove_newline(sv.ho_ten);

        printf("3. Ngày sinh (DD/MM/YYYY): ");
        fgets(sv.ngay_sinh, sizeof(sv.ngay_sinh), stdin);
        remove_newline(sv.ngay_sinh);

        printf("4. Điểm trung bình: ");
        fgets(buffer, sizeof(buffer), stdin);
        sv.diem_tb = atof(buffer);

        int bytes_sent = send(client_sock, &sv, sizeof(SinhVien), 0);
        if (bytes_sent < 0) {
            perror("Lỗi khi gửi dữ liệu");
            break;
        }
        
        printf("-> Đã đóng gói và gửi thông tin sinh viên '%s' tới Server!\n", sv.mssv);
    }

    close(client_sock);
    printf("Đã đóng kết nối.\n");
    return 0;
}