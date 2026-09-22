// Host unit tests for the NuPhy Air60 V2 port.
//
// The real air60v2/ansi/side.c and common/config/config.c are compiled
// against stub QMK headers (stubs/). rgb_matrix_set_color is recorded per
// LED, the clock is fake_now_ms, and EEPROM writes are counted.
//
// side_led_frame() is what rgb_matrix_indicators_kb() calls once per RGB frame.
// LED indexes: 0-63 keys, 64-68 left strip (indicators), 69-73 right
// strip (battery bar, filling from 73 down to 69).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "quantum.h"
#include "common/config.h"
#include "common/wireless.h"
#include "common/system/housekeeping_timer.h"

// firmware state shared with side.c (defined in stubs.c)
extern bool     f_bat_hold;
extern bool     f_dial_sw_init_ok;
extern uint16_t rf_link_show_time;

// side.c public API
void side_led_frame(void);
void side_led_show(void);
void fn_status_show(void);
void set_side_rgb(uint8_t r, uint8_t g, uint8_t b);
void set_indicator_on_side(uint8_t r, uint8_t g, uint8_t b);
void set_notice_on_side(uint8_t r, uint8_t g, uint8_t b);
void side_mode_control(uint8_t dir);
void side_color_control(uint8_t dir);
void side_speed_control(uint8_t dir);
void side_brightness_control(uint8_t brighten);
void device_reset_init(void);
void device_reset_show(void);

// stubs.c state
extern uint32_t eeprom_write_count;
extern uint32_t kb_config_reset_count;
extern const uint8_t side_color_lib[9][3];

#define SIDE_FIRST 64
#define LEFT_FIRST 64
#define RIGHT_FIRST 69
#define LED_COUNT RGB_MATRIX_LED_COUNT

static int failures = 0, checks = 0;
#define CHECK(cond)                                                              \
    do {                                                                         \
        checks++;                                                                \
        if (!(cond)) {                                                           \
            failures++;                                                          \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);             \
        }                                                                        \
    } while (0)
