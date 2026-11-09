#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "tusb.h"
#include "class/hid/hid_device.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "link_protocol.h"

extern const uint8_t ds5_hid_report_descriptor[];

#define DS5_HID_REPORT_DESCRIPTOR_SIZE 337

#define LINK_UART UART_NUM_1
#define LINK_TX_GPIO 13
#define LINK_RX_GPIO 14
#define LINK_RTS_GPIO 15
#define LINK_CTS_GPIO 16
#define LINK_READY_GPIO 17
#define LINK_BAUD_RATE 2000000

static const char *TAG = "ds5-s3-hid";
static uint8_t input_report[63];
static uint8_t output_report[64];

static const tusb_desc_device_t device_descriptor = {
    .bLength = sizeof(tusb_desc_device_t),
    .bDescriptorType = TUSB_DESC_DEVICE,
    .bcdUSB = 0x0200,
    .bDeviceClass = 0,
    .bDeviceSubClass = 0,
    .bDeviceProtocol = 0,
    .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
    .idVendor = 0x054C,
    .idProduct = 0x0CE6,
    .bcdDevice = 0x0100,
    .iManufacturer = 1,
    .iProduct = 2,
    .iSerialNumber = 3,
    .bNumConfigurations = 1,
};

enum {
    ITF_NUM_HID = 0,
    ITF_NUM_TOTAL,
};

static const uint8_t configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0,
                          TUD_CONFIG_DESC_LEN + TUD_HID_DESC_LEN,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, 0, false,
                       DS5_HID_REPORT_DESCRIPTOR_SIZE, 0x81, 64, 1),
};

static const char *string_descriptors[] = {
    (const char[]){0x09, 0x04},
    "Sony Interactive Entertainment",
    "Wireless Controller",
    "DS5-S3-HID-POC",
};

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    (void)instance;
    return ds5_hid_report_descriptor;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                               hid_report_type_t report_type, uint8_t *buffer,
                               uint16_t reqlen)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    uint16_t length = reqlen < 63 ? reqlen : 63;
    memcpy(buffer, input_report, length);
    return length;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                          hid_report_type_t report_type, uint8_t const *buffer,
                          uint16_t bufsize)
{
    (void)instance;
    (void)report_id;
    (void)report_type;
    uint16_t length = bufsize < sizeof(output_report) ? bufsize : sizeof(output_report);
    memcpy(output_report, buffer, length);
    ESP_LOGI(TAG, "HID OUT report id=0x%02x len=%u", report_id, length);
}

static void link_uart_init(void)
{
    const uart_config_t config = {
        .baud_rate = LINK_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_param_config(LINK_UART, &config));
    ESP_ERROR_CHECK(uart_set_pin(LINK_UART, LINK_TX_GPIO, LINK_RX_GPIO,
                                 LINK_RTS_GPIO, LINK_CTS_GPIO));
    ESP_ERROR_CHECK(uart_driver_install(LINK_UART, 1024, 1024, 0, NULL, 0));
    ESP_ERROR_CHECK(gpio_set_direction(LINK_READY_GPIO, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_set_level(LINK_READY_GPIO, 1));
    ESP_LOGI(TAG, "UART link ready: TX=%d RX=%d RTS=%d CTS=%d READY=%d baud=%d",
             LINK_TX_GPIO, LINK_RX_GPIO, LINK_RTS_GPIO, LINK_CTS_GPIO,
             LINK_READY_GPIO, LINK_BAUD_RATE);
}

static void link_send_hello(void)
{
    uint8_t encoded[DS5_LINK_MAX_PAYLOAD + 8];
    ds5_link_frame_t frame = {
        .type = DS5_LINK_HELLO,
        .flags = 0,
        .length = 0,
        .sequence = 0,
    };
    uint16_t length = ds5_link_encode(&frame, encoded, sizeof(encoded));
    if (length != 0) {
        uart_write_bytes(LINK_UART, encoded, length);
    }
}

static void link_uart_task(void *arg)
{
    (void)arg;
    uint8_t buffer[DS5_LINK_MAX_PAYLOAD + 8];
    uint16_t used = 0;

    while (true) {
        int count = uart_read_bytes(LINK_UART, &buffer[used], sizeof(buffer) - used,
                                    pdMS_TO_TICKS(20));
        if (count <= 0) {
            continue;
        }
        used += (uint16_t)count;
        while (used >= 8) {
            if (buffer[0] != DS5_LINK_SYNC) {
                memmove(buffer, &buffer[1], --used);
                continue;
            }
            uint16_t frame_length = (uint16_t)buffer[3] | ((uint16_t)buffer[4] << 8);
            uint16_t total = frame_length + 8;
            if (frame_length > DS5_LINK_MAX_PAYLOAD) {
                used = 0;
                break;
            }
            if (used < total) {
                break;
            }
            ds5_link_frame_t frame;
            if (ds5_link_decode(buffer, total, &frame) >= 0) {
                ESP_LOGI(TAG, "UART frame type=0x%02x len=%u seq=%u",
                         frame.type, frame.length, frame.sequence);
                if (frame.type == DS5_LINK_HELLO) {
                    ds5_link_frame_t ack = {
                        .type = DS5_LINK_HELLO_ACK,
                        .flags = 0,
                        .length = 0,
                        .sequence = frame.sequence,
                    };
                    uint8_t encoded[DS5_LINK_MAX_PAYLOAD + 8];
                    uint16_t length = ds5_link_encode(&ack, encoded, sizeof(encoded));
                    uart_write_bytes(LINK_UART, encoded, length);
                }
            }
            used -= total;
            if (used != 0) {
                memmove(buffer, &buffer[total], used);
            }
        }
    }
}

static void usb_task(void *arg)
{
    (void)arg;
    input_report[0] = 0x80;
    input_report[1] = 0x80;
    input_report[2] = 0x80;
    input_report[3] = 0x80;
    input_report[4] = 0x80;
    input_report[5] = 0x80;

    while (true) {
        if (tud_mounted() && tud_hid_ready()) {
            tud_hid_report(0x01, input_report, sizeof(input_report));
        }
        vTaskDelay(pdMS_TO_TICKS(4));
    }
}

void app_main(void)
{
    tinyusb_config_t tusb_cfg = TINYUSB_DEFAULT_CONFIG();

    tusb_cfg.descriptor.device = &device_descriptor;
    tusb_cfg.descriptor.full_speed_config = configuration_descriptor;
    tusb_cfg.descriptor.string = string_descriptors;
    tusb_cfg.descriptor.string_count = sizeof(string_descriptors) / sizeof(string_descriptors[0]);
#if (TUD_OPT_HIGH_SPEED)
    tusb_cfg.descriptor.high_speed_config = configuration_descriptor;
#endif

    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    link_uart_init();
    xTaskCreate(usb_task, "usb_task", 4096, NULL, 5, NULL);
    xTaskCreate(link_uart_task, "link_uart", 4096, NULL, 5, NULL);
    link_send_hello();
}
