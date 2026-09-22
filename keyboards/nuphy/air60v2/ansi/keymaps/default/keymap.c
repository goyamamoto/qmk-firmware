// Copyright 2026 goyamamoto
// SPDX-License-Identifier: GPL-2.0-or-later

// Stock NuPhy Air60 V2 keymap. The OS switch selects layer 0 (Mac) or
// layer 2 (Win); Fn is layer 1/3, Fn+Shift is layer 4/5, Fn+M is layer 6.
//
// Additions: Fn+Tab toggles the US-JIS substitution. The two keys beside
// Space are the OS mode's modifier when held (Mac: Cmd, Win: Alt) and switch
// the IME when tapped: left = IME off (Mac Eisu, Win Muhenkan), right = IME
// on (Mac Kana, Win Henkan). HOLD_ON_OTHER_KEY_PRESS in config.h makes a key
// pressed while they are held take the modifier at once. Caps Lock and Left
// Ctrl are swapped on both base layers (CAPS_LOCK_ROW/COL in config.h follow).

#include "ansi.h"
#include "keycodes.h"
#include QMK_KEYBOARD_H

// clang-format off
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
// layer Mac
[0] = LAYOUT_ansi_64(
	KC_ESC, 	KC_1,   	KC_2,   	KC_3,  		KC_4,   	KC_5,   	KC_6,   	KC_7,   	KC_8,   	KC_9,  		KC_0,   	KC_MINS,	KC_EQL, 	KC_BSPC,
	KC_TAB, 	KC_Q,   	KC_W,   	KC_E,  		KC_R,   	KC_T,   	KC_Y,   	KC_U,   	KC_I,   	KC_O,  		KC_P,   	KC_LBRC,	KC_RBRC, 	KC_BSLS,
	KC_LCTL,	KC_A,   	KC_S,   	KC_D,  		KC_F,   	KC_G,   	KC_H,   	KC_J,   	KC_K,   	KC_L,  		KC_SCLN,	KC_QUOT, 	            KC_ENT,
	KC_LSFT,	KC_Z,   	KC_X,   	KC_C,  		KC_V,   	KC_B,   	KC_N,   	KC_M,   	KC_COMM,	KC_DOT,		KC_SLSH,	KC_RSFT,	KC_UP,		KC_DEL,
	KC_CAPS,	KC_LALT,	LGUI_T(KC_LNG2),									KC_SPC, 							RGUI_T(KC_LNG1),	MO(1),   	KC_LEFT,	KC_DOWN,    KC_RGHT),

// layer Mac Fn
[1] = LAYOUT_ansi_64(
	KC_GRV, 	KC_BRID,  	KC_BRIU,  	MAC_TASK, 	MAC_SEARCH, MAC_VOICE,  MAC_DND,  	KC_MPRV,  	KC_MPLY,  	KC_MNXT, 	KC_MUTE, 	KC_VOLD, 	KC_VOLU, 	_______,
	USJIS_TOG,	LNK_BLE1,  	LNK_BLE2,  	LNK_BLE3,  	LNK_RF,   	_______,   	_______,   	_______,   	_______,   	_______,  	_______,   	DEV_RESET,	SLEEP_MODE, BAT_SHOW,
	_______, 	_______,   	_______,   	_______,  	_______,   	_______,   	_______,   	_______,   	_______,   	_______,  	_______,   	_______,	            _______,
	MO(4),	    _______,   	_______,   	_______,  	_______,   	_______,   	_______,   	MO(6),   	RM_SPDD,	RM_SPDU,  	_______,	MO(4),  	RM_VALU,    _______,
	_______,	_______,	_______,										_______, 							_______,	_______,   	RM_NEXT,	RM_VALD,    RM_HUEU),

// layer Win
[2] = LAYOUT_ansi_64(
	KC_ESC, 	KC_1,   	KC_2,   	KC_3,  		KC_4,   	KC_5,   	KC_6,   	KC_7,   	KC_8,   	KC_9,  		KC_0,   	KC_MINS,	KC_EQL, 	KC_BSPC,
	KC_TAB, 	KC_Q,   	KC_W,   	KC_E,  		KC_R,   	KC_T,   	KC_Y,   	KC_U,   	KC_I,   	KC_O,  		KC_P,   	KC_LBRC,	KC_RBRC, 	KC_BSLS,
	KC_LCTL,	KC_A,   	KC_S,   	KC_D,  		KC_F,   	KC_G,   	KC_H,   	KC_J,   	KC_K,   	KC_L,  		KC_SCLN,	KC_QUOT, 	            KC_ENT,
	KC_LSFT,	KC_Z,   	KC_X,   	KC_C,  		KC_V,   	KC_B,   	KC_N,   	KC_M,   	KC_COMM,	KC_DOT,		KC_SLSH,	KC_RSFT,	KC_UP,		KC_DEL,
	KC_CAPS,	KC_LGUI,	LALT_T(KC_INT5),									KC_SPC, 							RALT_T(KC_INT4),	MO(3),   	KC_LEFT,	KC_DOWN,    KC_RGHT),

