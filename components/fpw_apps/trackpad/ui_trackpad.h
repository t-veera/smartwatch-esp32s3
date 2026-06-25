// ui_trackpad — laptop trackpad over BLE HID (swipe down / side button).
//
// Drag with one finger to scroll, pinch to zoom on the connected laptop.
#pragma once

#include "lvgl.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void ui_trackpad_create(lv_obj_t *parent);

// Update the connection status line (set from the BLE layer later).
void ui_trackpad_set_connected(bool connected);

#ifdef __cplusplus
}
#endif
