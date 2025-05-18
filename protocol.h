#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define MAX_DATA_SIZE 1024

typedef struct {
    uint8_t operation;
    uint8_t status;
    uint32_t length;
} ProtocolHeader;


int pack_message(uint8_t operation, uint8_t status,const uint8_t *data, uint32_t length, uint8_t *buffer);
int parse_header(const uint8_t *buffer, ProtocolHeader *header);
int parse_data(const uint8_t *buffer, uint32_t length, uint8_t *data);

#endif // PROTOCOL_H
