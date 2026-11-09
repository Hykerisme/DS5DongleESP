idf_component_register(SRCS "main.c" "usb_descriptors.c" "../../link_protocol.c"
                       INCLUDE_DIRS "." "../.."
                       PRIV_REQUIRES esp_tinyusb esp_driver_uart)
