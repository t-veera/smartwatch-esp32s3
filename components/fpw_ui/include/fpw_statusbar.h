// fpw_statusbar — the persistent top status bar used on every screen.
//
// Left: Bluetooth icon. Right: WiFi icon + battery % (with a charge bolt while
// charging). Call fpw_statusbar_refresh() on a timer to update the battery.
#pragma once

#include "lvgl.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    lv_obj_t *root;
    lv_obj_t *bt;
    lv_obj_t *wifi;
    lv_obj_t *batt;
} fpw_statusbar_t;

void fpw_statusbar_create(lv_obj_t *parent, fpw_statusbar_t *out);
void fpw_statusbar_set_ble(fpw_statusbar_t *sb, bool connected);
void fpw_statusbar_set_wifi(fpw_statusbar_t *sb, bool connected);
void fpw_statusbar_refresh(fpw_statusbar_t *sb);

#ifdef __cplusplus
}
#endif
