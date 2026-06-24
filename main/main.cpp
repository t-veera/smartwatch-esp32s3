// Field Pocket Watch — firmware entry point.
//
// Step 1: display + touch baseline.
//   bsp_display_start() brings up the SH8601/CO5300 QSPI AMOLED panel, the
//   FT3168 touch controller, and the LVGL port task in one call. We draw a
//   white label on a true-black background and log touch coordinates to serial
//   so the display and touch paths can both be verified on hardware.

#include "bsp/esp32_s3_touch_amoled_2_06.h"
#include "esp_log.h"
#include "lvgl.h"

namespace {
constexpr char TAG[] = "fpw";

// Logs the active touch point on every press so FT3168 events show on serial.
void touch_event_cb(lv_event_t *e)
{
    lv_indev_t *indev = lv_indev_active();
    if (indev == nullptr) {
        return;
    }
    lv_point_t point;
    lv_indev_get_point(indev, &point);
    ESP_LOGI(TAG, "touch @ (%d, %d)", static_cast<int>(point.x), static_cast<int>(point.y));
}
}  // namespace

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Field Pocket Watch - Step 1 display/touch bring-up");

    lv_display_t *disp = bsp_display_start();
    if (disp == nullptr) {
        ESP_LOGE(TAG, "bsp_display_start() failed");
        return;
    }

    // LVGL is not thread-safe; hold the port mutex while building the UI.
    bsp_display_lock(0);

    lv_obj_t *scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, "Field Pocket Watch");
    lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
    lv_obj_center(label);

    lv_obj_add_event_cb(scr, touch_event_cb, LV_EVENT_PRESSED, nullptr);

    bsp_display_unlock();

    ESP_LOGI(TAG, "Display up. Tap the screen to log touch coordinates.");
}
