#include "esp_err.h"
#include "esp_log.h"
#include "bt_host.h"
#include "uart_link.h"
#include "nvs_flash.h"
#include "esp_log.h"

static const char *TAG = "app";

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    uart_link_init();
    ESP_LOGI(TAG, "starting ESP32 Classic Bluetooth HID Host POC-2");
    bt_host_init();
}
