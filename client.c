#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include "protocol.h"
#include <netinet/tcp.h>

#define SERVER_PORT 8080
#define RECV_BUF_SIZE 8192

// 初始化客戶端連線
int init_client(const char *server_ip, int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return -1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

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

    printf("Connected to server %s:%d\n", server_ip, port);
    return sockfd;
}

// 發送資料
int client_send(int sockfd, uint8_t operation, uint8_t status, const char *username, uint32_t *sequence, const uint8_t *data, uint32_t length) {
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

// 接收資料
int client_receive(int sockfd, const char *username, uint32_t *sequence, uint8_t data[MAX_DATA_SIZE]) {
    static uint8_t recv_buffer[RECV_BUF_SIZE];
    static int buffer_len = 0;

    while (1) {
        // 嘗試從 socket 讀更多資料進緩衝區
        int bytes = recv(sockfd, recv_buffer + buffer_len, RECV_BUF_SIZE - buffer_len, 0);
        if (bytes < 0) {
            perror("接收失敗");
            return -1;
        } else if (bytes == 0) {
            // 對端關閉連線
            printf("連線關閉\n");
            return 0;
        }
        buffer_len += bytes;

        if (buffer_len < 11) continue;

        uint8_t username_len = recv_buffer[2];
        int header_len = 3 + username_len + 8;
        if (buffer_len < header_len) continue;

        // 取得 data 長度（4 bytes）
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

        // 驗證 username 是否符合
        if (strcmp(username, header.username) != 0) {
            fprintf(stderr, "收到非針對當前用戶的數據\n");
            // 丟棄這筆封包
            memmove(recv_buffer, recv_buffer + total_len, buffer_len - total_len);
            buffer_len -= total_len;
            continue;
        }

        // 複製資料
        parse_data(recv_buffer + header_len, header.length, data);
        *sequence = header.sequence;

        printf("接收資料 - Operation: %d, Status: %d, Sequence: %u, Data: %s\n", 
                header.operation, header.status, *sequence, data);

        // 移除已處理的封包
        memmove(recv_buffer, recv_buffer + total_len, buffer_len - total_len);
        buffer_len -= total_len;

        return header.length;
    }
}

// 請求動態分配 port
int request_port(int sockfd) {
    uint32_t sequence = 1;
    uint8_t buffer[MAX_DATA_SIZE] = {0};
    int sent = client_send(sockfd, 0, 0, "", &sequence, NULL, 0);
    if (sent < 0) {
        fprintf(stderr, "Port request failed\n");
        return -1;
    }

    client_receive(sockfd, "", &sequence, buffer);

    int new_port = atoi((char *)buffer);
    printf("Received new port: %d\n", new_port);
    return new_port;
}

// 發送登入請求
int client_send_login(int sockfd, const char *username, const char *password) {
    
    uint32_t sequence = 1;
    
    int sent = client_send(sockfd, 1, 1, username, &sequence, (uint8_t *)password, strlen(password));
    if (sent < 0) {
        fprintf(stderr, "登入請求發送失敗\n");
        return -1;
    }

    uint8_t data[MAX_DATA_SIZE] = {0};
    client_receive(sockfd, username, &sequence, data);
    
    return 0;
}

int client_send_file_request(int sockfd, const char *username, const char *filepath) {
    uint32_t sequence = 1;
    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        perror("打開檔案失敗");
        return -1;
    }

    // 獲取檔案名稱
    const char *filename = strrchr(filepath, '/');
    filename = (filename) ? filename + 1 : filepath;

    // 獲取時間戳
    struct stat file_stat;
    if (stat(filepath, &file_stat) != 0) {
        perror("獲取檔案資訊失敗");
        fclose(fp);
        return -1;
    }

    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&file_stat.st_mtime));

    // 構建資料格式：檔名|時間戳\0
    char data_name[256];
    snprintf(data_name, sizeof(data_name), "%s|%s", filename, timestamp);

    // 發送請求
    int sent = client_send(sockfd, 2, 1, username, &sequence, (uint8_t *)data_name, strlen(data_name) + 1);
    if (sent < 0) {
        fprintf(stderr, "備份請求發送失敗\n");
        fclose(fp);
        return -1;
    }

    fclose(fp);
    return 0;
}

