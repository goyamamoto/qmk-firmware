// Copyright 2026 goyamamoto
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// data table lengths used by common/lighting/side_table.h
#define WAVE_TAB_LEN 112
#define BREATHE_TAB_LEN 128
#define FLOW_COLOR_TAB_LEN 224


// draw both strips for the current RGB frame; call before the common indicators
void side_led_frame(void);
// Fn-layer status lights on Tab/Q/W/E/R/Y; call after the common indicators
void fn_status_show(void);
