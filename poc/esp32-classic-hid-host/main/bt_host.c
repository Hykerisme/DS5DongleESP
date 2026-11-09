#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_bt.h"
#include "esp_hid_gap.h"
#include "esp_hidh.h"
#include "esp_log.h"
#include "esp_event.h"

static const char *TAG = "bt-host";

static void print_addr(const uint8_t *bda)
{
    if (bda) {
        ESP_LOGI(TAG, "BD_ADDR %02x:%02x:%02x:%02x:%02x:%02x",
                 bda[0], bda[1], bda[2], bda[3], bda[4], bda[5]);
    }
}

static void hidh_callback(void *handler_args, esp_event_base_t base,
                          int32_t id, void *event_data)
{
    (void)handler_args;
    (void)base;
    esp_hidh_event_t event = (esp_hidh_event_t)id;
    esp_hidh_event_data_t *param = event_data;

    switch (event) {
    case ESP_HIDH_START_EVENT:
        ESP_LOGI(TAG, "HID host started: %s", esp_err_to_name(param->start.status));
        break;
    case ESP_HIDH_OPEN_EVENT:
        print_addr(esp_hidh_dev_bda_get(param->open.dev));
        ESP_LOGI(TAG, "OPEN status=%s name=%s", esp_err_to_name(param->open.status),
                 esp_hidh_dev_name_get(param->open.dev));
        if (param->open.status == ESP_OK) {
            esp_hidh_dev_dump(param->open.dev, stdout);
        }
        break;
    case ESP_HIDH_INPUT_EVENT:
        print_addr(esp_hidh_dev_bda_get(param->input.dev));
        ESP_LOGI(TAG, "INPUT report=%u map=%u len=%u", param->input.report_id,
                 param->input.map_index, param->input.length);
        ESP_LOG_BUFFER_HEX(TAG, param->input.data, param->input.length);
        break;
    case ESP_HIDH_FEATURE_EVENT:
        print_addr(esp_hidh_dev_bda_get(param->feature.dev));
        ESP_LOGI(TAG, "FEATURE report=%u map=%u len=%u status=%s",
                 param->feature.report_id, param->feature.map_index,
                 param->feature.length, esp_err_to_name(param->feature.status));
        ESP_LOG_BUFFER_HEX(TAG, param->feature.data, param->feature.length);
        break;
    case ESP_HIDH_CLOSE_EVENT:
        print_addr(esp_hidh_dev_bda_get(param->close.dev));
        ESP_LOGI(TAG, "CLOSE status=%s", esp_err_to_name(param->close.status));
        esp_hidh_dev_free(param->close.dev);
        break;
    default:
        ESP_LOGI(TAG, "HID event %d", event);
        break;
    }
}

static void scan_task(void *arg)
{
    (void)arg;
    size_t result_count = 0;
    esp_hid_scan_result_t *results = NULL;
    ESP_LOGI(TAG, "Scanning Classic Bluetooth HID devices");
    ESP_ERROR_CHECK(esp_hid_scan(5, &result_count, &results));

    esp_hid_scan_result_t *candidate = NULL;
    for (esp_hid_scan_result_t *result = results; result; result = result->next) {
        ESP_LOGI(TAG, "found transport=%d name=%s rssi=%d",
                 result->transport, result->name ? result->name : "", result->rssi);
        if (result->transport == ESP_HID_TRANSPORT_BT && candidate == NULL) {
            candidate = result;
        }
    }
    if (candidate) {
        ESP_LOGI(TAG, "opening Classic Bluetooth HID device");
        esp_hidh_dev_t *dev = esp_hidh_dev_open(candidate->bda, candidate->transport, 0);
        if (dev == NULL) {
            ESP_LOGE(TAG, "failed to open Classic Bluetooth HID device");
        }
    } else {
        ESP_LOGW(TAG, "no Classic Bluetooth HID device found");
    }
    esp_hid_scan_results_free(results);
    vTaskDelete(NULL);
}

void bt_host_init(void)
{
    ESP_ERROR_CHECK(esp_hid_gap_init(HIDH_BTDM_MODE));
    esp_hidh_config_t config = {
        .callback = hidh_callback,
        .event_stack_size = 4096,
        .callback_arg = NULL,
    };
    ESP_ERROR_CHECK(esp_hidh_init(&config));
    xTaskCreate(scan_task, "bt_scan", 6144, NULL, 5, NULL);
}