int client_send_file_content(int sockfd, const char *username, const char *filepath) {
    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        perror("打開檔案失敗");
        return -1;
    }

    uint8_t buffer[MAX_DATA_SIZE];
    size_t read_bytes;
    uint32_t sequence_number = 1;

    while ((read_bytes = fread(buffer, 1, MAX_DATA_SIZE - 11 - strlen(username), fp)) > 0) {
        
        int sent = client_send(sockfd, 3, 0, username, &sequence_number, (uint8_t *)buffer, read_bytes);
        if (sent < 0) {
            perror("發送資料失敗");
            fclose(fp);
            return -1;
        }

        sequence_number++;
    }

    // 傳送結束標誌
    int sent = client_send(sockfd, 3, 1, username, &sequence_number, NULL, 0);
    if (sent < 0) {
        fprintf(stderr, "結束標誌傳送失敗\n");
    }
    
    fclose(fp);
    printf("檔案傳輸完成：%s\n", filepath);
    return 0;
}

int client_backup_file(int sockfd, const char *username, const char *filepath) {
    // 1. 傳送備份請求
    if (client_send_file_request(sockfd, username, filepath) != 0) {
        fprintf(stderr, "備份請求失敗：%s\n", filepath);
        return -1;
    }
    
    // 2. 傳送檔案內容
    if (client_send_file_content(sockfd, username, filepath) != 0) {
        fprintf(stderr, "檔案內容傳輸失敗：%s\n", filepath);
        return -1;
    }

    return 0;
}

// 發送取備份請求（operation = 5），data 是檔案名稱
int client_send_backup_request(int sockfd, const char *username, const char *filename) {
    uint32_t sequence = 1;

    // 發送取備份請求（operation = 5）
    int sent = client_send(sockfd, 5, 1, username, &sequence, (const uint8_t *)filename, strlen(filename));
    if (sent < 0) {
        fprintf(stderr, "取備份請求發送失敗\n");
        return -1;
    }

    // 開始接收備份資料（可能是多封包）
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        perror("無法開啟檔案寫入");
        return -1;
    }

    while (1) {
        uint8_t data[MAX_DATA_SIZE] = {0};
        int recv_len = client_receive(sockfd, username, &sequence, data);
        if (recv_len < 0) {
            fprintf(stderr, "接收備份資料失敗\n");
            fclose(fp);
            return -1;
        }

        // 檢查是否為結束封包（視協議設計而定，這裡假設 status == 1 表示結束）
        if (recv_len == 0) {
            break;
        }

        fwrite(data, 1, recv_len, fp);

        // 這邊可以視需要顯示接收進度
    }

    fclose(fp);
    printf("備份資料接收完成，已儲存為 %s\n", filename);
    return 0;
}

int client_request_and_receive_file_list(int sockfd, const char *username) {
    uint32_t sequence = 1;

    // 發送請求：operation = 4
    int sent = client_send(sockfd, 4, 1, username, &sequence, NULL, 0);
    if (sent < 0) {
        fprintf(stderr, "取備份檔案列表請求發送失敗\n");
        return -1;
    }

    // 接收多個封包
    int total_files = 0;

    while (1) {
        uint8_t data[MAX_DATA_SIZE] = {0};
        int recv_len = client_receive(sockfd, username, &sequence, data);
        if (recv_len <= 0) {
            fprintf(stderr, "接收備份列表時發生錯誤或連線關閉\n");
            break;
        }

        // 檢查結尾條件（例：空字串）
        if (strlen((char *)data) == 0) {
            break;
        }

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

     // 初始連接以請求新的 port
    int sockfd = init_client(server_ip, SERVER_PORT);
    if (sockfd < 0) return -1;
    
    // 請求新的 port
    int new_port = request_port(sockfd);
    close(sockfd);

    if (new_port < 0) return -1;

    // 使用新的 port 進行後續通訊
    sockfd = init_client(server_ip, new_port);

    int flag = 1;
    if (setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag)) < 0) {
        perror("setsockopt TCP_NODELAY 失敗");
    } else {
        printf("TCP_NODELAY 設定成功\n");
    }
    
    if (sockfd >= 0) {

        //client_send_login(sockfd, username, password);
        client_backup_file(sockfd, username, "test.txt");
        //client_send_backup_request(sockfd, username, "test.txt");
        //client_request_and_receive_file_list(sockfd, username);
        close(sockfd);
    }

    return 0;
}
