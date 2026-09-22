// Copyright 2026 goyamamoto
// SPDX-License-Identifier: GPL-2.0-or-later
//
// US-JIS substitution: type a US keyboard as printed on a host that is set
// to the Japanese keyboard layout. See usjis.c for the rules.
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "action.h"

// Call first from process_record_kb. Returns false when the event was consumed.
bool usjis_process_record(uint16_t keycode, keyrecord_t *record);
// Call from post_process_record_kb: re-applies the Shift policy after QMK
// handled a pass-through event (modifier keys, unsubstituted keys).
void usjis_post_process_record(uint16_t keycode, keyrecord_t *record);
// Forget every held key, for example after the host report was cleared.
void usjis_clear(void);
// Request a mode change; applied once no non-modifier key is held.
void usjis_request(bool enable);
void usjis_toggle(void);
// Effective mode (what keyboard_config stores).
bool usjis_is_enabled(void);
// True while substitution is in force: enabled and the OS switch is on Win.
bool usjis_is_active(void);
