#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include "protocol.h"

#define SERVER_PORT 8080
#define BUFFER_SIZE 2048

// 初始化客戶端連線
int init_client(const char *server_ip) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid address");
        close(sockfd);
        return -1;
    }

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        close(sockfd);
        return -1;
    }

    printf("Connected to server %s:%d\n", server_ip, SERVER_PORT);
    return sockfd;
}

// 發送資料
int client_send(int sockfd, uint8_t operation, uint8_t status, const uint8_t *data, uint32_t length) {
    uint8_t buffer[BUFFER_SIZE];
    int send_len = pack_message(operation, status, data, length, buffer);
    if (send_len < 0) {
        fprintf(stderr, "封裝訊息失敗\n");
        return -1;
    }

    int sent_bytes = send(sockfd, buffer, send_len, 0);
    if (sent_bytes != send_len) {
        perror("發送數據失敗");
        return -1;
    }

    return sent_bytes;
}

// 接收資料
int client_receive(int sockfd, uint8_t *buffer, int buffer_size) {
    int received = recv(sockfd, buffer, buffer_size, 0);
    if (received < 6) {
        fprintf(stderr, "收到的數據不足協議頭部長度\n");
        return -1;
    }

    ProtocolHeader header;
    if (parse_header(buffer, &header) != 0) {
        fprintf(stderr, "協議頭部解析失敗\n");
        return -1;
    }

    if (received < 6 + header.length) {
        fprintf(stderr, "數據區未完整接收，期望 %d 字節，實際接收 %d 字節\n", header.length, received - 6);
        return -1;
    }

    return header.length;
}

int main() {
    const char *server_ip = "192.168.56.102";
    int sockfd = init_client(server_ip);

    if (sockfd >= 0) {
        const char *test_message = "Hello, Server!";
        client_send(sockfd, 1, 0, (uint8_t *)test_message, strlen(test_message));

        uint8_t buffer[BUFFER_SIZE];
        int received_len = client_receive(sockfd, buffer, BUFFER_SIZE);
        if (received_len > 0) {
            printf("接收成功，數據長度：%d\n", received_len);
        }

        close(sockfd);
    }

    return 0;
}
