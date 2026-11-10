#pragma once

#include <stdbool.h>
#include <stdint.h>

void debug_link_init(void);
bool debug_link_is_active(void);
void debug_link_send_status(uint8_t status);
void debug_link_send_status_detail(const uint8_t *payload, uint16_t length);
void debug_link_send_scan_event(const uint8_t *payload, uint16_t length);
void debug_link_send_scan_end(void);
