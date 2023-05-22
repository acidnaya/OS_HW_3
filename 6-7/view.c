#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char buffer[1024];
int sock;

void receive_message() {
    bzero(buffer, 1024);
    if (recv(sock, buffer, sizeof(buffer), 0) < 0) {
        perror("recv() failed");
        exit(1);
    }
}

int main(int argc, char *argv[]) {
    char *ip = argv[1];
    int port = atoi(argv[2]);
    struct sockaddr_in addr;
    socklen_t addr_size;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket error: ");
        exit(1);
    }

    memset(&addr, '\0', sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(ip);
    connect(sock, (struct sockaddr*)&addr, sizeof(addr));

    printf("Hotel is opened.\n");

    while (1) {
        receive_message();
        if (strcmp(buffer, "exit") == 0) {
            break;
        }
        printf("%s", buffer);
    }
    printf("Hotel is closed.\n");
    close(sock);
    exit(0);
}