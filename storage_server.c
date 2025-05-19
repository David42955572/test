// 接收資料 (後端版)
int server_receive(int sockfd, uint8_t *operation, char *username, uint32_t *sequence, uint8_t data[MAX_DATA_SIZE]) {
    uint8_t buffer[MAX_DATA_SIZE];
    int received = recv(sockfd, buffer, MAX_DATA_SIZE, 0);
    if (received < 7) {
        fprintf(stderr, "收到的數據不足協議頭部長度\n");
        return -1;
    }

    ProtocolHeader header;
    if (parse_header(buffer, &header) != 0) {
        fprintf(stderr, "協議頭部解析失敗\n");
        return -1;
    }

    // 取得 operation、username、sequence
    *operation = header.operation;
    strncpy(username, header.username, header.username_len);
    username[header.username_len] = '\0'; // 確保字串結尾
    *sequence = header.sequence;

    // 解析數據
    parse_data(buffer + 3 + header.username_len + 4, header.length, data);

    printf("接收資料 - Operation: %d, Username: %s, Sequence: %u, Data: %s\n", 
            *operation, username, *sequence, data);
    
    return header.length;
}

