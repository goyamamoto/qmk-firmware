/*
Copyright 2023 @ Nuphy <https://nuphy.com/>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// Side lights of the Air60 V2: two 5-LED strips driven by the second
// IS31FL3733 as rgb_matrix LEDs 64-73. Effects run on both strips row by
// row, as in the stock NuPhy firmware. The left strip carries caps lock and
// the link state; the right strip carries the battery level and, as in the
// stock firmware, the OS-switch and sleep-toggle blink.

#include "ansi.h"
#include "common/config.h"
#include "common/wireless.h"
#include "common/core/usjis.h"
#include "config.h"
#include "host.h"
#include "rgb_matrix.h"
#include "side.h"
#include "timer.h"
#include "common/lighting/side_table.h"

#define SIDE_INDEX 64
#define SIDE_LINE 5

#define SIDE_WAVE EFFECT_WAVE
#define SIDE_MIX EFFECT_MIX
#define SIDE_STATIC EFFECT_STATIC
#define SIDE_BREATH EFFECT_BREATH
#define SIDE_OFF EFFECT_OFF

#define LIGHT_COLOR_MAX 8
#define LIGHT_SPEED_MAX 4

#define LOW_BAT_BLINK_PRIOD 500

// speed and brightness steps from the stock Air60 V2 firmware
static const uint8_t side_speed_table[5][5] = {
    [SIDE_WAVE] = {14, 19, 25, 32, 40}, [SIDE_MIX] = {14, 19, 25, 32, 40}, [SIDE_STATIC] = {50, 50, 50, 50, 50}, [SIDE_BREATH] = {14, 19, 25, 32, 40}, [SIDE_OFF] = {50, 50, 50, 50, 50},
};

static const uint8_t side_light_table[6] = {
    0, 48, 96, 144, 192, 255,
};

// one row across both strips: {left, right}
static const uint8_t side_row_index_tab[SIDE_LINE][2] = {
    {SIDE_INDEX + 4, SIDE_INDEX + 5}, {SIDE_INDEX + 3, SIDE_INDEX + 6}, {SIDE_INDEX + 2, SIDE_INDEX + 7}, {SIDE_INDEX + 1, SIDE_INDEX + 8}, {SIDE_INDEX + 0, SIDE_INDEX + 9},
};

static const uint8_t indicator_led_index_tab[SIDE_LINE] = {
    SIDE_INDEX + 0, SIDE_INDEX + 1, SIDE_INDEX + 2, SIDE_INDEX + 3, SIDE_INDEX + 4,
};

// the battery bar grows from the same end as the stock firmware
static const uint8_t battery_led_index_tab[SIDE_LINE] = {
    SIDE_INDEX + 9, SIDE_INDEX + 8, SIDE_INDEX + 7, SIDE_INDEX + 6, SIDE_INDEX + 5,
};

static bool     f_charging        = true;
static bool     f_charge_full     = false; // charger reports full (state 0x03)
static uint8_t  side_play_point   = 0;
static uint16_t side_play_cnt     = 0;
static uint32_t led_play_timer    = 0;
static uint8_t  low_bat_blink_cnt = 6;
static uint8_t  side_r, side_g, side_b;

static uint8_t key_pwm_tab[SIDE_LINE] = {0};
static uint8_t power_play_index       = 0;
static bool    f_power_show           = true;

extern DEV_INFO_STRUCT dev_info;
extern bool            f_bat_hold;
extern bool            f_dial_sw_init_ok;
extern uint16_t        rf_link_show_time;
extern void            kb_config_reset(void);

static void light_point_playing(uint8_t trend, uint8_t step, uint8_t len, uint8_t *point) {
    if (trend) {
        *point += step;
        if (*point >= len) {
            *point -= len;
        }
    } else {
        *point -= step;
        if (*point >= len) {
            *point = len - (255 - *point) - 1;
        }
    }
}

static void count_rgb_light(uint8_t light_temp) {
    uint16_t temp;

    temp   = light_temp * side_r + side_r;
    side_r = temp >> 8;

    temp   = light_temp * side_g + side_g;
    side_g = temp >> 8;

    temp   = light_temp * side_b + side_b;
    side_b = temp >> 8;
}

static uint8_t clamp_speed(uint8_t speed) {
    if (speed > LIGHT_SPEED_MAX) {
        return LIGHT_SPEED_MAX / 2;
    }
    return speed;
}

static uint8_t clamp_brightness(uint8_t brightness) {
    if (brightness > 5) {
        return 5;
    }
    return brightness;
}

static void set_segment_rgb(const uint8_t *indices, uint8_t count, uint8_t r, uint8_t g, uint8_t b) {
    for (uint8_t i = 0; i < count; i++) {
        rgb_matrix_set_color(indices[i], r, g, b);
    }
}

static void set_row_rgb(uint8_t row, uint8_t r, uint8_t g, uint8_t b) {
    rgb_matrix_set_color(side_row_index_tab[row][0], r, g, b);
    rgb_matrix_set_color(side_row_index_tab[row][1], r, g, b);
}

static void set_all_rows_rgb(uint8_t r, uint8_t g, uint8_t b) {
    for (uint8_t i = 0; i < SIDE_LINE; i++) {
        set_row_rgb(i, r, g, b);
    }
}

static void set_battery_rgb(uint8_t r, uint8_t g, uint8_t b) {
    set_segment_rgb(battery_led_index_tab, SIDE_LINE, r, g, b);
}

static void adjust_brightness(uint8_t *brightness, uint8_t brighten) {
    if (brighten) {
        if (*brightness == 5) {
            return;
        }
        (*brightness)++;
    } else {
        if (*brightness == 0) {
            return;
        }
        (*brightness)--;
    }

    save_config_to_eeprom();
}

static void adjust_speed(uint8_t *speed, uint8_t faster) {
    *speed = clamp_speed(*speed);

    if (faster) {
        if (*speed) {
            (*speed)--;
        }
    } else {
        if (*speed < LIGHT_SPEED_MAX) {
            (*speed)++;
        }
    }

    save_config_to_eeprom();
}

static void adjust_color(uint8_t mode, uint8_t *rgb_mode, uint8_t *color, uint8_t dir) {
    if (mode == SIDE_WAVE || mode == SIDE_BREATH || mode == SIDE_STATIC) {
        uint8_t delta = dir ? RGB_MATRIX_HUE_STEP : (uint8_t)(-RGB_MATRIX_HUE_STEP);

        keyboard_config.lights.side_static_color.hue += delta;
        save_config_to_eeprom();
        return;
    }

    if (*rgb_mode) {
        *rgb_mode = 0;
        *color    = dir ? 0 : LIGHT_COLOR_MAX - 1;
    } else if (dir) {
        (*color)++;
        if (*color >= LIGHT_COLOR_MAX) {
            *rgb_mode = 1;
            *color    = 0;
        }
    } else {
        (*color)--;
        if (*color >= LIGHT_COLOR_MAX) {
            *rgb_mode = 1;
            *color    = 0;
        }
    }

    save_config_to_eeprom();
}

static void adjust_mode(uint8_t *mode, uint8_t dir, uint8_t *play_point) {
    if (dir) {
        (*mode)++;
        if (*mode > SIDE_OFF) {
            *mode = 0;
        }
    } else {
        if (*mode > 0) {
            (*mode)--;
        } else {
            *mode = SIDE_OFF;
        }
    }

    *play_point = 0;
    save_config_to_eeprom();
}

static bool consume_animation_step(uint8_t mode, uint8_t speed, uint16_t *play_cnt) {
    speed = clamp_speed(speed);

    if (*play_cnt <= side_speed_table[mode][speed]) {
        return false;
    }

    *play_cnt -= side_speed_table[mode][speed];
    if (*play_cnt > 20) {
        *play_cnt = 0;
    }

    return true;
}

static void render_wave(uint8_t brightness, uint8_t speed) {
    uint8_t play_index;

    if (!consume_animation_step(SIDE_WAVE, speed, &side_play_cnt)) {
        return;
    }

    brightness = clamp_brightness(brightness);

    light_point_playing(0, 2, WAVE_TAB_LEN, &side_play_point);

    play_index = side_play_point;

    for (uint8_t i = 0; i < SIDE_LINE; i++) {
        rgb_t rgb = nuphy_picker_hsv_rgb(keyboard_config.lights.side_static_color.hue, keyboard_config.lights.side_static_color.sat, 255);

        side_r = rgb.r;
        side_g = rgb.g;
        side_b = rgb.b;
        light_point_playing(1, 12, WAVE_TAB_LEN, &play_index);
        count_rgb_light(wave_data_tab[play_index]);

        count_rgb_light(side_light_table[brightness]);
        set_row_rgb(i, side_r, side_g, side_b);
    }
}

static void render_mix(uint8_t brightness, uint8_t speed) {
    if (!consume_animation_step(SIDE_MIX, speed, &side_play_cnt)) {
        return;
    }

    brightness = clamp_brightness(brightness);

    light_point_playing(1, 1, FLOW_COLOR_TAB_LEN, &side_play_point);

    side_r = flow_rainbow_color_tab[side_play_point][0];
    side_g = flow_rainbow_color_tab[side_play_point][1];
    side_b = flow_rainbow_color_tab[side_play_point][2];

    count_rgb_light(side_light_table[brightness]);
    set_all_rows_rgb(side_r, side_g, side_b);
}

static void render_breathe(uint8_t brightness, uint8_t speed) {
    if (!consume_animation_step(SIDE_BREATH, speed, &side_play_cnt)) {
        return;
    }

    brightness = clamp_brightness(brightness);

    light_point_playing(0, 1, BREATHE_TAB_LEN, &side_play_point);

    rgb_t rgb = nuphy_picker_hsv_rgb(keyboard_config.lights.side_static_color.hue, keyboard_config.lights.side_static_color.sat, 255);

    side_r = rgb.r;
    side_g = rgb.g;
    side_b = rgb.b;

    count_rgb_light(breathe_data_tab[side_play_point]);
    count_rgb_light(side_light_table[brightness]);
    set_all_rows_rgb(side_r, side_g, side_b);
}

static void render_static(uint8_t brightness) {
    rgb_t rgb = nuphy_static_picker_rgb(keyboard_config.lights.side_static_color.hue, keyboard_config.lights.side_static_color.sat, brightness);

    side_r = rgb.r;
    side_g = rgb.g;
    side_b = rgb.b;

    count_rgb_light(side_light_table[clamp_brightness(brightness)]);
    set_all_rows_rgb(side_r, side_g, side_b);
}

static void render_side_effect(void) {
    switch (keyboard_config.lights.side_mode) {
        case SIDE_WAVE:
            render_wave(keyboard_config.lights.side_brightness, keyboard_config.lights.side_speed);
            break;
        case SIDE_MIX:
            render_mix(keyboard_config.lights.side_brightness, keyboard_config.lights.side_speed);
            break;
        case SIDE_STATIC:
            render_static(keyboard_config.lights.side_brightness);
            break;
        case SIDE_BREATH:
            render_breathe(keyboard_config.lights.side_brightness, keyboard_config.lights.side_speed);
            break;
        case SIDE_OFF:
        default:
            set_all_rows_rgb(0, 0, 0);
            break;
    }
}

void side_rgb_refresh(void) {
    rgb_matrix_update_pwm_buffers();
}

void side_brightness_control(uint8_t brighten) {
#if !NUPHY_SIDE_LIGHTING_ENABLED
    return;
#endif
    adjust_brightness(&keyboard_config.lights.side_brightness, brighten);
}

void side_speed_control(uint8_t fast) {
#if !NUPHY_SIDE_LIGHTING_ENABLED
    return;
#endif
    adjust_speed(&keyboard_config.lights.side_speed, fast);
}

void side_color_control(uint8_t dir) {
#if !NUPHY_SIDE_LIGHTING_ENABLED
    return;
#endif
    adjust_color(keyboard_config.lights.side_mode, &keyboard_config.lights.side_rgb, &keyboard_config.lights.side_color, dir);
}

void side_mode_control(uint8_t dir) {
#if !NUPHY_SIDE_LIGHTING_ENABLED
    return;
#endif
    adjust_mode(&keyboard_config.lights.side_mode, dir, &side_play_point);
}

// The Air60 V2 has no ambient light. keys.c calls these for the AMBIENT_*
// keycodes, so they must exist; they do nothing here.
void ambient_brightness_control(uint8_t brighten) {}
void ambient_speed_control(uint8_t fast) {}
void ambient_color_control(uint8_t dir) {}
void ambient_mode_control(uint8_t dir) {}

void set_side_rgb(uint8_t r, uint8_t g, uint8_t b) {
#if !NUPHY_SIDE_LIGHTING_ENABLED
    return;
#endif
    set_all_rows_rgb(r, g, b);
}

void set_indicator_on_side(uint8_t r, uint8_t g, uint8_t b) {
    set_segment_rgb(indicator_led_index_tab, SIDE_LINE, r, g, b);
}

// OS-switch and sleep-toggle notices go to the right strip (stock behaviour)
void set_notice_on_side(uint8_t r, uint8_t g, uint8_t b) {
    set_battery_rgb(r, g, b);
}

// charging: the level bar (same segments and colour as when not charging)
// breathes, so the level stays readable while the motion says "charging"
static void bat_charging_breathe(uint8_t bat_end_led, uint8_t r, uint8_t g, uint8_t b) {
    static uint32_t interval_timer = 0;
    static uint8_t  play_point     = 0;

    if (timer_elapsed32(interval_timer) > 30) {
        interval_timer = timer_read32();
        light_point_playing(0, 2, BREATHE_TAB_LEN, &play_point);
    }

    side_r = r;
    side_g = g;
    side_b = b;
    count_rgb_light(breathe_data_tab[play_point]);
    for (uint8_t i = 0; i < SIDE_LINE; i++) {
        if (i <= bat_end_led) {
            rgb_matrix_set_color(battery_led_index_tab[i], side_r, side_g, side_b);
        } else {
            rgb_matrix_set_color(battery_led_index_tab[i], 0x00, 0x00, 0x00);
        }
    }
}

static void low_bat_show(void) {
    static uint32_t interval_timer = 0;

    side_r = 0x80;
    side_g = 0x00;
    side_b = 0x00;

    if (low_bat_blink_cnt) {
        if (timer_elapsed32(interval_timer) > (LOW_BAT_BLINK_PRIOD >> 1)) {
            side_r = 0x00;
            side_g = 0x00;
            side_b = 0x00;
        }

        if (timer_elapsed32(interval_timer) >= LOW_BAT_BLINK_PRIOD) {
            interval_timer = timer_read32();
            low_bat_blink_cnt--;
        }
    }

    set_battery_rgb(side_r, side_g, side_b);
}

static void bat_percent_led(uint8_t bat_percent) {
    uint8_t bat_end_led = 0;
    uint8_t bat_r;
    uint8_t bat_g;
    uint8_t bat_b;

    if (bat_percent <= 20) {
        bat_end_led = 0;
        bat_r       = side_color_lib[0][0];
        bat_g       = side_color_lib[0][1];
        bat_b       = side_color_lib[0][2];
    } else if (bat_percent <= 40) {
        bat_end_led = 1;
        bat_r       = side_color_lib[1][0];
        bat_g       = side_color_lib[1][1];
        bat_b       = side_color_lib[1][2];
    } else if (bat_percent <= 60) {
        bat_end_led = 2;
        bat_r       = side_color_lib[2][0];
        bat_g       = side_color_lib[2][1];
        bat_b       = side_color_lib[2][2];
    } else if (bat_percent <= 80) {
        bat_end_led = 3;
        bat_r       = side_color_lib[4][0];
        bat_g       = side_color_lib[4][1];
        bat_b       = side_color_lib[4][2];
    } else {
        bat_end_led = 4;
        bat_r       = side_color_lib[3][0];
        bat_g       = side_color_lib[3][1];
        bat_b       = side_color_lib[3][2];
    }

    bat_r = bat_r * keyboard_config.custom.battery_indicator_brightness / 100;
    bat_g = bat_g * keyboard_config.custom.battery_indicator_brightness / 100;
    bat_b = bat_b * keyboard_config.custom.battery_indicator_brightness / 100;

    if (f_charging) {
        low_bat_blink_cnt = 6;
        if (f_charge_full) {
            // full: a steady full bar in the 100 % colour
            uint8_t r = side_color_lib[3][0] * keyboard_config.custom.battery_indicator_brightness / 100;
            uint8_t g = side_color_lib[3][1] * keyboard_config.custom.battery_indicator_brightness / 100;
            uint8_t b = side_color_lib[3][2] * keyboard_config.custom.battery_indicator_brightness / 100;
            set_battery_rgb(r, g, b);
        } else {
            bat_charging_breathe(bat_end_led, bat_r, bat_g, bat_b);
        }
    } else if (bat_percent < 10) {
        low_bat_show();
    } else {
        low_bat_blink_cnt = 6;
        for (uint8_t i = 0; i < SIDE_LINE; i++) {
            if (i <= bat_end_led) {
                rgb_matrix_set_color(battery_led_index_tab[i], bat_r, bat_g, bat_b);
            } else {
                rgb_matrix_set_color(battery_led_index_tab[i], 0x00, 0x00, 0x00);
            }
        }
    }
}

static void bat_led_show(void) {
    static bool     bat_show_flag    = true;
    static uint32_t bat_show_time    = 0;
    static uint32_t bat_sts_debounce = 0;
    static uint32_t bat_per_debounce = 0;
    static uint8_t  charge_state     = 0;
    static uint8_t  bat_percent      = 0;
    static bool     f_init           = true;

    if (dev_info.link_mode != LINK_USB) {
        if (rf_link_show_time < RF_LINK_SHOW_TIME) {
            return;
        }

        if (dev_info.rf_state != RF_CONNECT) {
            return;
        }
    }

    if (f_init) {
        f_init        = false;
        bat_show_time = timer_read32();
        charge_state  = dev_info.rf_charge;
        bat_percent   = dev_info.rf_battery;
    }

    if (charge_state != dev_info.rf_charge) {
        if (timer_elapsed32(bat_sts_debounce) > 1000) {
            if (((charge_state & 0x01) == 0) && ((dev_info.rf_charge & 0x01) != 0)) {
                bat_show_flag = true;
                f_charging    = true;
                bat_show_time = timer_read32();
            }
            charge_state = dev_info.rf_charge;
        }
    } else {
        bat_sts_debounce = timer_read32();

        if (f_charging) {
            if (timer_elapsed32(bat_show_time) > 10000) {
                bat_show_flag = false;
                f_charging    = false;
            }
        } else if (timer_elapsed32(bat_show_time) > 5000) {
            bat_show_flag = false;
        }

        f_charge_full = (charge_state == 0x03);
        if (f_charge_full) {
            f_charging = true;
        } else if (!(charge_state & 0x01)) {
            f_charging = false;
        }
    }

    if (bat_percent != dev_info.rf_battery) {
        if (timer_elapsed32(bat_per_debounce) > 1000) {
            bat_percent = dev_info.rf_battery;
        }
    } else {
        bat_per_debounce = timer_read32();

        if ((bat_percent < 10) && (!(charge_state & 0x01))) {
            bat_show_flag = true;
            bat_show_time = timer_read32();

            if (rgb_matrix_config.hsv.v > RGB_MATRIX_VAL_STEP) {
                rgb_matrix_config.hsv.v = RGB_MATRIX_VAL_STEP;
            }

            if (keyboard_config.lights.side_brightness > 1) {
                keyboard_config.lights.side_brightness = 1;
            }
        }
    }

    if (f_bat_hold || bat_show_flag) {
        bat_percent_led(bat_percent);
    }
}

void device_reset_show(void) {
    gpio_write_pin_high(DC_BOOST_PIN);
    gpio_write_pin_high(RGB_DRIVER_SDB1);
    gpio_write_pin_high(RGB_DRIVER_SDB2);

    for (int blink_cnt = 0; blink_cnt < 3; blink_cnt++) {
        rgb_matrix_set_color_all(0xFF, 0xFF, 0xFF);
        rgb_matrix_update_pwm_buffers();
        wait_ms(200);

        rgb_matrix_set_color_all(0x00, 0x00, 0x00);
        rgb_matrix_update_pwm_buffers();
        wait_ms(200);
    }
}

void device_reset_init(void) {
    side_play_point  = 0;
    side_play_cnt    = 0;
    led_play_timer   = timer_read32();
    power_play_index = 0;
    f_power_show     = true;
    f_bat_hold       = false;

    kb_config_reset();
}

void rgb_test_show(void) {
    gpio_write_pin_high(DC_BOOST_PIN);
    gpio_write_pin_high(RGB_DRIVER_SDB1);
    gpio_write_pin_high(RGB_DRIVER_SDB2);

    rgb_matrix_set_color_all(0xFF, 0x00, 0x00);
    rgb_matrix_update_pwm_buffers();
    wait_ms(1000);

    rgb_matrix_set_color_all(0x00, 0xFF, 0x00);
    rgb_matrix_update_pwm_buffers();
    wait_ms(1000);

    rgb_matrix_set_color_all(0x00, 0x00, 0xFF);
    rgb_matrix_update_pwm_buffers();
    wait_ms(1000);
}

// power-on animation: light the rows one by one and let them fade out
static void side_power_mode_show(void) {
    if (!consume_animation_step(SIDE_WAVE, keyboard_config.lights.side_speed, &side_play_cnt)) {
        return;
    }

    if (power_play_index < SIDE_LINE) {
        key_pwm_tab[power_play_index] = 0xFF;
        power_play_index++;
    }

    for (uint8_t i = 0; i < SIDE_LINE; i++) {
        if (keyboard_config.lights.side_mode == SIDE_MIX) {
            side_r = flow_rainbow_color_tab[side_play_point % FLOW_COLOR_TAB_LEN][0];
            side_g = flow_rainbow_color_tab[side_play_point % FLOW_COLOR_TAB_LEN][1];
            side_b = flow_rainbow_color_tab[side_play_point % FLOW_COLOR_TAB_LEN][2];
        } else {
            rgb_t rgb = nuphy_picker_hsv_rgb(keyboard_config.lights.side_static_color.hue, keyboard_config.lights.side_static_color.sat, 255);

            side_r = rgb.r;
            side_g = rgb.g;
            side_b = rgb.b;
        }

        count_rgb_light(key_pwm_tab[i]);
        count_rgb_light(side_light_table[2]);
        set_row_rgb(i, side_r, side_g, side_b);
    }

    for (uint8_t i = 0; i < SIDE_LINE; i++) {
        if (key_pwm_tab[i] & 0x80) {
            key_pwm_tab[i] -= 8;
        } else if (key_pwm_tab[i] & 0x40) {
            key_pwm_tab[i] -= 6;
        } else if (key_pwm_tab[i] & 0x20) {
            key_pwm_tab[i] -= 4;
        } else if (key_pwm_tab[i] & 0x10) {
            key_pwm_tab[i] -= 3;
        } else if (key_pwm_tab[i] & 0x08) {
            key_pwm_tab[i] -= 2;
        } else if (key_pwm_tab[i]) {
            key_pwm_tab[i]--;
        }
    }

    if (key_pwm_tab[SIDE_LINE - 1] == 1) {
        f_power_show      = false;
        rf_link_show_time = 0;
        f_charging        = true;
    }
}

// The common layer calls side_led_show() from housekeeping, between the RGB
// task's render and flush steps. Drawing there let the effect overwrite the
// caps lock indicator and the battery bar just before a flush, which showed
// as a flicker. The strips are therefore drawn once per RGB frame from
// rgb_matrix_indicators_kb() (side_led_frame), before the common indicators.
// QMK skips the indicator callbacks while RGB Matrix is off, so in that case
// the strips are still drawn from here; nothing else touches them then.
void side_led_show(void) {
    if (!rgb_matrix_is_enabled()) side_led_frame();
}

// While Fn is held, the keys that select a link show the current link and
// Tab shows the US-JIS mode: Q/W/E blue for BLE 1-3, R green for 2.4 GHz,
// Y white for USB (a wireless link blinks until it is connected); Tab green
// when US-JIS is on, red when off. Nothing is shown while RGB Matrix is
// off, for places where the keyboard must not light up.
void fn_status_show(void) {
    if (!rgb_matrix_is_enabled()) return;

    uint8_t layer = get_highest_layer(layer_state);
    if (layer == 0 || layer == 2) return;

    bool connected = dev_info.link_mode == LINK_USB || dev_info.rf_state == RF_CONNECT;
    if (connected || (timer_read32() / 250) % 2 == 0) {
        switch (dev_info.link_mode) {
            case LINK_BT_1: rgb_matrix_set_color(get_led_index(2, 1), 0x00, 0x00, 0x80); break;
            case LINK_BT_2: rgb_matrix_set_color(get_led_index(2, 2), 0x00, 0x00, 0x80); break;
            case LINK_BT_3: rgb_matrix_set_color(get_led_index(2, 3), 0x00, 0x00, 0x80); break;
            case LINK_RF_24: rgb_matrix_set_color(get_led_index(2, 4), 0x00, 0x80, 0x00); break;
            case LINK_USB: rgb_matrix_set_color(get_led_index(2, 6), 0x80, 0x80, 0x80); break;
            default: break;
        }
    }

    if (usjis_is_enabled()) {
        rgb_matrix_set_color(get_led_index(2, 0), 0x00, 0x80, 0x00);
    } else {
        rgb_matrix_set_color(get_led_index(2, 0), 0x80, 0x00, 0x00);
    }
}

void side_led_frame(void) {
    static bool flag_power_on = true;
    uint32_t    elapsed;

    if (flag_power_on) {
        if (!f_dial_sw_init_ok) {
            return;
        }
        flag_power_on = false;
    }

    elapsed        = timer_elapsed32(led_play_timer);
    led_play_timer = timer_read32();
    side_play_cnt += elapsed;

    if (!keyboard_config.common.power_on_animation) {
        f_power_show = false;
    }

    if (f_power_show) {
        side_power_mode_show();
        return;
    }

#if !NUPHY_SIDE_LIGHTING_ENABLED
    return;
#endif

    render_side_effect();

#if (WORK_MODE == THREE_MODE)
    bat_led_show();
#endif
}
