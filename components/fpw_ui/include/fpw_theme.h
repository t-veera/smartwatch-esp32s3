// fpw_theme.h — Field Pocket Watch design system.
//
// Single source of truth for colour and type tokens. Include this everywhere;
// never hardcode colours or font pointers in UI code.
#pragma once

#include "lvgl.h"

// --- Colour tokens ---------------------------------------------------------
#define FPW_COL_BG        lv_color_hex(0x000000)   // true black — AMOLED off pixels
#define FPW_COL_SURFACE   lv_color_hex(0x0F0F0F)   // card / panel backgrounds
#define FPW_COL_TEAL      lv_color_hex(0x2DD4BF)   // steps, BLE, active state
#define FPW_COL_AMBER     lv_color_hex(0xF59E0B)   // battery, music progress
#define FPW_COL_RED       lv_color_hex(0xEF4444)   // recording (used once only)
#define FPW_COL_WHITE     lv_color_hex(0xFFFFFF)   // primary text
#define FPW_COL_WHITE_55  lv_color_make(140, 140, 140)  // secondary text ~55%
#define FPW_COL_WHITE_35  lv_color_make(89, 89, 89)     // tertiary text ~35%
#define FPW_COL_WHITE_15  lv_color_make(38, 38, 38)     // hint text ~15%
#define FPW_COL_WHITE_06  lv_color_make(15, 15, 15)     // track / bar background

// --- Type tokens -----------------------------------------------------------
// Built-in LVGL Montserrat faces (enabled in sdkconfig.defaults). On this small
// high-DPI panel the time is rendered from FPW_FONT_TIME scaled up via an LVGL
// transform (see FPW_TIME_SCALE), since 48px is the largest built-in face.
#define FPW_FONT_TIME    (&lv_font_montserrat_48)  // watchface time (transform-scaled)
#define FPW_FONT_TIMER   (&lv_font_montserrat_36)  // recorder timer
#define FPW_FONT_VALUE   (&lv_font_montserrat_28)  // big readouts (date, data values)
#define FPW_FONT_LABEL   (&lv_font_montserrat_20)  // track names, section labels
#define FPW_FONT_HINT    (&lv_font_montserrat_14)  // units, nav hints, status bar

// Crisp clock face: Montserrat-Medium @112px, digits 0-9 and colon only,
// generated with lv_font_conv. The watchface uses this so the time renders
// sharp and the screen transition stays smooth (no per-frame transform scale).
#ifdef __cplusplus
extern "C" {
#endif
extern const lv_font_t font_time_112;
#ifdef __cplusplus
}
#endif
#define FPW_FONT_CLOCK  (&font_time_112)

// --- Canvas ----------------------------------------------------------------
#define FPW_SCREEN_W  410
#define FPW_SCREEN_H  502
