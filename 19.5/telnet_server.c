#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>

int read_line(int sock, char *buf, int size) {
    int i = 0;
    unsigned char c;
    while (i < size - 1) {
        int n = recv(sock, &c, 1, 0);
        if (n <= 0) return n;

        if (c == 255) {
            unsigned char temp;
            recv(sock, &temp, 1, 0);
            recv(sock, &temp, 1, 0);
            continue;
        }

        if (c == '\r') continue;
        if (c == '\n') break;

        buf[i++] = c;
    }
    buf[i] = '\0';
    return i;
}

int check_login(const char *user, const char *pass) {
    FILE *f = fopen("database.txt", "r");
    if (!f) return 0;
    
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char db_u[100], db_p[100];
        if (sscanf(line, "%s %s", db_u, db_p) == 2) {
            if (strcmp(user, db_u) == 0 && strcmp(pass, db_p) == 0) {
                fclose(f);
                return 1;
            }
        }
    }
    fclose(f);
    return 0;
}

void *handle_client(void *arg) {
    int client_fd = *(int *)arg;
    free(arg);

    char user[100];
    char pass[100];
    int authenticated = 0;

    while (!authenticated) {
        send(client_fd, "Username: ", 10, 0);
        memset(user, 0, sizeof(user));
        if (read_line(client_fd, user, sizeof(user)) <= 0) break;
        if (strlen(user) == 0) continue;

        send(client_fd, "Password: ", 10, 0);
        memset(pass, 0, sizeof(pass));
        if (read_line(client_fd, pass, sizeof(pass)) <= 0) break;

        if (check_login(user, pass)) {
            send(client_fd, "Login successful.\n", 18, 0);
            authenticated = 1;
        } else {
            send(client_fd, "Login failed. Try again.\n", 25, 0);
        }
    }

    if (!authenticated) {
        close(client_fd);
        return NULL;
    }

    char cmd[256];
    char sys_cmd[512];
    char out_file[64];
    snprintf(out_file, sizeof(out_file), "out_%d.txt", client_fd);

    while (1) {
        send(client_fd, "> ", 2, 0);
        memset(cmd, 0, sizeof(cmd));
        if (read_line(client_fd, cmd, sizeof(cmd)) <= 0) break;
        if (strlen(cmd) == 0) continue;

        snprintf(sys_cmd, sizeof(sys_cmd), "%s > %s 2>&1", cmd, out_file);
        system(sys_cmd);

        FILE *f = fopen(out_file, "r");
        if (f) {
            char buf[1024];
            size_t n;
            while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
                send(client_fd, buf, n, 0);
            }
            fclose(f);
            remove(out_file);
        }
    }

    close(client_fd);
    return NULL;
}

int main() {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(8080);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        printf("Error: Port 8080 is already in use.\n");
        exit(1);
    }

    if (listen(server_fd, 10) < 0) exit(1);

    printf("Telnet Server (Multithreaded) listening on port 8080\n");

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int *client_fd = malloc(sizeof(int));
        *client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);

        if (*client_fd < 0) {
            free(client_fd);
            continue;
        }

        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, client_fd);
        pthread_detach(tid);
    }

    return 0;
}