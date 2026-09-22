#pragma once
#include <stdint.h>
// Fake clock controlled by the test.
extern uint32_t fake_now_ms;
uint32_t timer_read32(void);
uint32_t timer_elapsed32(uint32_t last);
uint16_t timer_read(void);
uint16_t timer_elapsed(uint16_t last);
