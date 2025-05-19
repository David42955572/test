#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "protocol.h"

#define MAIN_PORT 8080
#define PORT_RANGE_START 50000
#define PORT_RANGE_END 51000
#define MAX_CLIENTS (PORT_RANGE_END - PORT_RANGE_START)

typedef struct {
    int port;
    int in_use;
} PortEntry;

PortEntry port_table[MAX_CLIENTS];

void init_port_table() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        port_table[i].port = PORT_RANGE_START + i;
        port_table[i].in_use = 0;
    }
}

int allocate_port() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!port_table[i].in_use) {
            port_table[i].in_use = 1;
            return port_table[i].port;
        }
    }
    return -1;
}

void release_port(int port) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (port_table[i].port == port) {
            port_table[i].in_use = 0;
            break;
        }
    }
}

void *handle_dynamic_port(void *arg) {
    int dynamic_socket = *(int *)arg;
    free(arg);

    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    int client_socket = accept(dynamic_socket, (struct sockaddr *)&client_addr, &addr_len);
    if (client_socket < 0) {
        perror("accept 失敗");
        close(dynamic_socket);
        return NULL;
    }

    uint8_t buffer[MAX_DATA_SIZE];
    int bytes_received = recv(client_socket, buffer, MAX_DATA_SIZE, 0);
    if (bytes_received <= 0) {
        close(client_socket);
        return NULL;
    }

    ProtocolHeader header;
    if (parse_header(buffer, &header) == -1) {
        fprintf(stderr, "協議解析失敗\n");
        close(client_socket);
        return NULL;
    }

    printf("接收到資料 - Operation: %d, Status: %d, Username: %s, Length: %d\n", 
           header.operation, header.status, header.username, header.length);

    uint8_t data[MAX_DATA_SIZE + 1];
    parse_data(buffer + 3 + header.username_len + 4, header.length, data);
    printf("接收到數據: %s\n", data);

    const char *response = "資料接收成功";
    uint8_t send_buffer[2048];
    int send_len = pack_message(1, 0, header.username, 0, (const uint8_t *)response, strlen(response), send_buffer);
    send(client_socket, send_buffer, send_len, 0);

    close(client_socket);
    close(dynamic_socket);
    return NULL;
}

void *handle_main_port(void *arg) {
    int server_fd = *(int *)arg;
    struct sockaddr_in address;
    int addrlen = sizeof(address);

    while (1) {
        int client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen);
        if (client_socket < 0) {
            perror("接受連接失敗");
            continue;
        }

        uint8_t buffer[MAX_DATA_SIZE];
        int bytes_received = recv(client_socket, buffer, MAX_DATA_SIZE, 0);
        if (bytes_received <= 0) {
            close(client_socket);
            continue;
        }

        ProtocolHeader header;
        if (parse_header(buffer, &header) == -1 || header.operation != 0) {
            fprintf(stderr, "協議解析失敗或操作碼錯誤\n");
            close(client_socket);
            continue;
        }

        int allocated_port = allocate_port();
        if (allocated_port == -1) {
            fprintf(stderr, "無可用 port\n");
            close(client_socket);
            continue;
        }

        printf("分配 port %d 給新的客戶端\n", allocated_port);

        char port_str[6];
        snprintf(port_str, sizeof(port_str), "%d", allocated_port);

        uint8_t send_buffer[2048];
        int send_len = pack_message(0, 0, header.username, 0, (const uint8_t *)port_str, strlen(port_str), send_buffer);
        send(client_socket, send_buffer, send_len, 0);
        close(client_socket);

        // 建立新的 socket
        int dynamic_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (dynamic_socket < 0) {
            perror("建立動態 socket 失敗");
            // 釋放 port
            release_port(allocated_port);
            close(client_socket);
            continue;
        }

        // 設定動態 port 的 sockaddr
        struct sockaddr_in dynamic_addr;
        dynamic_addr.sin_family = AF_INET;
        dynamic_addr.sin_addr.s_addr = INADDR_ANY;
        dynamic_addr.sin_port = htons(allocated_port);

        if (bind(dynamic_socket, (struct sockaddr *)&dynamic_addr, sizeof(dynamic_addr)) < 0) {
            perror("bind 動態 port 失敗");
            release_port(allocated_port);
            close(dynamic_socket);
            close(client_socket);
            continue;
        }

        if (listen(dynamic_socket, 3) < 0) {
            perror("listen 動態 port 失敗");
            release_port(allocated_port);
            close(dynamic_socket);
            close(client_socket);
            continue;
        }

        // 把動態 socket 傳給執行緒
        int *new_socket = malloc(sizeof(int));
        *new_socket = dynamic_socket;
        pthread_create(&tid, NULL, handle_dynamic_port, new_socket);
        pthread_detach(tid);

    }

    return NULL;
}

int main() {
    init_port_table();

    int main_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (main_socket == 0) {
        perror("建立 socket 失敗");
        exit(EXIT_FAILURE);
    }

    int opt = 1;
    setsockopt(main_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(MAIN_PORT);

    if (bind(main_socket, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("綁定失敗");
        exit(EXIT_FAILURE);
    }

    if (listen(main_socket, 3) < 0) {
        perror("監聽失敗");
        exit(EXIT_FAILURE);
    }

    printf("主執行序啟動於 port %d\n", MAIN_PORT);

    pthread_t main_thread;
    pthread_create(&main_thread, NULL, handle_main_port, &main_socket);
    pthread_join(main_thread, NULL);

    return 0;
}
