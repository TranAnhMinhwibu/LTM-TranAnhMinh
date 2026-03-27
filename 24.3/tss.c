#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdint.h>

int main(int argc, char *argv[]){
    if(argc != 3){
        printf("Usage: %s <server_ip> <server_port>\n", argv[0]);
        return 1;
    }

    
}