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

// 發送登入請求
int client_send_login(int sockfd, const char *username, const char *password) {
    char credentials[256];
    snprintf(credentials, sizeof(credentials), "%s:%s", username, password);

    // 先發送登入請求
    int sent = client_send(sockfd, 1, 0, (uint8_t *)credentials, strlen(credentials));
    if (sent < 0) {
        fprintf(stderr, "登入請求發送失敗\n");
        return -1;
    }

    // 接收伺服器回應
    uint8_t buffer[BUFFER_SIZE];
    int received_len = client_receive(sockfd, buffer, BUFFER_SIZE);
    if (received_len < 0) {
        fprintf(stderr, "登入回應接收失敗\n");
        return -1;
    }

    ProtocolHeader header;
    parse_header(buffer, &header);

    // 解析數據區並印出
    uint8_t data[MAX_DATA_SIZE + 1];
    parse_data(buffer + 6, header.length, data);

    printf("登入回應 - Operation: %d, Status: %d, Data: %s\n", header.operation, header.status, data);

    // 這裡可以根據 status 做不同的處理，譬如判斷是否登入成功
    switch (header.status) {
        case 0:
            printf("登入成功\n");
            break;
        case 1:
            printf("用戶名或密碼錯誤\n");
            break;
        default:
            printf("未知狀態碼: %d\n", header.status);
            break;
    }

    return 0;
}

int client_send_backup_file(int sockfd, const char *filepath) {
    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        perror("打開備份檔案失敗");
        return -1;
    }

    // 讀檔案大小
    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);
    rewind(fp);

    if (filesize <= 0 || filesize > MAX_DATA_SIZE) {
        fprintf(stderr, "備份檔案大小不正確或超過上限\n");
        fclose(fp);
        return -1;
    }

    uint8_t *file_buffer = malloc(filesize);
    if (!file_buffer) {
        fprintf(stderr, "記憶體配置失敗\n");
        fclose(fp);
        return -1;
    }

    size_t read_bytes = fread(file_buffer, 1, filesize, fp);
    fclose(fp);
    if (read_bytes != filesize) {
        fprintf(stderr, "讀取備份檔案失敗\n");
        free(file_buffer);
        return -1;
    }

    // 傳送備份資料
    int sent = client_send(sockfd, 2, 0, file_buffer, filesize);
    free(file_buffer);

    if (sent < 0) {
        fprintf(stderr, "備份資料發送失敗\n");
        return -1;
    }

    // 接收伺服器回應
    uint8_t buffer[BUFFER_SIZE];
    int received_len = client_receive(sockfd, buffer, BUFFER_SIZE);
    if (received_len < 0) {
        fprintf(stderr, "備份回應接收失敗\n");
        return -1;
    }

    ProtocolHeader header;
    parse_header(buffer, &header);

    uint8_t data[MAX_DATA_SIZE + 1];
    parse_data(buffer + 6, header.length, data);

    printf("備份回應 - Operation: %d, Status: %d, Data: %s\n", header.operation, header.status, data);

    if (header.status != 0) {
        fprintf(stderr, "備份失敗，狀態碼：%d\n", header.status);
        return -1;
    }

    return 0;
}

int main() {
    const char *server_ip = "192.168.56.102";
    int sockfd = init_client(server_ip);

    if (sockfd >= 0) {
        const char *username , *password;
        username="user";
        password="pass";
        client_send_login(sockfd, username, password);
        client_send_backup_file(sockfd, "test.txt");
        
        close(sockfd);
    }

    return 0;
}