// layer Win Fn
[3] = LAYOUT_ansi_64(
	KC_GRV, 	KC_F1,  	KC_F2,  	KC_F3, 		KC_F4,  	KC_F5,  	KC_F6,  	KC_F7,  	KC_F8,  	KC_F9, 		KC_F10, 	KC_F11, 	KC_F12, 	_______,
	USJIS_TOG,	LNK_BLE1,  	LNK_BLE2,  	LNK_BLE3,  	LNK_RF,   	_______,   	_______,   	_______,   	_______,   	_______,  	_______,   	DEV_RESET,	SLEEP_MODE, BAT_SHOW,
	_______, 	_______,   	_______,   	_______,  	_______,   	_______,   	_______,   	_______,   	_______,   	_______,  	_______,   	_______,	            _______,
	MO(5),	    _______,   	_______,   	_______,  	_______,   	_______,   	_______,   	MO(6),   	RM_SPDD,	RM_SPDU,  	_______,	MO(5),  	RM_VALU,    _______,
	_______,	_______,	_______,										_______, 							_______,	_______,   	RM_NEXT,	RM_VALD,    RM_HUEU),

// layer Mac Fn+Shift
[4] = LAYOUT_ansi_64(
	SHIFT_GRV, 	KC_F1,  	KC_F2,  	KC_F3, 		KC_F4,  	KC_F5,  	KC_F6,  	KC_F7,  	KC_F8,  	KC_F9, 		KC_F10, 	KC_F11, 	KC_F12, 	_______,
	_______, 	_______,  	_______,  	_______,  	_______,   	_______,   	_______,   	_______,   	_______,   	_______,  	_______,   	_______,	_______,    _______,
	_______, 	_______,   	_______,   	_______,  	_______,   	_______,   	_______,   	_______,   	_______,   	_______,  	_______,   	_______,	            _______,
	_______,	_______,   	_______,   	RGB_TEST,  	_______,   	_______,   	_______,   	_______,   	_______,   	_______,  	_______,	_______, 	_______,    _______,
	_______,	_______,	_______,										_______, 							_______,	_______,   	_______,	_______,    _______),

// layer Win Fn+Shift
[5] = LAYOUT_ansi_64(
	SHIFT_GRV, 	KC_BRID,   	KC_BRIU,    _______,  	_______,   	_______,   	_______,   	KC_MPRV,   	KC_MPLY,   	KC_MNXT,  	KC_MUTE, 	KC_VOLD, 	KC_VOLU, 	_______,
	_______, 	_______,  	_______,  	_______,  	_______,   	_______,   	_______,   	_______,   	_______,   	_______,  	_______,   	_______,	_______,    _______,
	_______, 	_______,   	_______,   	_______,  	_______,   	_______,   	_______,   	_______,   	_______,   	_______,  	_______,   	_______,	            _______,
	_______,	_______,   	_______,   	RGB_TEST,  	_______,   	_______,   	_______,   	_______,   	_______,   	_______,  	_______,	_______, 	_______,    _______,
	_______,	_______,	_______,										_______, 							_______,	_______,   	_______,	_______,    _______),

// layer side light (Fn+M)
[6] = LAYOUT_ansi_64(
	_______, 	_______,  	_______,  	_______, 	_______,  	_______,  	_______,  	_______,  	_______,  	_______, 	_______, 	_______, 	_______, 	_______,
	_______, 	_______,  	_______,  	_______, 	_______,  	_______,  	_______,  	_______,  	_______,  	_______, 	_______, 	_______, 	_______, 	_______,
	_______, 	_______,   	_______,   	_______,  	_______,   	_______,   	_______,   	_______,   	_______,   	_______,  	_______,   	_______,	            _______,
	_______,    _______,   	_______,   	_______,  	_______,   	_______,   	_______,   	_______,   	SIDE_SPD,	SIDE_SPI,  	_______,	_______,  	SIDE_VAI,    _______,
	_______,	_______,	_______,										_______, 							_______,	_______,   	SIDE_MOD,	SIDE_VAD,    SIDE_HUI)
};
// clang-format on
