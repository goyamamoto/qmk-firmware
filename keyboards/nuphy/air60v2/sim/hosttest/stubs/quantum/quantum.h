// Host-test stand-in for QMK's quantum.h: just enough for keyboards/nuphy.
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "color.h"
#include "timer.h"
#include "rgb_matrix.h"
#include "host.h"
#include "keycodes.h"
#include "quantum_keycodes.h"
#include "modifiers.h"
#include "action_stub.h"

#ifndef PROGMEM
#    define PROGMEM
#endif

// Pins are opaque identifiers on hardware; on the host every GPIO call is a no-op.
#define gpio_write_pin_high(p) ((void)0)
#define gpio_write_pin_low(p) ((void)0)
#define gpio_set_pin_output(p) ((void)0)
#define gpio_set_pin_input_high(p) ((void)0)
#define gpio_read_pin(p) (0)

void wait_ms(uint32_t ms);

// layer state model
extern uint32_t layer_state;
uint8_t         get_highest_layer(uint32_t state);
