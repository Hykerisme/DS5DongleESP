idf_component_register(SRCS "app_main.c" "bt_host.c" "esp_hid_gap.c" "uart_link.c" "debug_link.c" "../../link_protocol.c" "../../debug_protocol.c"
                       INCLUDE_DIRS "." "../.."
                       REQUIRES esp_hid nvs_flash
                       PRIV_REQUIRES esp_driver_uart)
