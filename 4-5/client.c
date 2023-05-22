#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <semaphore.h>
#include <errno.h>
#include <sys/wait.h>
#include <time.h>

int sock;
struct sockaddr_in addr;
socklen_t addr_size;
char buffer[1024];

void send_message() {
    if (send(sock, buffer, strlen(buffer), 0) != strlen(buffer)) {
        perror("send() failed");
        exit(1);
    }
}

void receive_message() {
    bzero(buffer, 1024);
    if (recv(sock, buffer, sizeof(buffer), 0) < 0) {
        perror("recv() failed");
        exit(1);
    }
}

void enter_room() {
    srand(getpid() % 47);
    int gender = ((rand()) % 2) + 2; // 2 - женщина, 3 - мужчина

    sleep(rand() % 3);

    bzero(buffer, 1024);
    sprintf(buffer, "%d", gender);
    send_message();
    receive_message();
    if (atoi(buffer) > 0) {
        printf("Entered room #%s.\n", buffer);
    } else {
        printf("Could not enter a room.\n");
    }
    bzero(buffer, 1024);
    sprintf(buffer, "exit");
    send_message();
    exit(0);
}


int main(int argc, char *argv[]) {
    char *ip = argv[1]; // ip сервера (например "127.0.0.1")
    int port = atoi(argv[2]); // номер порта (5567 например)

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
    //printf("client connected to the server\n");

    bzero(buffer, 1024);
    strcpy(buffer, "enter");
    send_message();
    //printf("sent: %s\n", buffer);

    bzero(buffer, 1024);
    receive_message();
    //printf("received: '%s'\n", buffer);
    if (strcmp(buffer, "enter") == 0) {
        enter_room();
    }
    close(sock);
    exit(0);
}