#define CHECK_EQ(a, b)                                                                                   \
    do {                                                                                                 \
        checks++;                                                                                        \
        long _a = (long)(a), _b = (long)(b);                                                             \
        if (_a != _b) {                                                                                  \
            failures++;                                                                                  \
            printf("  FAIL %s:%d: %s == %s  (%ld != %ld)\n", __FILE__, __LINE__, #a, #b, _a, _b);        \
        }                                                                                                \
    } while (0)
#define SECTION(name) printf("== %s\n", name)

// ---- helpers ------------------------------------------------------------

static void frame(uint32_t dt_ms) {
    fake_now_ms += dt_ms;
    led_rec_reset();
    side_led_frame();
}

static int only_side_leds_written(void) {
    for (int i = 0; i < SIDE_FIRST; i++) {
        if (led_rec[i].writes) return 0;
    }
    return 1;
}

static int all_side_leds_written(void) {
    for (int i = SIDE_FIRST; i < LED_COUNT; i++) {
        if (!led_rec[i].writes) return 0;
    }
    return 1;
}

static int side_all_zero(void) {
    for (int i = SIDE_FIRST; i < LED_COUNT; i++) {
        if (led_rec[i].r || led_rec[i].g || led_rec[i].b) return 0;
    }
    return 1;
}

static int led_is(int i, uint8_t r, uint8_t g, uint8_t b) {
    return led_rec[i].r == r && led_rec[i].g == g && led_rec[i].b == b;
}

static int battery_lit_count(void) {
    int n = 0;
    for (int i = RIGHT_FIRST; i < LED_COUNT; i++) {
        if (led_rec[i].r || led_rec[i].g || led_rec[i].b) n++;
    }
    return n;
}

// a battery draw writes the right strip a second time after the effect pass
static int battery_drawn(void) {
    for (int i = RIGHT_FIRST; i < LED_COUNT; i++) {
        if (led_rec[i].writes < 2) return 0;
    }
    return 1;
}

static void setup_led_config(void) {
    // unique index per matrix position: 20*row + col (fits uint8_t)
    for (int r = 0; r < MATRIX_ROWS; r++)
        for (int c = 0; c < MATRIX_COLS; c++)
            g_led_config.matrix_co[r][c] = (uint8_t)(20 * r + c);
}

static void quiet_state(void) {
    // wired, no power-on animation, side effect off, battery hidden
    dev_info.link_mode                            = LINK_USB;
    dev_info.rf_state                             = RF_CONNECT;
    dev_info.rf_charge                            = 0;
    f_bat_hold                                    = false;
    f_dial_sw_init_ok                             = true;
    keyboard_config.common.power_on_animation     = 0;
    keyboard_config.lights.side_mode              = EFFECT_OFF;
    keyboard_config.lights.side_brightness        = 5;
    keyboard_config.lights.side_speed             = 2;
    keyboard_config.custom.battery_indicator_brightness = 100;
}

// ---- tests --------------------------------------------------------------

static void test_two_digit_leds(void) {
    SECTION("two-digit value display (tens on Tab row, ones on number row)");
    setup_led_config();
    // NUPHY_TENS_DIGIT_ROW is 2 on the Air60 V2 (no F-row)
    CHECK_EQ(NUPHY_TENS_DIGIT_ROW, 2);
    CHECK_EQ(two_digit_decimals_led(5), 40);   // Tab
    CHECK_EQ(two_digit_ones_led(5), 25);       // "5"
    CHECK_EQ(two_digit_decimals_led(10), 41);  // Q
    CHECK_EQ(two_digit_ones_led(10), 30);      // "0" is col 10
    CHECK_EQ(two_digit_decimals_led(99), 49);  // O
    CHECK_EQ(two_digit_ones_led(99), 29);      // "9"
    CHECK_EQ(two_digit_decimals_led(0), 40);
    CHECK_EQ(two_digit_ones_led(0), 30);
    CHECK_EQ(two_digit_decimals_led(100), 0);   // out of range -> Esc
    CHECK_EQ(two_digit_ones_led(100), 0);
    CHECK_EQ(get_led_index(5, 1), 101);         // WIN_LOCK position resolves through matrix_co
}

static void test_config_defaults(void) {
    SECTION("keyboard_config defaults and EEPROM init");
    eeprom_write_count = 0;
    custom_eeprom_init();  // blank EEPROM -> defaults + magic + one write
    CHECK_EQ(keyboard_config.been_initiated, NUPHY_CONFIG_INIT_MAGIC);
    CHECK_EQ(eeprom_write_count, 1);
    CHECK_EQ(keyboard_config.lights.side_mode, DEFAULT_SIDE_MODE);
    CHECK_EQ(keyboard_config.lights.side_brightness, DEFAULT_SIDE_BRIGHTNESS);
    CHECK_EQ(keyboard_config.lights.side_speed, DEFAULT_SIDE_SPEED);
    CHECK_EQ(keyboard_config.lights.side_static_color.hue, RGB_DEFAULT_COLOR);
    CHECK_EQ(keyboard_config.common.power_on_animation, 1);
    CHECK_EQ(keyboard_config.common.sleep_timeout, DEFAULT_SLEEP_TIMEOUT);
    CHECK_EQ(keyboard_config.common.sleep_toggle, 1);
    CHECK_EQ(keyboard_config.common.usb_sleep_toggle, 0);
    CHECK_EQ(keyboard_config.common.caps_indicator_type, CAPS_INDICATOR_SIDE);
    CHECK_EQ(keyboard_config.custom.battery_indicator_brightness, 100);
    CHECK_EQ(keyboard_config.custom.detect_numlock_state, 0);
    CHECK_EQ(keyboard_config.custom.battery_indicator_numeric, 0);
    CHECK_EQ(sizeof(keyboard_config_t), NUPHY_VIA_EEPROM_CUSTOM_CONFIG_SIZE);

    // second init on an initialised EEPROM keeps the values and does not write
    keyboard_config.lights.side_brightness = 1;
    save_config_to_eeprom();
    eeprom_write_count = 0;
    custom_eeprom_init();
    CHECK_EQ(keyboard_config.lights.side_brightness, 1);
    CHECK_EQ(eeprom_write_count, 0);

    // sleep timeout stays within 1..60 minutes
    keyboard_config.common.sleep_timeout = 1;
    adjust_sleep_timeout(0);
    CHECK_EQ(keyboard_config.common.sleep_timeout, 1);
    keyboard_config.common.sleep_timeout = 60;
    adjust_sleep_timeout(1);
    CHECK_EQ(keyboard_config.common.sleep_timeout, 60);
    keyboard_config.common.sleep_timeout = 5;
    CHECK_EQ(get_sleep_timeout(), 5UL * 60 * 1000 / TIMER_STEP);
}

static void test_strip_addressing(void) {
    SECTION("strip addressing: effect 64-73, indicators left, notices right");
    led_rec_reset();
    set_side_rgb(1, 2, 3);
    CHECK(only_side_leds_written());
    CHECK(all_side_leds_written());
    for (int i = SIDE_FIRST; i < LED_COUNT; i++) CHECK(led_is(i, 1, 2, 3));

    led_rec_reset();
    set_indicator_on_side(9, 8, 7);
    CHECK(only_side_leds_written());
    for (int i = LEFT_FIRST; i < RIGHT_FIRST; i++) CHECK(led_rec[i].writes == 1 && led_is(i, 9, 8, 7));
    for (int i = RIGHT_FIRST; i < LED_COUNT; i++) CHECK_EQ(led_rec[i].writes, 0);

    // OS-switch and sleep-toggle notices use the right strip, as on stock
    led_rec_reset();
    set_notice_on_side(4, 5, 6);
    CHECK(only_side_leds_written());
    for (int i = LEFT_FIRST; i < RIGHT_FIRST; i++) CHECK_EQ(led_rec[i].writes, 0);
    for (int i = RIGHT_FIRST; i < LED_COUNT; i++) CHECK(led_rec[i].writes == 1 && led_is(i, 4, 5, 6));
}

static void test_controls(void) {
    SECTION("side light key controls (mode / brightness / speed / color)");
    init_keyboard_config();
    keyboard_config.lights.side_mode = EFFECT_WAVE;
    eeprom_write_count               = 0;
    for (int i = 0; i < 4; i++) side_mode_control(1);
    CHECK_EQ(keyboard_config.lights.side_mode, EFFECT_OFF);
    side_mode_control(1);
    CHECK_EQ(keyboard_config.lights.side_mode, EFFECT_WAVE);  // wraps
    side_mode_control(0);
    CHECK_EQ(keyboard_config.lights.side_mode, EFFECT_OFF);   // wraps the other way
    CHECK_EQ(eeprom_write_count, 6);

    keyboard_config.lights.side_brightness = 4;
    side_brightness_control(1);
    side_brightness_control(1);
    side_brightness_control(1);
    CHECK_EQ(keyboard_config.lights.side_brightness, 5);      // clamped at 5
    for (int i = 0; i < 7; i++) side_brightness_control(0);
    CHECK_EQ(keyboard_config.lights.side_brightness, 0);      // clamped at 0

    keyboard_config.lights.side_speed = 2;
    side_speed_control(1);
    side_speed_control(1);
    side_speed_control(1);
    CHECK_EQ(keyboard_config.lights.side_speed, 0);           // fastest
    for (int i = 0; i < 6; i++) side_speed_control(0);
    CHECK_EQ(keyboard_config.lights.side_speed, 4);           // slowest
    keyboard_config.lights.side_speed = 200;                  // corrupt value is clamped, not indexed out of range
    side_speed_control(0);
    CHECK_EQ(keyboard_config.lights.side_speed, 3);

    keyboard_config.lights.side_mode             = EFFECT_STATIC;
    keyboard_config.lights.side_static_color.hue = 0;
    side_color_control(1);
    CHECK_EQ(keyboard_config.lights.side_static_color.hue, RGB_MATRIX_HUE_STEP);
    side_color_control(0);
    CHECK_EQ(keyboard_config.lights.side_static_color.hue, 0);
}

static void test_effects(void) {
    SECTION("side effects write exactly the two strips");
    init_keyboard_config();
    quiet_state();
    device_reset_init();
    keyboard_config.lights.side_static_color.hue = 168;
    keyboard_config.lights.side_static_color.sat = 255;

    // battery bar must not interfere: hide it by timing out the initial show
    keyboard_config.lights.side_mode = EFFECT_OFF;
    for (int i = 0; i < 100; i++) frame(100);
    CHECK(side_all_zero());

    const uint8_t modes[] = {EFFECT_WAVE, EFFECT_MIX, EFFECT_STATIC, EFFECT_BREATH};
    for (unsigned m = 0; m < sizeof(modes); m++) {
        keyboard_config.lights.side_mode       = modes[m];
        keyboard_config.lights.side_brightness = 5;
        int lit = 0;
        for (int i = 0; i < 10; i++) {
            frame(100);
            CHECK(only_side_leds_written());
            if (all_side_leds_written() && !side_all_zero()) lit++;
        }
        CHECK(lit > 0);
    }

    // static: brightness 5 is the picker colour unscaled, brightness 0 is dark
    keyboard_config.lights.side_mode       = EFFECT_STATIC;
    keyboard_config.lights.side_brightness = 5;
    rgb_t want                             = nuphy_picker_hsv_rgb(168, 255, 255);
    frame(100);
    for (int i = SIDE_FIRST; i < LED_COUNT; i++) CHECK(led_is(i, want.r, want.g, want.b));
    keyboard_config.lights.side_brightness = 0;
    frame(100);
    CHECK(all_side_leds_written() && side_all_zero());

    // off
    keyboard_config.lights.side_mode       = EFFECT_OFF;
    keyboard_config.lights.side_brightness = 5;
    frame(100);
    CHECK(all_side_leds_written() && side_all_zero());
}

static void test_power_on_animation(void) {
    SECTION("power-on animation runs on the strips only and finishes");
    init_keyboard_config();
    quiet_state();
    keyboard_config.common.power_on_animation = 1;
    keyboard_config.lights.side_mode          = EFFECT_OFF;
    dev_info.link_mode                        = LINK_RF_24;  // battery gated by rf_link_show_time
    rf_link_show_time                         = 0;
    device_reset_init();  // f_power_show = true

    int nonzero_frames = 0, frames_to_finish = -1, zero_run = 0;
    for (int i = 0; i < 3000; i++) {
        frame(20);
        CHECK(only_side_leds_written());
        if (!side_all_zero()) {
            nonzero_frames++;
            zero_run = 0;
        } else if (++zero_run == 50 && frames_to_finish < 0) {
            frames_to_finish = i;
        }
    }
    CHECK(nonzero_frames > 5);
    CHECK(frames_to_finish > 0 && frames_to_finish < 2000);
    printf("  animation: %d lit frames, quiet after frame %d (20 ms/frame)\n", nonzero_frames, frames_to_finish);

    // with the animation disabled the effect runs from the first frame
    device_reset_init();
    keyboard_config.common.power_on_animation = 0;
    keyboard_config.lights.side_mode          = EFFECT_STATIC;
    frame(100);
    CHECK(all_side_leds_written() && !side_all_zero());
}

static void test_battery_bar(void) {
    SECTION("battery bar (right strip, 73 -> 69) in wired mode");
    init_keyboard_config();
    quiet_state();
    device_reset_init();
    keyboard_config.lights.side_mode = EFFECT_OFF;

    // 55% -> three segments of side_color_lib[2] (yellow), rest dark
    dev_info.rf_battery = 55;
    f_bat_hold          = true;
    for (int i = 0; i < 15; i++) frame(100);  // debounce of percent change
    CHECK(battery_drawn());
    const uint8_t *c = side_color_lib[2];
    CHECK(led_is(73, c[0], c[1], c[2]));
    CHECK(led_is(72, c[0], c[1], c[2]));
    CHECK(led_is(71, c[0], c[1], c[2]));
    CHECK(led_is(70, 0, 0, 0));
    CHECK(led_is(69, 0, 0, 0));
    for (int i = LEFT_FIRST; i < RIGHT_FIRST; i++) CHECK_EQ(led_rec[i].writes, 1);  // left strip untouched by the bar

    // indicator brightness scales the colour
    keyboard_config.custom.battery_indicator_brightness = 50;
    frame(100);
    CHECK(led_is(73, c[0] * 50 / 100, c[1] * 50 / 100, c[2] * 50 / 100));
    keyboard_config.custom.battery_indicator_brightness = 100;

    // thresholds: <=20 one segment red, >80 five segments green
    dev_info.rf_battery = 15;
    for (int i = 0; i < 15; i++) frame(100);
    CHECK_EQ(battery_lit_count(), 1);
    CHECK(led_is(73, side_color_lib[0][0], side_color_lib[0][1], side_color_lib[0][2]));
    dev_info.rf_battery = 95;
    for (int i = 0; i < 15; i++) frame(100);
    CHECK_EQ(battery_lit_count(), 5);
    CHECK(led_is(69, side_color_lib[3][0], side_color_lib[3][1], side_color_lib[3][2]));

    // without hold the bar disappears after the show timeout
    f_bat_hold = false;
    for (int i = 0; i < 80; i++) frame(100);
    CHECK(!battery_drawn());
    CHECK(side_all_zero());

    // low battery (<10%, not charging): red blink on the bar, lights dimmed
    rgb_matrix_config.hsv.v                = 200;
    keyboard_config.lights.side_brightness = 5;
    dev_info.rf_battery                    = 5;
    int red = 0, dark = 0;
    for (int i = 0; i < 40; i++) {
        frame(100);
        if (battery_drawn()) {
            if (led_is(73, 0x80, 0, 0) && led_is(69, 0x80, 0, 0)) red++;
            if (led_is(73, 0, 0, 0)) dark++;
        }
    }
    CHECK(red > 0 && dark > 0);
    CHECK_EQ(rgb_matrix_config.hsv.v, RGB_MATRIX_VAL_STEP);
    CHECK_EQ(keyboard_config.lights.side_brightness, 1);

    // charging (not full) at 55 %: the three-segment yellow bar breathes
    dev_info.rf_battery = 55;
    for (int i = 0; i < 15; i++) frame(100);
    dev_info.rf_charge = 0x01;
    for (int i = 0; i < 15; i++) frame(100);
    int bright_max = 0, bright_min = 255;
    for (int i = 0; i < 40; i++) {
        frame(30);
        CHECK(only_side_leds_written());
        CHECK(led_is(70, 0, 0, 0) && led_is(69, 0, 0, 0));           // segments above the level stay dark
        CHECK(led_rec[73].r == led_rec[73].g && led_rec[73].b == 0);   // yellow, scaled
        CHECK(led_rec[72].r == led_rec[73].r && led_rec[71].r == led_rec[73].r);
        if (led_rec[73].r > bright_max) bright_max = led_rec[73].r;
        if (led_rec[73].r < bright_min) bright_min = led_rec[73].r;
    }
    CHECK(bright_max > bright_min + 64);
    printf("  charging breathe at 55 %%: red %d..%d\n", bright_min, bright_max);
    // full (0x03): a steady full bar in the 100 % colour, no pulsing
    dev_info.rf_charge = 0x03;
    for (int i = 0; i < 15; i++) frame(100);
    for (int i = 0; i < 20; i++) {
        frame(100);
        CHECK(battery_lit_count() == 5);
        for (int k = RIGHT_FIRST; k < LED_COUNT; k++) CHECK(led_is(k, side_color_lib[3][0], side_color_lib[3][1], side_color_lib[3][2]));
    }
    dev_info.rf_charge = 0;
}

static void test_wireless_gating(void) {
    SECTION("battery bar is hidden until the RF link is shown and connected");
    quiet_state();
    keyboard_config.lights.side_mode = EFFECT_OFF;
    dev_info.link_mode               = LINK_RF_24;
    dev_info.rf_battery              = 55;
    f_bat_hold                       = true;

    rf_link_show_time = 0;
    dev_info.rf_state = RF_CONNECT;
    for (int i = 0; i < 15; i++) frame(100);
    CHECK(!battery_drawn());

    rf_link_show_time = RF_LINK_SHOW_TIME;
    dev_info.rf_state = RF_PAIRING;
    for (int i = 0; i < 15; i++) frame(100);
    CHECK(!battery_drawn());

    dev_info.rf_state = RF_CONNECT;
    for (int i = 0; i < 15; i++) frame(100);
    CHECK(battery_drawn());
    CHECK_EQ(battery_lit_count(), 3);
}

static void test_housekeeping_hook(void) {
    SECTION("housekeeping hook draws the strips only while RGB Matrix is off");
    init_keyboard_config();
    quiet_state();
    keyboard_config.lights.side_mode = EFFECT_STATIC;
    rgb_matrix_config.enable         = true;
    fake_now_ms += 100;
    led_rec_reset();
    side_led_show();
    CHECK(!all_side_leds_written()); // the RGB frame draws them instead
    rgb_matrix_config.enable = false;
    fake_now_ms += 100;
    led_rec_reset();
    side_led_show();
    CHECK(all_side_leds_written() && !side_all_zero());
    rgb_matrix_config.enable = true;
}

static void test_fn_status(void) {
    SECTION("Fn held: link on Q/W/E/R/Y, US-JIS on Tab, only while RGB Matrix is on");
    setup_led_config(); // (2,c) -> LED 40 + c: Tab 40, Q 41, W 42, E 43, R 44, Y 46
    rgb_matrix_config.enable             = true;
    keyboard_config.common.usjis_enabled = 1;
    dev_info.rf_state                    = RF_CONNECT;
    layer_state                          = 1u << 3; // Win Fn layer

    struct { uint8_t link; int led; uint8_t r, g, b; } cases[] = {
        {LINK_BT_1, 41, 0, 0, 0x80}, {LINK_BT_2, 42, 0, 0, 0x80}, {LINK_BT_3, 43, 0, 0, 0x80},
        {LINK_RF_24, 44, 0, 0x80, 0}, {LINK_USB, 46, 0x80, 0x80, 0x80},
    };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        dev_info.link_mode = cases[i].link;
        led_rec_reset();
        fn_status_show();
        CHECK(led_is(cases[i].led, cases[i].r, cases[i].g, cases[i].b));
        for (int k = 41; k <= 46; k++) if (k != cases[i].led) CHECK_EQ(led_rec[k].writes, 0);
        CHECK(led_is(40, 0, 0x80, 0)); // Tab green: US-JIS on
        for (int k = 0; k < 40; k++) CHECK_EQ(led_rec[k].writes, 0);
    }
    keyboard_config.common.usjis_enabled = 0;
    led_rec_reset();
    fn_status_show();
    CHECK(led_is(40, 0x80, 0, 0)); // Tab red: US-JIS off

    // wireless but not connected: the link key blinks with a 250 ms period, Tab stays
    dev_info.link_mode = LINK_RF_24;
    dev_info.rf_state  = RF_PAIRING;
    int lit = 0, dark = 0;
    for (int i = 0; i < 8; i++) {
        fake_now_ms += 125;
        led_rec_reset();
        fn_status_show();
        if (led_rec[44].writes) lit++; else dark++;
        CHECK(led_rec[40].writes == 1);
    }
    CHECK(lit > 0 && dark > 0);
    dev_info.rf_state = RF_CONNECT;

    // base layers and Mac Fn layer
    layer_state = 1u << 2;
    led_rec_reset();
    fn_status_show();
    CHECK(led_rec[40].writes == 0 && led_rec[44].writes == 0);
    layer_state = 1u << 1;
    led_rec_reset();
    fn_status_show();
    CHECK(led_rec[40].writes == 1 && led_rec[44].writes == 1);

    // RGB Matrix off: nothing at all
    rgb_matrix_config.enable = false;
    led_rec_reset();
    fn_status_show();
    for (int k = 0; k < LED_COUNT; k++) CHECK_EQ(led_rec[k].writes, 0);
    rgb_matrix_config.enable = true;
    layer_state              = 1;
}

static void test_device_reset(void) {
    SECTION("device reset blinks all LEDs and resets the config");
    kb_config_reset_count = 0;
    f_bat_hold            = true;
    uint32_t t0           = fake_now_ms;
    pwm_flush_count       = 0;
    led_rec_reset();
    device_reset_show();
    CHECK_EQ(pwm_flush_count, 6);
    CHECK_EQ(fake_now_ms - t0, 1200);
    CHECK_EQ(led_rec[0].writes, 6);
    CHECK_EQ(led_rec[LED_COUNT - 1].writes, 6);
    device_reset_init();
    CHECK_EQ(kb_config_reset_count, 1);
    CHECK(!f_bat_hold);
}

int main(void) {
    test_two_digit_leds();
    test_config_defaults();
    test_strip_addressing();
    test_controls();
    test_effects();
    test_power_on_animation();
    test_battery_bar();
    test_wireless_gating();
    test_housekeeping_hook();
    test_fn_status();
    test_device_reset();
    printf("\n%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
