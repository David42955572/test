#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "protocol.h"

#define PORT 8080
#define BUFFER_SIZE 2048

void handle_client(int client_socket) {
    uint8_t buffer[BUFFER_SIZE];
    int bytes_received;

    // 接收資料
    bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
    if (bytes_received < 0) {
        perror("接收失敗");
        close(client_socket);
        return;
    }

    ProtocolHeader header;
    if (parse_header(buffer, &header) == -1) {
        fprintf(stderr, "協議解析失敗\n");
        close(client_socket);
        return;
    }

    printf("接收到資料 - Operation: %d, Status: %d, Username: %s, Length: %d\n", 
           header.operation, header.status, header.username, header.length);

    // 解析數據區
    uint8_t data[MAX_DATA_SIZE + 1];
    parse_data(buffer + 3 + header.username_len + 4, header.length, data);
    printf("接收到數據: %s\n", data);

    // 回應客戶端
    const char *response = "資料接收成功";
    uint8_t send_buffer[2048];
    int send_len = pack_message(1, 0, header.username, (const uint8_t *)(response), strlen(response), send_buffer);
    send(client_socket, send_buffer, send_len, 0);

    close(client_socket);
}

int main() {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("建立 socket 失敗");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("設置 socket 選項失敗");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("綁定失敗");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("監聽失敗");
        exit(EXIT_FAILURE);
    }

    printf("傳輸伺服器已啟動，等待客戶端連接...\n");

    while (1) {
        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("接受連接失敗");
            continue;
        }
        handle_client(client_socket);
    }

    return 0;
}
