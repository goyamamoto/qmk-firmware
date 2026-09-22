// Host-test stand-in for QMK's rgb_matrix.h. Every set_color call is recorded.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "color.h"

#ifndef RGB_MATRIX_HUE_STEP
#    define RGB_MATRIX_HUE_STEP 8
#endif
#ifndef RGB_MATRIX_VAL_STEP
#    define RGB_MATRIX_VAL_STEP 16
#endif
#ifndef RGB_MATRIX_MAXIMUM_BRIGHTNESS
#    define RGB_MATRIX_MAXIMUM_BRIGHTNESS 255
#endif
#ifndef RGB_MATRIX_LED_COUNT
#    define RGB_MATRIX_LED_COUNT 74
#endif
#define NO_LED 255

typedef struct { uint8_t x; uint8_t y; } led_point_t;
typedef struct {
    uint8_t     matrix_co[MATRIX_ROWS][MATRIX_COLS];
    led_point_t point[RGB_MATRIX_LED_COUNT];
    uint8_t     flags[RGB_MATRIX_LED_COUNT];
} led_config_t;
extern led_config_t g_led_config;

typedef struct { bool enable; uint8_t mode; hsv_t hsv; uint8_t speed; uint8_t flags; } rgb_config_t;
extern rgb_config_t rgb_matrix_config;

// recorder
typedef struct { uint8_t r, g, b; uint32_t writes; } led_record_t;
extern led_record_t led_rec[RGB_MATRIX_LED_COUNT];
extern uint32_t     pwm_flush_count;
void led_rec_reset(void);

void    rgb_matrix_set_color(int index, uint8_t r, uint8_t g, uint8_t b);
void    rgb_matrix_set_color_all(uint8_t r, uint8_t g, uint8_t b);
void    rgb_matrix_update_pwm_buffers(void);
bool    rgb_matrix_is_enabled(void);
uint8_t rgb_matrix_get_val(void);
