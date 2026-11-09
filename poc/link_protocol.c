#include "link_protocol.h"

uint16_t ds5_link_crc16(const uint8_t *data, uint16_t length)
{
    uint16_t crc = 0xFFFF;
    while (length--) {
        crc ^= *data++;
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 1) ? (crc >> 1) ^ 0xA001 : crc >> 1;
        }
    }
    return crc;
}

uint16_t ds5_link_encode(const ds5_link_frame_t *frame, uint8_t *out, uint16_t capacity)
{
    if (!frame || !out || frame->length > DS5_LINK_MAX_PAYLOAD || capacity < frame->length + 8) {
        return 0;
    }

    out[0] = DS5_LINK_SYNC;
    out[1] = frame->type;
    out[2] = frame->flags;
    out[3] = (uint8_t)(frame->length & 0xFF);
    out[4] = (uint8_t)(frame->length >> 8);
    out[5] = frame->sequence;
    for (uint16_t i = 0; i < frame->length; ++i) {
        out[6 + i] = frame->payload[i];
    }
    uint16_t crc = ds5_link_crc16(&out[1], 5 + frame->length);
    out[6 + frame->length] = (uint8_t)(crc & 0xFF);
    out[7 + frame->length] = (uint8_t)(crc >> 8);
    return frame->length + 8;
}

int ds5_link_decode(const uint8_t *data, uint16_t length, ds5_link_frame_t *frame)
{
    if (!data || !frame || length < 8 || data[0] != DS5_LINK_SYNC) {
        return -1;
    }

    uint16_t payload_length = (uint16_t)data[3] | ((uint16_t)data[4] << 8);
    if (payload_length > DS5_LINK_MAX_PAYLOAD || length < payload_length + 8) {
        return -1;
    }

    uint16_t expected = (uint16_t)data[6 + payload_length] |
                        ((uint16_t)data[7 + payload_length] << 8);
    if (ds5_link_crc16(&data[1], 5 + payload_length) != expected) {
        return -1;
    }

    frame->type = data[1];
    frame->flags = data[2];
    frame->length = payload_length;
    frame->sequence = data[5];
    for (uint16_t i = 0; i < payload_length; ++i) {
        frame->payload[i] = data[6 + i];
    }
    return (int)(payload_length + 8);
}
