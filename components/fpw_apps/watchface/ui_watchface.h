// ui_watchface — the permanent home screen.
//
// Not an esp-brookesia app: it is built directly on the active screen at boot
// and on every app close. Reads time from the RTC, battery from the PMIC and
// steps from the IMU.
#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Build the watchface on `parent` and start its 1 Hz update timer.
// Call with the LVGL lock held.
void ui_watchface_create(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif
