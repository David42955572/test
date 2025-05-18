#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define MAX_DATA_SIZE 1024
#define MAX_USERNAME_LENGTH 255

typedef struct {
    uint8_t operation;
    uint8_t status;
    uint8_t username_len;
    char username[MAX_USERNAME_LENGTH + 1];  // +1 是為了存放 '\0'
    uint32_t length;  // 數據區長度
} ProtocolHeader;

/**
 * 封裝訊息
 * operation: 操作碼
 * status: 狀態碼
 * username: 使用者名稱
 * data: 數據
 * data_length: 數據長度
 * buffer: 封裝後的緩衝區
 * 返回封裝後的數據長度
 */
int pack_message(uint8_t operation, uint8_t status, const char *username, const uint8_t *data, uint32_t data_length, uint8_t *buffer);

/**
 * 解析協議頭部
 * buffer: 接收的緩衝區
 * header: 協議頭部結構體
 * 返回 0 表示成功，-1 表示失敗
 */
int parse_header(const uint8_t *buffer, ProtocolHeader *header);

/**
 * 解析數據區
 * buffer: 數據部分的起始位置
 * length: 數據長度
 * output: 解析後的數據
 * 返回 0 表示成功，-1 表示失敗
 */
int parse_data(const uint8_t *buffer, uint32_t length, uint8_t *output);

#endif // PROTOCOL_H
