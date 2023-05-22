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
#include <sys/mman.h>
#include <signal.h>

int CLIENTS_NUM = 55;
int server_socket;
char buffer[1024];

// Общая память - список комнат
char rooms[] = "rooms";
int rooms_fd;
int double_rooms = 15;
int size = 25;

// Семафор для посетителей отеля
const char *clients_sem = "/clients-semaphore";
sem_t *clients;

void init_memory(int shm) {
    char *addr;
    char buf[25];
    bzero(buf, 25);

    addr = mmap(0, size, PROT_WRITE|PROT_READ, MAP_SHARED, shm, 0);
    if (addr == (char*)-1 ) {
        printf("Error getting pointer to shared memory\n");
        return;
    }

    memcpy(addr, buf, size);
    close(shm);
}

// Функция осуществляющая при запуске общие манипуляции с памятью и семафорами
void init(void) {
    // Создание или открытие мьютекса для доступа к буферу (доступ открыт)
    int fd_clients = shm_open("clients_sem", O_RDWR | O_CREAT, 0666);
    ftruncate(fd_clients, sizeof(sem_t));
    clients = mmap(0, sizeof(sem_t), PROT_WRITE|PROT_READ, MAP_SHARED, fd_clients, 0);
    sem_init(clients, 1, 1);

    if ((rooms_fd = shm_open(rooms, O_CREAT|O_RDWR, 0666)) == -1 ) {
        printf("Opening error.\n");
        perror("shm_open");
        exit(-1);
    }

    if (ftruncate(rooms_fd, size) == -1) {
        printf("Truncating error.\n");
        perror("ftruncate");
        exit(-1);
    } else {
        init_memory(rooms_fd);
        printf("Hotel with %d rooms opened!\n", size);
    }

    close(rooms_fd);
}

void unlink_all(void) {
    if(shm_unlink(rooms) == -1) {
        printf("Shared memory is absent\n");
        perror("shm_unlink");
    }

    sem_destroy(clients);
}

void send_message(int socket) {
    if (send(socket, buffer, strlen(buffer), 0) != strlen(buffer)) {
        perror("send error: ");
        exit(1);
    }
}

void receive_message(int socket) {
    bzero(buffer, sizeof(buffer));
    if (recv(socket, buffer, 1024, 0) < 0) {
        perror("recv error: ");
        exit(1);
    }
}

int accept_connection(struct sockaddr_in client_addr, socklen_t addr_size) {
    int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &addr_size);
    if (client_socket < 0) {
        perror("error connecting to client: ");
        exit(1);
    }
    printf("client connected\n");
    return client_socket;
}

int check_rooms(int gender, int num) {
    char *addr;
    int shm;
    if ((shm = shm_open(rooms, O_RDWR, 0666)) == -1 ) {
        printf("Opening error\n");
        perror("shm_open");
        return -1;
    }

    addr = mmap(0, size, PROT_WRITE|PROT_READ, MAP_SHARED, shm, 0);
    if (addr == (char*)-1 ) {
        printf("Error getting pointer to shared memory\n");
        return -1;
    }

    printf("A %s #%d came to the hotel...\n", (gender == 2 ? "woman" : "man"), num);

    sem_wait(clients);

    char result = (char)gender;
    for (int i = 0; i < size; ++i) {
        if (i < double_rooms && (addr[i] == gender || addr[i] == 0)) {
            if (addr[i] == gender) {
                result *= 2;
            }
            addr[i] = result;
            close(shm);
            printf("A %s #%d entered double room #%d...\n", (gender == 2 ? "woman" : "man"), num, i + 1);
            sem_post(clients);
            return i + 1;
        }
        if (i >= double_rooms && addr[i] == 0) {
            addr[i] = result;
            close(shm);
            printf("A %s #%d entered single room #%d...\n", (gender == 2 ? "woman" : "man"), num, i + 1);
            sem_post(clients);
            return i + 1;
        }
    }

    sem_post(clients);

    printf("A %s #%d left hotel\n", (gender == 2 ? "woman" : "man"), num);
    close(shm);
    return 0;
}

int main(int argc, char *argv[]) {
    unlink_all();
    char *ip = argv[1];
    int port = atoi(argv[2]);

    int client_socket;
    //int view_sock;
    struct sockaddr_in server_addr, client_addr, view_addr;
    socklen_t addr_size;

    init();

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("socket error: ");
        exit(1);
    }
    printf("server ip: %s\n", ip);
    printf("TCP server socket created\n");

    memset(&server_addr, '\0', sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip);

    int bind_check = bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr));
    if (bind_check < 0) {
        perror("bind error: ");
        exit(1);
    }

    listen(server_socket, CLIENTS_NUM);

    pid_t childpid;
    for (int i = 1; i <= CLIENTS_NUM; ++i) {
        client_socket = accept_connection(client_addr, addr_size);

        if((childpid = fork()) == 0) {
            close(server_socket);
            receive_message(client_socket);
            if (strcmp(buffer, "enter") == 0) {
                send_message(client_socket);
                receive_message(client_socket);
                sprintf(buffer, "%d", check_rooms(atoi(buffer), i));
                send_message(client_socket);
            }
            receive_message(client_socket);
            if (strcmp(buffer, "exit") == 0) {
                printf("client disconnected\n");
            }
            exit(0);
        }
    }

    fd_set rfds;
    struct timeval tv;
    int retval;
    FD_ZERO(&rfds);
    FD_SET(0, &rfds);
    tv.tv_sec = 10;
    tv.tv_usec = 0;
    retval = select(1, &rfds, NULL, NULL, &tv);
    if (retval) {
        printf("\nKilling child processes.\n");
        signal(SIGQUIT, SIG_IGN);
        kill(0, SIGQUIT);
        printf("All child processes were killed.\n");
    } else {
        while(wait(NULL) > 0);
        printf("\nChild processes exited by their own.\n");
    }
    unlink_all();
    close(client_socket);
    return 0;
}
