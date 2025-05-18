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
int client_send(int sockfd, uint8_t operation, uint8_t status, const char *username, const uint8_t *data, uint32_t length) {
    uint8_t buffer[BUFFER_SIZE];
    int send_len = pack_message(operation, status, username, data, length, buffer);
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
int client_receive(int sockfd, const char *username) {
    uint8_t buffer[BUFFER_SIZE];
    int received = recv(sockfd, buffer, BUFFER_SIZE, 0);
    if (received < 6) {
        fprintf(stderr, "收到的數據不足協議頭部長度\n");
        return -1;
    }

    ProtocolHeader header;
    if (parse_header(buffer, &header) != 0) {
        fprintf(stderr, "協議頭部解析失敗\n");
        return -1;
    }

    if (strcmp(username, header.username) != 0) {
        printf("收到非針對當前用戶的數據\n");
        return -1;
    }

    uint8_t data[MAX_DATA_SIZE + 1];
    parse_data(buffer + 3 + header.username_len + 4, header.length, data);
    printf("接收資料 - Operation: %d, Status: %d, Data: %s\n", header.operation, header.status, data);

    return header.length;
}

// 發送登入請求
int client_send_login(int sockfd, const char *username, const char *password) {
    char credentials[256];

    int sent = client_send(sockfd, 1, 0, username, (uint8_t *)password, strlen(password));
    if (sent < 0) {
        fprintf(stderr, "登入請求發送失敗\n");
        return -1;
    }

    client_receive(sockfd, username);
    return 0;
}

// 發送備份請求
int client_send_backup_file(int sockfd, const char *username, const char *filepath) {
    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        perror("打開備份檔案失敗");
        return -1;
    }

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

    fread(file_buffer, 1, filesize, fp);
    fclose(fp);

    int sent = client_send(sockfd, 2, 0, username, file_buffer, filesize);
    free(file_buffer);

    if (sent < 0) {
        fprintf(stderr, "備份資料發送失敗\n");
        return -1;
    }

    client_receive(sockfd, username);
    return 0;
}

// 發送取備份請求（operation = 3），data 是檔案名稱
int client_send_backup_request(int sockfd, const char *username, const char *filename) {
    // filename 以字串形式放入 data，長度為 strlen(filename)
    int sent = client_send(sockfd, 3, 0, username, (const uint8_t *)filename, strlen(filename));
    if (sent < 0) {
        fprintf(stderr, "取備份請求發送失敗\n");
        return -1;
    }

    // 等待伺服器回傳備份資料
    int recv_len = client_receive(sockfd, username);
    if (recv_len < 0) {
        fprintf(stderr, "取備份回應接收失敗\n");
        return -1;
    }

    return 0;
}

int client_request_and_receive_file_list(int sockfd, const char *username) {
    // 送出取備份檔案列表請求（operation=4, status=0, data=NULL）
    int sent = client_send(sockfd, 4, 0, username, NULL, 0);
    if (sent < 0) {
        fprintf(stderr, "取備份檔案列表請求發送失敗\n");
        return -1;
    }

    // 緊接著準備接收多筆檔案名稱
    uint8_t buffer[BUFFER_SIZE];
    int total_files = 0;

    while (1) {
        int received = recv(sockfd, buffer, BUFFER_SIZE, 0);
        if (received <= 0) {
            // 連線斷開或錯誤
            break;
        }

        ProtocolHeader header;
        if (parse_header(buffer, &header) != 0) {
            fprintf(stderr, "協議頭解析失敗\n");
            break;
        }

        if (strcmp(username, header.username) != 0) {
            printf("收到非針對當前用戶的數據，忽略\n");
            continue;
        }

        if (header.length == 0) {
            // 空資料長度代表沒資料了，結束接收
            break;
        }

        uint8_t data[MAX_DATA_SIZE + 1] = {0};
        parse_data(buffer + 3 + header.username_len + 4, header.length, data);
        data[header.length] = '\0';  // 確保字串結尾

        printf("備份檔案 #%d: %s\n", ++total_files, data);
    }

    if (total_files == 0) {
        printf("沒有備份檔案\n");
    }

    return total_files;
}

int main() {
    const char *server_ip = "192.168.56.102";
    const char *username = "user";
    const char *password = "pass";
    int sockfd = init_client(server_ip);

    if (sockfd >= 0) {
        client_send_login(sockfd, username, password);
        client_send_backup_file(sockfd, username, "test.txt");
        client_send_backup_request(sockfd, username, "test.txt");
        client_request_and_receive_file_list(sockfd, username);
        close(sockfd);
    }

    return 0;
}
