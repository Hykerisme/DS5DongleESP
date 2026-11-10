#include <string.h>

#include "debug_protocol.h"
#include "link_protocol.h"

uint16_t ds5_debug_encode(const ds5_debug_frame_t *frame, uint8_t *out, uint16_t capacity)
{
    uint16_t total;
    if (!frame || !out || frame->length > DS5_DEBUG_MAX_PAYLOAD) {
        return 0;
    }
    total = (uint16_t)(frame->length + DS5_DEBUG_FRAME_OVERHEAD);
    if (capacity < total) {
        return 0;
    }
    out[0] = DS5_DEBUG_SYNC;
    out[1] = DS5_DEBUG_VERSION;
    out[2] = frame->type;
    out[3] = frame->flags;
    out[4] = (uint8_t)frame->length;
    out[5] = (uint8_t)(frame->length >> 8);
    out[6] = frame->sequence;
    memcpy(&out[7], frame->payload, frame->length);
    uint16_t crc = ds5_link_crc16(&out[1], (uint16_t)(6 + frame->length));
    out[7 + frame->length] = (uint8_t)crc;
    out[8 + frame->length] = (uint8_t)(crc >> 8);
    return total;
}

int ds5_debug_decode(const uint8_t *data, uint16_t length, ds5_debug_frame_t *frame)
{
    uint16_t payload_length;
    uint16_t total;
    uint16_t expected_crc;
    if (!data || !frame || length < DS5_DEBUG_FRAME_OVERHEAD ||
        data[0] != DS5_DEBUG_SYNC || data[1] != DS5_DEBUG_VERSION) {
        return -1;
    }
    payload_length = (uint16_t)data[4] | ((uint16_t)data[5] << 8);
    if (payload_length > DS5_DEBUG_MAX_PAYLOAD) {
        return -1;
    }
    total = (uint16_t)(payload_length + DS5_DEBUG_FRAME_OVERHEAD);
    if (length < total) {
        return -1;
    }
    expected_crc = (uint16_t)data[7 + payload_length] |
                   ((uint16_t)data[8 + payload_length] << 8);
    if (ds5_link_crc16(&data[1], (uint16_t)(6 + payload_length)) != expected_crc) {
        return -1;
    }
    frame->type = data[2];
    frame->flags = data[3];
    frame->length = payload_length;
    frame->sequence = data[6];
    memcpy(frame->payload, &data[7], payload_length);
    return total;
}
