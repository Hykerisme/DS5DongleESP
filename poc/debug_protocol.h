#pragma once

#include <stdint.h>

#define DS5_DEBUG_SYNC 0xD5
#define DS5_DEBUG_VERSION 1
#define DS5_DEBUG_MAX_PAYLOAD 64
#define DS5_DEBUG_FRAME_OVERHEAD 8

typedef enum {
    DS5_DEBUG_HELLO = 0x01,
    DS5_DEBUG_HELLO_ACK = 0x02,
    DS5_DEBUG_CLAIM = 0x03,
    DS5_DEBUG_RELEASE = 0x04,
    DS5_DEBUG_INPUT_REPORT = 0x10,
    DS5_DEBUG_OUTPUT_REPORT = 0x11,
    DS5_DEBUG_STATUS = 0x20,
    DS5_DEBUG_PING = 0x30,
    DS5_DEBUG_PONG = 0x31,
} ds5_debug_type_t;

typedef struct {
    uint8_t type;
    uint8_t flags;
    uint16_t length;
    uint8_t sequence;
    uint8_t payload[DS5_DEBUG_MAX_PAYLOAD];
} ds5_debug_frame_t;

uint16_t ds5_debug_encode(const ds5_debug_frame_t *frame, uint8_t *out, uint16_t capacity);
int ds5_debug_decode(const uint8_t *data, uint16_t length, ds5_debug_frame_t *frame);
