#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>

void process_request(int client, char *request) {
    char method[16], uri[256], *body;
    sscanf(request, "%s %s", method, uri);

    char op[16] = "";
    double a = 0, b = 0;
    int found = 0;

    if (strcmp(method, "GET") == 0) {
        char *qs = strchr(uri, '?');
        if (qs) {
            if (sscanf(qs + 1, "op=%[^&]&a=%lf&b=%lf", op, &a, &b) == 3) {
                found = 1;
            }
        }
    } 
    else if (strcmp(method, "POST") == 0) {
        body = strstr(request, "\r\n\r\n");
        if (body) {
            if (sscanf(body + 4, "op=%[^&]&a=%lf&b=%lf", op, &a, &b) == 3) {
                found = 1;
            }
        }
    }

    char html[1024];

    if (found) {
        double res = 0;
        char op_char = '?';
        if (strcmp(op, "add") == 0) { res = a + b; op_char = '+'; }
        else if (strcmp(op, "sub") == 0) { res = a - b; op_char = '-'; }
        else if (strcmp(op, "mul") == 0) { res = a * b; op_char = '*'; }
        else if (strcmp(op, "div") == 0 && b != 0) { res = a / b; op_char = '/'; }
        
        snprintf(html, sizeof(html), "<html><body><h1>Ket qua: %g %c %g = %g</h1></body></html>\n", a, op_char, b, res);
    } else {
        snprintf(html, sizeof(html), "<html><body><h2>Loi: Khong thay tham so. Vui long truyen tham so op, a, b.</h2></body></html>\n");
    }

    char response[2048];
    snprintf(response, sizeof(response), "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n%s", html);
    send(client, response, strlen(response), 0);
}

int main() {
    signal(SIGCHLD, SIG_IGN);

    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(8080);
    printf("Server is listening on port 8080...\n");
    
    bind(listener, (struct sockaddr *)&addr, sizeof(addr));
    listen(listener, 10);

    while (1) {
        int client = accept(listener, NULL, NULL);
        if (client < 0) continue;
        
        if (fork() == 0) {
            close(listener);
            char buf[2048] = {0};
            recv(client, buf, sizeof(buf) - 1, 0);
            if (strlen(buf) > 0) {
                process_request(client, buf);
            }
            close(client);
            exit(0);
        }
        close(client);
    }
    return 0;
}

