#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <errno.h>

#define MAX_CLIENTS 64

typedef struct {
    int fd;
    int state;
    char name[256];
} ClientState;

void generate_hust_email(const char* full_name, const char* mssv, char* email) {
    char name_copy[256];
    strncpy(name_copy, full_name, sizeof(name_copy) - 1);
    name_copy[255] = '\0';

    for(int i = 0; name_copy[i]; i++) {
        if(name_copy[i] >= 'A' && name_copy[i] <= 'Z') name_copy[i] += 32;
    }

    char *words[20];
    int word_count = 0;
    char *token = strtok(name_copy, " \r\n\t");
    while(token != NULL && word_count < 20) {
        words[word_count++] = token;
        token = strtok(NULL, " \r\n\t");
    }

    if(word_count == 0) {
        strcpy(email, "invalid@sis.hust.edu.vn");
        return;
    }

    char prefix[256] = {0};
    strcpy(prefix, words[word_count - 1]);
    strcat(prefix, ".");

    for(int i = 0; i < word_count - 1; i++) {
        int len = strlen(prefix);
        prefix[len] = words[i][0];
        prefix[len+1] = '\0';
    }

    char clean_mssv[64] = {0};
    int j = 0;
    for(int i = 0; mssv[i] && j < 63; i++) {
        if(mssv[i] >= '0' && mssv[i] <= '9') {
            clean_mssv[j++] = mssv[i];
        }
    }
    clean_mssv[j] = '\0';

    int mssv_len = strlen(clean_mssv);
    const char* mssv_part = clean_mssv;
    if (mssv_len >= 6) mssv_part = clean_mssv + (mssv_len - 6); 

    sprintf(email, "%s%s@sis.hust.edu.vn", prefix, mssv_part);
}

int main() {
    int listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == -1) {
        perror("socket() failed");
        return 1;
    }

    unsigned long ul = 1;
    ioctl(listener, FIONBIO, &ul);

    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int))) {
        perror("setsockopt() failed");
        close(listener);
        return 1;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(8080);

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr))) {
        perror("bind() failed");
        close(listener);
        return 1;
    }

    if (listen(listener, 5)) {
        perror("listen() failed");
        close(listener);
        return 1;
    }

    printf("Server is listening on port 8080...\n");

    ClientState clients[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        clients[i].state = 0;
    }
    
    int max_index = 0;
    char buf[1024];

    while (1) {
        while (1) {
            int client = accept(listener, NULL, NULL);
            if (client == -1) {
                if (errno == EWOULDBLOCK || errno == EAGAIN) {
                    break;
                } else {
                    break;
                }
            }

            printf("New client accepted: %d\n", client);

            int free_slot = -1;
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clients[i].state == 0) {
                    free_slot = i;
                    break;
                }
            }

            if (free_slot != -1) {
                clients[free_slot].fd = client;
                clients[free_slot].state = 1;
                memset(clients[free_slot].name, 0, sizeof(clients[free_slot].name));
                
                ul = 1;
                ioctl(client, FIONBIO, &ul);

                char *msg = "Please enter your Full Name: ";
                send(client, msg, strlen(msg), 0);

                if (free_slot >= max_index) {
                    max_index = free_slot + 1;
                }
            } else {
                close(client); 
            }
        }


        for (int i = 0; i < max_index; i++) {
            if (clients[i].state == 0) continue;

            int len = recv(clients[i].fd, buf, sizeof(buf) - 1, 0);
            
            if (len == -1) {
                if (errno == EWOULDBLOCK || errno == EAGAIN) {
                    continue; 
                } else {
                    close(clients[i].fd);
                    clients[i].state = 0;
                }
            } else if (len == 0) {
                close(clients[i].fd);
                clients[i].state = 0; 
            } else {
                buf[len] = '\0';
                buf[strcspn(buf, "\r\n")] = 0; 
                if (strlen(buf) == 0) continue;

                if (clients[i].state == 1) {
                    strncpy(clients[i].name, buf, 255);
                    clients[i].state = 2;
                    
                    char *msg = "Please enter your Student ID: ";
                    send(clients[i].fd, msg, strlen(msg), 0);
                } 
                else if (clients[i].state == 2) {
                    char email[256];
                    generate_hust_email(clients[i].name, buf, email);
                    
                    char response[512];
                    sprintf(response, "-> Email: %s\n", email);
                    send(clients[i].fd, response, strlen(response), 0);

                    close(clients[i].fd);
                    clients[i].state = 0;
                }
            }
        }

        usleep(10000); 
    }

    close(listener);
    return 0;
}