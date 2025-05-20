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
                   uint8_t *status,   // 新增 status 輸出參數
                   char *username, 
                   uint32_t *sequence, 
                   uint8_t data[MAX_DATA_SIZE]) {
    static uint8_t recv_buffer[RECV_BUF_SIZE];
    static int buffer_len = 0;

    while (1) {
        int bytes = recv(sockfd, recv_buffer + buffer_len, RECV_BUF_SIZE - buffer_len, 0);
        if (bytes < 0) {
            perror("接收失敗");
            return -1;
        } else if (bytes == 0) {
            return 0; // 連線關閉
        }
        buffer_len += bytes;

        if (buffer_len < 11) continue;

        uint8_t username_len = recv_buffer[2];
        int header_len = 3 + username_len + 8;
        if (buffer_len < header_len) continue;

        uint32_t data_len;
        memcpy(&data_len, recv_buffer + 3 + username_len + 4, 4);
        data_len = ntohl(data_len);

        int total_len = header_len + data_len;
        if (buffer_len < total_len) continue;

        ProtocolHeader header;
        if (parse_header(recv_buffer, &header) != 0) {
            fprintf(stderr, "協議頭部解析失敗\n");
            return -1;
        }

        *operation = header.operation;
        *status = header.status;            // 解析 status
        strncpy(username, header.username, header.username_len);
        username[header.username_len] = '\0';
        *sequence = header.sequence;

        parse_data(recv_buffer + header_len, header.length, data);

        printf("接收資料 - Operation: %d, Status: %d, Username: %s, Sequence: %u, Data: %s\n",
               *operation, *status, username, *sequence, data);

        memmove(recv_buffer, recv_buffer + total_len, buffer_len - total_len);
        buffer_len -= total_len;

        return header.length;
    }
}

int handle_login(const char *username, const uint8_t *password) {
    FILE *fp = fopen("users.txt", "r");
    if (!fp) {
        perror("無法打開使用者清單檔案");
        return 0;
    }

    char file_user[100], file_pass[100];
    int valid = 0;

    while (fscanf(fp, "%s %s", file_user, file_pass) == 2) {
        if (strcmp(file_user, username) == 0 && strcmp(file_pass, (const char *)password) == 0) {
            valid = 1;
            break;
        }
    }

    fclose(fp);
    return valid;
}

FILE* handle_start_backup(const char *username, const char *timestamp) {
    char folder[128], filename[256];
    snprintf(folder, sizeof(folder), "./backup/%s", username);
    mkdir(folder, 0777);  // 若資料夾不存在則建立

    snprintf(filename, sizeof(filename), "%s/%s_%s.txt", folder, username, timestamp);
    return fopen(filename, "w");
}

int handle_write_backup(FILE *fp, const uint8_t *data, int len) {
    if (!fp) return -1;
    size_t written = fwrite(data, 1, len, fp);
    return written == len ? 0 : -1;
}

int handle_list_backups(int sockfd, const char *username) {
    char path[128];
    snprintf(path, sizeof(path), "./backup/%s", username);

    DIR *dir = opendir(path);
    if (!dir) {
        perror("無法開啟備份資料夾");
        return -1;
    }

    struct dirent *entry;
    uint32_t seq = 1;

    while ((entry = readdir(dir))) {
        if (entry->d_type == DT_REG) {
            client_send(sockfd, 4, 0, username, &seq, (const uint8_t *)entry->d_name, strlen(entry->d_name));
            seq++;
        }
    }

    closedir(dir);
    return 0;
}

int handle_send_backup(int sockfd, const char *username, const char *filename) {
    char filepath[256];
    snprintf(filepath, sizeof(filepath), "./backup/%s/%s", username, filename);

    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        perror("無法打開備份檔案");
        return -1;
    }

    uint8_t buffer[MAX_DATA_SIZE];
    size_t read_len;
    uint32_t seq = 1;

    while ((read_len = fread(buffer, 1, MAX_DATA_SIZE, fp)) > 0) {
        client_send(sockfd, 5, 0, username, &seq, buffer, read_len);
        seq++;
    }

    fclose(fp);
    return 0;
}

void transfer_data(int src_socket, char *username) {
    uint8_t operation = 0;
    uint8_t status = 0;
    int keep_receiving = 1;
    FILE *backup_fp = NULL; // 用於備份寫入階段

    while (keep_receiving) {
        uint8_t data[MAX_DATA_SIZE] = {0};
        uint32_t sequence = 0;

        int length = server_receive(src_socket, &operation, &status, username, &sequence, data);
        if (length < 0) {
            perror("接收資料失敗");
            break;
        } else if (length == 0) {
            printf("連線已關閉\n");
            break;
        }

        printf("接收到數據 - Operation: %d, Status: %d, Sequence: %u, Data: %s\n",
               operation, status, sequence, data);

        switch (operation) {
            case 1: // 登入驗證
                if (!handle_login(username, data)) {
                    fprintf(stderr, "登入失敗，結束連線\n");
                    keep_receiving = 0;
                }
                break;

            case 2: // 創建並開啟備份檔案（data 是 timestamp）
                if (backup_fp) fclose(backup_fp);
                backup_fp = handle_start_backup(username, (char *)data);
                if (!backup_fp) {
                    fprintf(stderr, "無法創建備份檔案\n");
                    keep_receiving = 0;
                }
                break;

            case 3: // 寫入備份資料
                if (handle_write_backup(backup_fp, data, length) != 0) {
                    fprintf(stderr, "備份資料寫入失敗\n");
                    keep_receiving = 0;
                }
                break;

            case 4: // 回傳該使用者的所有檔案名稱
                handle_list_backups(src_socket, username);
                break;

            case 5: // 傳送指定備份檔案內容（data 是檔名）
                handle_send_backup(src_socket, username, (char *)data);
                break;

            default:
                fprintf(stderr, "未知的操作類型: %d\n", operation);
                break;
        }

        if (status == 1) {
            keep_receiving = 0;
        }

    }

    if (backup_fp) fclose(backup_fp);

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
