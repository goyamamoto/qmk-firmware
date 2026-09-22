// Implementations of the QMK / common-layer symbols the units under test need.
#include <stdio.h>
#include <stdlib.h>
#include "quantum.h"
#include "common/wireless.h"
#include "eeconfig.h"

uint32_t fake_now_ms = 0;
uint32_t timer_read32(void) { return fake_now_ms; }
uint32_t timer_elapsed32(uint32_t last) { return fake_now_ms - last; }
uint16_t timer_read(void) { return (uint16_t)fake_now_ms; }
uint16_t timer_elapsed(uint16_t last) { return (uint16_t)(fake_now_ms - last); }
void     wait_ms(uint32_t ms) { fake_now_ms += ms; }

led_t host_led_state_value;
led_t host_keyboard_led_state(void) { return host_led_state_value; }

led_config_t g_led_config;
rgb_config_t rgb_matrix_config = {.enable = true, .hsv = {168, 255, 200}};
led_record_t led_rec[RGB_MATRIX_LED_COUNT];
uint32_t     pwm_flush_count = 0;

void led_rec_reset(void) { memset(led_rec, 0, sizeof(led_rec)); }
void rgb_matrix_set_color(int index, uint8_t r, uint8_t g, uint8_t b) {
    if (index < 0 || index >= RGB_MATRIX_LED_COUNT) {
        fprintf(stderr, "rgb_matrix_set_color out of range: %d\n", index);
        abort();
    }
    led_rec[index].r = r; led_rec[index].g = g; led_rec[index].b = b; led_rec[index].writes++;
}
void rgb_matrix_set_color_all(uint8_t r, uint8_t g, uint8_t b) {
    for (int i = 0; i < RGB_MATRIX_LED_COUNT; i++) rgb_matrix_set_color(i, r, g, b);
}
void    rgb_matrix_update_pwm_buffers(void) { pwm_flush_count++; }
bool    rgb_matrix_is_enabled(void) { return rgb_matrix_config.enable; }
uint8_t rgb_matrix_get_val(void) { return rgb_matrix_config.hsv.v; }

// fake EEPROM datablock
static uint8_t fake_eeprom[64];
uint32_t eeprom_write_count = 0;
bool     eeconfig_is_enabled(void) { return true; }
uint32_t eeconfig_read_kb_datablock(void *data, uint32_t offset, uint32_t length) { memcpy(data, fake_eeprom + offset, length); return length; }
uint32_t eeconfig_update_kb_datablock(const void *data, uint32_t offset, uint32_t length) { memcpy(fake_eeprom + offset, data, length); eeprom_write_count++; return length; }

// state owned elsewhere in the firmware
DEV_INFO_STRUCT dev_info = {.link_mode = LINK_USB, .rf_state = RF_CONNECT, .rf_battery = 100};
bool            f_bat_hold        = false;
bool            f_dial_sw_init_ok = true;
bool            f_deep_sleep_show = false;
bool            f_usb_sleep_show  = false;
uint16_t        rf_link_show_time = 0;
uint32_t        kb_config_reset_count = 0;
bool            f_usjis_show          = false;

uint32_t layer_state = 1;
uint8_t  get_highest_layer(uint32_t state) {
    uint8_t l = 0;
    while (state >>= 1) l++;
    return l;
}
void            kb_config_reset(void) { kb_config_reset_count++; }

// ---- keyboard report model (US-JIS tests) ----
static uint8_t  real_mods, weak_mods;
static uint8_t  keys_down[6];
report_snapshot_t report_log[REPORT_LOG_MAX];
uint32_t         report_log_count = 0;

uint8_t get_mods(void) { return real_mods; }
void    set_mods(uint8_t m) { real_mods = m; }
void    add_mods(uint8_t m) { real_mods |= m; }
void    del_mods(uint8_t m) { real_mods &= ~m; }
uint8_t get_weak_mods(void) { return weak_mods; }
void    add_weak_mods(uint8_t m) { weak_mods |= m; }
void    del_weak_mods(uint8_t m) { weak_mods &= ~m; }
void send_keyboard_report(void) {
    if (report_log_count >= REPORT_LOG_MAX) return;
    report_snapshot_t *r = &report_log[report_log_count++];
    r->mods = real_mods | weak_mods;
    memcpy(r->keys, keys_down, sizeof(keys_down));
}
void register_code(uint8_t code) {
    for (int i = 0; i < 6; i++) if (keys_down[i] == code) { send_keyboard_report(); return; }
    for (int i = 0; i < 6; i++) if (!keys_down[i]) { keys_down[i] = code; break; }
    send_keyboard_report();
}
void unregister_code(uint8_t code) {
    for (int i = 0; i < 6; i++) if (keys_down[i] == code) keys_down[i] = 0;
    send_keyboard_report();
}
void report_model_reset(void) { real_mods = weak_mods = 0; memset(keys_down, 0, sizeof(keys_down)); report_log_count = 0; }
