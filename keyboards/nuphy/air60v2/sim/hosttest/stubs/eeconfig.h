#pragma once
#include <stdint.h>
#include <stdbool.h>
bool     eeconfig_is_enabled(void);
uint32_t eeconfig_read_kb_datablock(void *data, uint32_t offset, uint32_t length);
uint32_t eeconfig_update_kb_datablock(const void *data, uint32_t offset, uint32_t length);
