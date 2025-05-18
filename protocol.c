#include "protocol.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>

#define MAX_DATA_SIZE 1024

// 封裝完整協議包
int pack_message(uint8_t operation, uint8_t status, const char *username, const uint8_t *data, uint32_t data_length, uint8_t *buffer) {
    uint8_t username_len = strlen(username);
    if (data_length > MAX_DATA_SIZE) return -1;

    // 封裝頭部
    buffer[0] = operation;
    buffer[1] = status;
    buffer[2] = username_len;
    memcpy(buffer + 3, username, username_len);

    uint32_t net_length = htonl(data_length);
    memcpy(buffer + 3 + username_len, &net_length, sizeof(uint32_t));

    // 封裝數據區
    memcpy(buffer + 3 + username_len + 4, data, data_length);

    return 3 + username_len + 4 + data_length;
}

// 解析協議頭部
int parse_header(const uint8_t *buffer, ProtocolHeader *header) {
    header->operation = buffer[0];
    header->status = buffer[1];
    header->username_len = buffer[2];

    // 提取使用者名稱
    memcpy(header->username, buffer + 3, header->username_len);
    header->username[header->username_len] = '\0';

    uint32_t net_length;
    memcpy(&net_length, buffer + 3 + header->username_len, sizeof(uint32_t));
    header->length = ntohl(net_length);

    return 0;
}

// 解析數據區
int parse_data(const uint8_t *buffer, uint32_t length, uint8_t *output) {
    memcpy(output, buffer, length);
    output[length] = '\0';  // 確保結尾是空字元
    return 0;
}
