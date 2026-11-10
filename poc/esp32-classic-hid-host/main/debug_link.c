#include <string.h>

#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "debug_link.h"
#include "debug_protocol.h"
#include "uart_link.h"

#define DEBUG_UART UART_NUM_0
#define DEBUG_BAUD_RATE 2000000
#define DEBUG_RX_BUFFER_SIZE 512
#define DEBUG_TIMEOUT_MS 1000

static const char *TAG = "debug-link";
static volatile bool debug_active;
static TickType_t last_activity;
static uint8_t rx_buffer[DEBUG_RX_BUFFER_SIZE];
static uint16_t rx_used;
static uint8_t tx_sequence;

static void debug_send(uint8_t type, const uint8_t *payload, uint16_t length, uint8_t sequence)
{
    ds5_debug_frame_t frame = {
        .type = type,
        .length = length,
        .sequence = sequence,
    };
    uint8_t encoded[DS5_DEBUG_MAX_PAYLOAD + DS5_DEBUG_FRAME_OVERHEAD];
    if (length > DS5_DEBUG_MAX_PAYLOAD) {
        return;
    }
    if (payload && length) {
        memcpy(frame.payload, payload, length);
    }
    uint16_t encoded_length = ds5_debug_encode(&frame, encoded, sizeof(encoded));
    if (encoded_length) {
        uart_write_bytes(DEBUG_UART, encoded, encoded_length);
    }
}

static void debug_handle_frame(const ds5_debug_frame_t *frame)
{
    last_activity = xTaskGetTickCount();
    switch (frame->type) {
    case DS5_DEBUG_HELLO:
        debug_send(DS5_DEBUG_HELLO_ACK, NULL, 0, frame->sequence);
        break;
    case DS5_DEBUG_CLAIM:
        debug_active = true;
        debug_send(DS5_DEBUG_STATUS, (const uint8_t[]){1}, 1, frame->sequence);
        ESP_LOGI(TAG, "WebSerial input override active");
        break;
    case DS5_DEBUG_RELEASE:
        debug_active = false;
        debug_send(DS5_DEBUG_STATUS, (const uint8_t[]){0}, 1, frame->sequence);
        ESP_LOGI(TAG, "WebSerial input override released");
        break;
    case DS5_DEBUG_INPUT_REPORT:
        if (debug_active && frame->length >= 2) {
            uint16_t report_id = (uint16_t)frame->payload[0] |
                                 ((uint16_t)frame->payload[1] << 8);
            uart_link_send_input(report_id, &frame->payload[2], frame->length - 2);
        }
        break;
    case DS5_DEBUG_PING:
        debug_send(DS5_DEBUG_PONG, frame->payload, frame->length, frame->sequence);
        break;
    default:
        break;
    }
}

static void debug_task(void *arg)
{
    (void)arg;
    last_activity = xTaskGetTickCount();
    while (true) {
        int count = uart_read_bytes(DEBUG_UART, &rx_buffer[rx_used],
                                    sizeof(rx_buffer) - rx_used, pdMS_TO_TICKS(20));
        if (count > 0) {
            rx_used += (uint16_t)count;
        }
        while (rx_used >= DS5_DEBUG_FRAME_OVERHEAD) {
            if (rx_buffer[0] != DS5_DEBUG_SYNC) {
                memmove(rx_buffer, &rx_buffer[1], --rx_used);
                continue;
            }
            uint16_t payload_length = (uint16_t)rx_buffer[4] |
                                      ((uint16_t)rx_buffer[5] << 8);
            uint16_t total = payload_length + DS5_DEBUG_FRAME_OVERHEAD;
            if (payload_length > DS5_DEBUG_MAX_PAYLOAD) {
                rx_used = 0;
                break;
            }
            if (rx_used < total) {
                break;
            }
            ds5_debug_frame_t frame;
            if (ds5_debug_decode(rx_buffer, total, &frame) == total) {
                debug_handle_frame(&frame);
            }
            rx_used -= total;
            if (rx_used) {
                memmove(rx_buffer, &rx_buffer[total], rx_used);
            }
        }
        if (debug_active && xTaskGetTickCount() - last_activity > pdMS_TO_TICKS(DEBUG_TIMEOUT_MS)) {
            debug_active = false;
            ESP_LOGW(TAG, "WebSerial input override timed out");
        }
    }
}

void debug_link_init(void)
{
    xTaskCreate(debug_task, "debug_link", 4096, NULL, 3, NULL);
}

bool debug_link_is_active(void)
{
    return debug_active;
}

void debug_link_send_status(uint8_t status)
{
    debug_send(DS5_DEBUG_STATUS, &status, 1, tx_sequence++);
}
