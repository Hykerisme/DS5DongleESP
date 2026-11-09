#pragma once

#include <stdint.h>

#define DS5_LINK_SYNC 0xA5
#define DS5_LINK_MAX_PAYLOAD 64

typedef enum {
    DS5_LINK_HELLO = 0x01,
    DS5_LINK_HELLO_ACK = 0x02,
    DS5_LINK_INPUT_REPORT = 0x10,
    DS5_LINK_OUTPUT_REPORT = 0x11,
    DS5_LINK_STATUS = 0x20,
} ds5_link_type_t;

typedef struct {
    uint8_t type;
    uint8_t flags;
    uint16_t length;
    uint8_t sequence;
    uint8_t payload[DS5_LINK_MAX_PAYLOAD];
} ds5_link_frame_t;

uint16_t ds5_link_crc16(const uint8_t *data, uint16_t length);
uint16_t ds5_link_encode(const ds5_link_frame_t *frame, uint8_t *out, uint16_t capacity);
int ds5_link_decode(const uint8_t *data, uint16_t length, ds5_link_frame_t *frame);
