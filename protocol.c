#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>

#define MAX_DATA_SIZE 1024

// 協議頭部結構
typedef struct {
    uint8_t operation;  // 操作碼
    uint8_t status;     // 狀態碼
    uint32_t length;    // 數據區長度 (大端序)
} ProtocolHeader;

// 封裝協議頭部
void pack_header(uint8_t operation, uint8_t status, uint32_t length, uint8_t *buffer) {
    buffer[0] = operation;
    buffer[1] = status;
    uint32_t net_length = htonl(length);
    memcpy(buffer + 2, &net_length, sizeof(uint32_t));
}

// 封裝完整協議包
int pack_message(uint8_t operation, uint8_t status, const uint8_t *data, uint32_t data_length, uint8_t *buffer) {
    if (data_length > MAX_DATA_SIZE) return -1;

    // 封裝頭部
    pack_header(operation, status, data_length, buffer);

    // 封裝數據區
    memcpy(buffer + 6, data, data_length);

    return 6 + data_length;  // 返回總長度
}

// 解析協議頭部
int parse_header(const uint8_t *buffer, ProtocolHeader *header) {
    header->operation = buffer[0];
    header->status = buffer[1];

    uint32_t net_length;
    memcpy(&net_length, buffer + 2, sizeof(uint32_t));
    header->length = ntohl(net_length);

    return 0;
}

// 解析數據區
int parse_data(const uint8_t *buffer, uint32_t length, uint8_t *output) {
    memcpy(output, buffer, length);
    output[length] = '\0';  // 確保結尾是空字元
    return 0;
}

// 測試函式
void test_protocol() {
    uint8_t buffer[1030];
    uint8_t data[] = "Hello, Protocol";
    int len = pack_message(1, 0, data, strlen((char *)data), buffer);

    ProtocolHeader header;
    parse_header(buffer, &header);

    printf("Operation: %d, Status: %d, Length: %d\n", header.operation, header.status, header.length);

    uint8_t output[MAX_DATA_SIZE];
    parse_data(buffer + 6, header.length, output);
    printf("Data: %s\n", output);
}

int main() {
    test_protocol();
    return 0;
}
