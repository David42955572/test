#include "protocol.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>


// 封裝完整協議包
int pack_message(uint8_t operation, uint8_t status, const char *username, uint32_t sequence, const uint8_t *data, uint32_t data_length, uint8_t *buffer) {
    uint8_t username_len = strlen(username);
    if (data_length > MAX_DATA_SIZE) return -1;

    buffer[0] = operation;
    buffer[1] = status;
    buffer[2] = username_len;
    memcpy(buffer + 3, username, username_len);

    uint32_t net_sequence = htonl(sequence);
    memcpy(buffer + 3 + username_len, &net_sequence, sizeof(uint32_t));

    // 寫入 data_length（network byte order）
    uint32_t net_data_length = htonl(data_length);
    memcpy(buffer + 3 + username_len + 4, &net_data_length, sizeof(uint32_t));

    // 寫入 data
    memcpy(buffer + 3 + username_len + 4 + 4, data, data_length);

    return 3 + username_len + 4 + 4 + data_length;
}

// 解析協議頭部
int parse_header(const uint8_t *buffer, ProtocolHeader *header) {
    header->operation = buffer[0];
    header->status = buffer[1];
    header->username_len = buffer[2];

    memcpy(header->username, buffer + 3, header->username_len);
    header->username[header->username_len] = '\0';

    uint32_t net_sequence;
    memcpy(&net_sequence, buffer + 3 + header->username_len, sizeof(uint32_t));
    header->sequence = ntohl(net_sequence);

    // 正確計算 data_length 的位置
    uint32_t data_len_offset = 3 + header->username_len + 4;
    uint32_t net_data_length;
    memcpy(&net_data_length, buffer + data_len_offset, sizeof(uint32_t));
    header->length = ntohl(net_data_length);

    return 0;
}


// 解析數據區
int parse_data(const uint8_t *buffer, uint32_t length, uint8_t *output) {
    memcpy(output, buffer, length);
    output[length] = '\0';  // 若你希望轉成 C-style 字串時再加
    return 0;
}
