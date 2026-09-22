/*
Copyright 2023 @ Nuphy <https://nuphy.com/>

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "action.h"
#include "color.h"
#include "common/config.h"
#include "common/keyboard.h"
#include "common/keys.h"
#include "common/lighting/highlight.h"
#include "common/wireless.h"
#include "config.h"
#include "host.h"
#include "is31fl3733.h"
#include "keyboard.h"
#include "mcu_pwr.h"
#include "quantum.h"
#include "rgb_matrix.h"
#include "side.h"

#ifdef VIA_ENABLE
#    include "eeprom.h"
#    include "via.h"
#else
#    include "eeconfig.h"
#endif

bool process_record_kb(uint16_t keycode, keyrecord_t *record) {
    if (!process_record_user(keycode, record)) {
        return false;
    }

    return process_record_nuphy(keycode, record);
}

void keyboard_post_init_kb(void) {
    keyboard_post_init_nuphy();
    keyboard_post_init_user();
}

void housekeeping_task_kb(void) {
    housekeeping_task_nuphy();
    housekeeping_task_user();
}

bool rgb_matrix_indicators_kb(void) {
    side_led_frame(); // side strips first, so the indicators below draw over the effect
    rgb_matrix_indicators_nuphy();
    fn_status_show();
    return rgb_matrix_indicators_user();
}

bool rgb_matrix_indicators_advanced_kb(uint8_t led_min, uint8_t led_max) {
    nuphy_highlight_custom_keys(led_min, led_max);

    return rgb_matrix_indicators_advanced_user(led_min, led_max);
}

void gpio_init(void) {
    pwr_rgb_led_on();

#if (WORK_MODE == THREE_MODE)
    gpio_set_pin_output(NRF_WAKEUP_PIN);
    gpio_write_pin_high(NRF_WAKEUP_PIN);

    gpio_set_pin_input_high(NRF_TEST_PIN);

    gpio_set_pin_output(NRF_RESET_PIN);
    gpio_write_pin_low(NRF_RESET_PIN);
    wait_ms(50);
    gpio_write_pin_high(NRF_RESET_PIN);

    gpio_set_pin_input_high(DEVICE_MODE_PIN);
#endif
    gpio_set_pin_input_high(OS_MODE_PIN);

    gpio_set_pin_output(DC_BOOST_PIN);
    gpio_write_pin_high(DC_BOOST_PIN);
}

const is31fl3733_led_t PROGMEM g_is31fl3733_leds[IS31FL3733_LED_COUNT] = {
    {0, SW1_CS16, SW2_CS16, SW3_CS16}, // "Esc"
    {0, SW1_CS2, SW2_CS2, SW3_CS2}, // "!1"
    {0, SW1_CS3, SW2_CS3, SW3_CS3}, // "@2"
    {0, SW1_CS4, SW2_CS4, SW3_CS4}, // "#3"
    {0, SW1_CS5, SW2_CS5, SW3_CS5}, // "$4"
    {0, SW1_CS6, SW2_CS6, SW3_CS6}, // "%5"
    {0, SW1_CS7, SW2_CS7, SW3_CS7}, // "^6"
    {0, SW1_CS8, SW2_CS8, SW3_CS8}, // "&7"
    {0, SW1_CS9, SW2_CS9, SW3_CS9}, // "*8"
    {0, SW1_CS10, SW2_CS10, SW3_CS10}, // "(9"
    {0, SW1_CS11, SW2_CS11, SW3_CS11}, // ")0"
    {1, SW4_CS1, SW5_CS1, SW6_CS1}, // "_-"
    {1, SW4_CS2, SW5_CS2, SW6_CS2}, // "+="
    {1, SW4_CS3, SW5_CS3, SW6_CS3}, // "Backsp"
    {0, SW4_CS1, SW5_CS1, SW6_CS1}, // "Tab"
    {0, SW4_CS2, SW5_CS2, SW6_CS2}, // "Q"
    {0, SW4_CS3, SW5_CS3, SW6_CS3}, // "W"
    {0, SW4_CS4, SW5_CS4, SW6_CS4}, // "E"
    {0, SW4_CS5, SW5_CS5, SW6_CS5}, // "R"
    {0, SW4_CS6, SW5_CS6, SW6_CS6}, // "T"
    {0, SW4_CS7, SW5_CS7, SW6_CS7}, // "Y"
    {0, SW4_CS8, SW5_CS8, SW6_CS8}, // "U"
    {0, SW4_CS9, SW5_CS9, SW6_CS9}, // "I"
    {0, SW4_CS10, SW5_CS10, SW6_CS10}, // "O"
    {0, SW4_CS11, SW5_CS11, SW6_CS11}, // "P"
    {1, SW7_CS1, SW8_CS1, SW9_CS1}, // "{["
    {1, SW7_CS2, SW8_CS2, SW9_CS2}, // "}]"
    {1, SW7_CS3, SW8_CS3, SW9_CS3}, // "|\\"
    {0, SW7_CS1, SW8_CS1, SW9_CS1}, // "Caps"
    {0, SW7_CS2, SW8_CS2, SW9_CS2}, // "A"
    {0, SW7_CS3, SW8_CS3, SW9_CS3}, // "S"
    {0, SW7_CS4, SW8_CS4, SW9_CS4}, // "D"
    {0, SW7_CS5, SW8_CS5, SW9_CS5}, // "F"
    {0, SW7_CS6, SW8_CS6, SW9_CS6}, // "G"
    {0, SW7_CS7, SW8_CS7, SW9_CS7}, // "H"
    {0, SW7_CS8, SW8_CS8, SW9_CS8}, // "J"
    {0, SW7_CS9, SW8_CS9, SW9_CS9}, // "K"
    {0, SW7_CS10, SW8_CS10, SW9_CS10}, // "L"
    {0, SW7_CS11, SW8_CS11, SW9_CS11}, // ":"
    {1, SW7_CS16, SW8_CS16, SW9_CS16}, // "\""
    {1, SW7_CS14, SW8_CS14, SW9_CS14}, // "Enter"
    {0, SW10_CS1, SW11_CS1, SW12_CS1}, // "Shift"
    {0, SW10_CS3, SW11_CS3, SW12_CS3}, // "Z"
    {0, SW10_CS4, SW11_CS4, SW12_CS4}, // "X"
    {0, SW10_CS5, SW11_CS5, SW12_CS5}, // "C"
    {0, SW10_CS6, SW11_CS6, SW12_CS6}, // "V"
    {0, SW10_CS7, SW11_CS7, SW12_CS7}, // "B"
    {0, SW10_CS8, SW11_CS8, SW12_CS8}, // "N"
    {0, SW10_CS9, SW11_CS9, SW12_CS9}, // "M"
    {0, SW10_CS10, SW11_CS10, SW12_CS10}, // "<,"
    {0, SW10_CS11, SW11_CS11, SW12_CS11}, // ">."
    {1, SW10_CS1, SW11_CS1, SW12_CS1}, // "?/"
    {1, SW10_CS3, SW11_CS3, SW12_CS3}, // "Shift"
    {1, SW10_CS4, SW11_CS4, SW12_CS4}, // "↑"
    {1, SW7_CS4, SW8_CS4, SW9_CS4}, // "Del"
    {0, SW10_CS16, SW11_CS16, SW12_CS16}, // "Ctrl"
    {0, SW10_CS15, SW11_CS15, SW12_CS15}, // "Opt"
    {0, SW10_CS14, SW11_CS14, SW12_CS14}, // "Cmd"
    {0, SW10_CS13, SW11_CS13, SW12_CS13}, // "Space"
    {0, SW10_CS12, SW11_CS12, SW12_CS12}, // "Cmd"
    {1, SW10_CS16, SW11_CS16, SW12_CS16}, // "Fn"
    {1, SW10_CS13, SW11_CS13, SW12_CS13}, // "←"
    {1, SW10_CS12, SW11_CS12, SW12_CS12}, // "↓"
    {1, SW10_CS11, SW11_CS11, SW12_CS11}, // "→"

    {1, SW1_CS5, SW2_CS5, SW3_CS5}, // left side (index 64-68)
    {1, SW1_CS4, SW2_CS4, SW3_CS4},
    {1, SW1_CS3, SW2_CS3, SW3_CS3},
    {1, SW1_CS2, SW2_CS2, SW3_CS2},
    {1, SW1_CS1, SW2_CS1, SW3_CS1},

    {1, SW1_CS6, SW2_CS6, SW3_CS6}, // right side (index 69-73)
    {1, SW1_CS7, SW2_CS7, SW3_CS7},
    {1, SW1_CS8, SW2_CS8, SW3_CS8},
    {1, SW1_CS9, SW2_CS9, SW3_CS9},
    {1, SW1_CS10, SW2_CS10, SW3_CS10}
};
