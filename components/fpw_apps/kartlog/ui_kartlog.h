// ui_kartlog — Kart Telemetry Logger app screen.
//
// Shows session status (IDLE / REC), elapsed time, sample count and the file
// name. Logging itself is driven by the PWR button and runs independently of
// this screen (and of the display sleeping).
#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_kartlog_create(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif
