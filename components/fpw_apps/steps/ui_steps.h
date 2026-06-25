// ui_steps — step counter app (swipe up).
//
// Step count + distance + average/max speed for the day, with a reset button.
#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Build the Steps UI on `parent` and start its update timer.
void ui_steps_create(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif
