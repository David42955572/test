#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "protocol.h"

#define MAIN_PORT 8080

// 發送資料
int server_send(int sockfd, uint8_t operation, uint8_t status, const char *username, uint32_t *sequence, const uint8_t *data, uint32_t length) {
    uint8_t buffer[MAX_DATA_SIZE];
    int send_len = pack_message(operation, status, username, *sequence, data, length, buffer);
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

#define RECV_BUF_SIZE 8192

int server_receive(int sockfd, 
                          uint8_t *operation, 
                          char *username, 
                          uint32_t *sequence, 
                          uint8_t data[MAX_DATA_SIZE]) {
    static uint8_t recv_buffer[RECV_BUF_SIZE];
    static int buffer_len = 0;

    while (1) {
        // 嘗試從 socket 讀更多資料填滿緩衝區
        int bytes = recv(sockfd, recv_buffer + buffer_len, RECV_BUF_SIZE - buffer_len, 0);
        if (bytes < 0) {
            perror("接收失敗");
            return -1;
        } else if (bytes == 0) {
            // 連線關閉
            return 0;
        }
        buffer_len += bytes;

        // 判斷是否有足夠資料解析 header（假設header最小長度為固定值，例如 11）
        if (buffer_len < 11) continue;

        uint8_t username_len = recv_buffer[2];
        int header_len = 3 + username_len + 8;
        if (buffer_len < header_len) continue;

        // 取得 data 長度
        uint32_t data_len;
        memcpy(&data_len, recv_buffer + 3 + username_len + 4, 4);
        data_len = ntohl(data_len);

        int total_len = header_len + data_len;
        if (buffer_len < total_len) continue;

        // 解析 header
        ProtocolHeader header;
        if (parse_header(recv_buffer, &header) != 0) {
            fprintf(stderr, "協議頭部解析失敗\n");
            return -1;
        }

        // 複製結果輸出
        *operation = header.operation;
        strncpy(username, header.username, header.username_len);
        username[header.username_len] = '\0';
        *sequence = header.sequence;

        parse_data(recv_buffer + header_len, header.length, data);

        printf("接收資料 - Operation: %d, Username: %s, Sequence: %u, Data: %s\n", 
                *operation, username, *sequence, data);

        // 移除已處理資料
        memmove(recv_buffer, recv_buffer + total_len, buffer_len - total_len);
        buffer_len -= total_len;

        // 回傳收到的資料長度
        return header.length;
    }
}


void transfer_data(int src_socket,  char *username) {
    int sequence_counter = 1;
    int received_final_status = 0;
    uint8_t operation = 0;

    while (!received_final_status) {
        uint8_t data[MAX_DATA_SIZE] = {0};
        uint32_t sequence = 0;
        int length = server_receive(src_socket, &operation, username, &sequence, data);

        if (length < 0) {
            perror("接收資料失敗");
            break;
        }

        printf("接收到數據 - Sequence: %u, Data: %s\n", sequence, data);

        if (sequence == sequence_counter) {
            received_final_status = 1;
        }

        sequence_counter++;
    }

    uint8_t response_data[50];
    snprintf((char *)response_data, 50, "收到 %d 筆資料", sequence_counter);
    uint32_t response_sequence = 1;
    server_send(src_socket, 0, 0, username, &response_sequence, response_data, strlen((char *)response_data));
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("建立 socket 失敗");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(MAIN_PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("綁定 socket 失敗");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 10) == -1) {
        perror("監聽 socket 失敗");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("伺服器正在監聽 port %d\n", MAIN_PORT);

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket == -1) {
            perror("接受連線失敗");
            continue;
        }

        char username[32];
        transfer_data(client_socket, username);

        close(client_socket);
        printf("連線已關閉\n");
    }

    close(server_socket);
    return 0;
}
