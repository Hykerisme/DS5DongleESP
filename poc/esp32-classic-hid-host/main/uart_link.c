#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "link_protocol.h"

#define LINK_UART UART_NUM_2
#define LINK_TX_GPIO 17
#define LINK_RX_GPIO 16
#define LINK_RTS_GPIO 15
#define LINK_CTS_GPIO 4
#define LINK_READY_GPIO 2
#define LINK_BAUD_RATE 2000000

static const char *TAG = "link";

void uart_link_init(void)
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
    ESP_ERROR_CHECK(uart_driver_install(LINK_UART, 4096, 4096, 0, NULL, 0));
    ESP_ERROR_CHECK(gpio_set_direction(LINK_READY_GPIO, GPIO_MODE_INPUT));
    ESP_LOGI(TAG, "UART2 configured: TX=%d RX=%d RTS=%d CTS=%d READY=%d baud=%d",
             LINK_TX_GPIO, LINK_RX_GPIO, LINK_RTS_GPIO, LINK_CTS_GPIO,
             LINK_READY_GPIO, LINK_BAUD_RATE);
}
