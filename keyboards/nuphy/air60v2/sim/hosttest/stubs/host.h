#pragma once
#include <stdint.h>
#include <stdbool.h>
typedef union { uint8_t raw; struct { bool num_lock : 1; bool caps_lock : 1; bool scroll_lock : 1; }; } led_t;
led_t host_keyboard_led_state(void);
