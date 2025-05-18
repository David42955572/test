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

// 客戶端發送登入請求
void client_login(int sockfd, const char *username, const char *password) {
    uint8_t buffer[BUFFER_SIZE];
    char credentials[256];
    snprintf(credentials, sizeof(credentials), "%s:%s", username, password);

    int len = pack_message(1, 0, (uint8_t *)credentials, strlen(credentials), buffer);
    send(sockfd, buffer, len, 0);

    // 接收回應
    int received = recv(sockfd, buffer, BUFFER_SIZE, 0);
    if (received > 0) {
        ProtocolHeader header;
        parse_header(buffer, &header);
        printf("Login Response - Operation: %d, Status: %d, Length: %d\n", header.operation, header.status, header.length);
    }
}

int main() {
    const char *server_ip = "192.168.56.102";
    int sockfd = init_client(server_ip);

    if (sockfd >= 0) {
        client_login(sockfd, "test_user", "password123");
        close(sockfd);
    }

    return 0;
}
