#include "fpw_nav.h"
#include "ui_watchface.h"
#include "ui_steps.h"
#include "ui_trackpad.h"
#include "fpw_theme.h"
#include "fpw_statusbar.h"

#include "bsp/esp32_s3_touch_amoled_2_06.h"
#include "lvgl.h"

namespace {

constexpr uint32_t ANIM_MS = 200;

enum AppId { APP_STEPS, APP_CONNECT, APP_METRO, APP_RECORD, APP_COUNT };

struct AppScreen {
    lv_obj_t *scr;
    fpw_statusbar_t sb;
    lv_dir_t back_dir;                 // swipe direction that returns home
    lv_screen_load_anim_t back_anim;   // transition used when returning home
};

lv_obj_t *s_watchface;
lv_obj_t *s_trackpad;            // button-only full-screen HID trackpad
AppScreen s_apps[APP_COUNT];

void load_screen(lv_obj_t *scr, lv_screen_load_anim_t anim)
{
    if (lv_screen_active() != scr) {
        lv_screen_load_anim(scr, anim, ANIM_MS, 0, false);
    }
}

// Gesture on the watchface -> open the app in that direction.
void watchface_gesture_cb(lv_event_t *)
{
    switch (lv_indev_get_gesture_dir(lv_indev_active())) {
        case LV_DIR_TOP:    load_screen(s_apps[APP_STEPS].scr,   LV_SCR_LOAD_ANIM_OVER_TOP);    break;
        case LV_DIR_BOTTOM: load_screen(s_apps[APP_CONNECT].scr, LV_SCR_LOAD_ANIM_OVER_BOTTOM); break;
        case LV_DIR_LEFT:   load_screen(s_apps[APP_METRO].scr,   LV_SCR_LOAD_ANIM_OVER_LEFT);   break;
        case LV_DIR_RIGHT:  load_screen(s_apps[APP_RECORD].scr,  LV_SCR_LOAD_ANIM_OVER_RIGHT);  break;
        default: break;
    }
}

// Gesture on an app -> swipe back returns to the watchface.
void app_gesture_cb(lv_event_t *e)
{
    AppScreen *a = static_cast<AppScreen *>(lv_event_get_user_data(e));
    if (lv_indev_get_gesture_dir(lv_indev_active()) == a->back_dir) {
        load_screen(s_watchface, a->back_anim);
    }
}

// Long-press anywhere on an app -> back to the watchface.
void app_longpress_cb(lv_event_t *e)
{
    AppScreen *a = static_cast<AppScreen *>(lv_event_get_user_data(e));
    load_screen(s_watchface, a->back_anim);
}

void refresh_cb(lv_timer_t *)
{
    for (int i = 0; i < APP_COUNT; i++) {
        fpw_statusbar_refresh(&s_apps[i].sb);
    }
}

// Create a bare app screen: background, status bar and nav gestures. Content
// (placeholder or real UI) is added by the caller.
lv_obj_t *setup_app_screen(AppId id, lv_dir_t back_dir, lv_screen_load_anim_t back_anim)
{
    lv_obj_t *scr = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr, FPW_COL_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(scr, LV_OBJ_FLAG_CLICKABLE);

    AppScreen &a = s_apps[id];
    a.scr = scr;
    a.back_dir = back_dir;
    a.back_anim = back_anim;

    fpw_statusbar_create(scr, &a.sb);
    fpw_statusbar_set_ble(&a.sb, false);   // no BLE yet -> dim until connected
    fpw_statusbar_set_wifi(&a.sb, false);

    lv_obj_add_event_cb(scr, app_gesture_cb, LV_EVENT_GESTURE, &a);
    lv_obj_add_event_cb(scr, app_longpress_cb, LV_EVENT_LONG_PRESSED, &a);
    return scr;
}

void add_back_hint(lv_obj_t *scr)
{
    lv_obj_t *hint = lv_label_create(scr);
    lv_label_set_text(hint, "swipe back or long-press for watch");
    lv_obj_set_style_text_font(hint, FPW_FONT_HINT, 0);
    lv_obj_set_style_text_color(hint, FPW_COL_WHITE_15, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -18);
}

void add_placeholder_body(lv_obj_t *scr, const char *title)
{
    lv_obj_t *name = lv_label_create(scr);
    lv_label_set_text(name, title);
    lv_obj_set_style_text_font(name, FPW_FONT_VALUE, 0);
    lv_obj_set_style_text_color(name, FPW_COL_WHITE, 0);
    lv_obj_align(name, LV_ALIGN_CENTER, 0, -16);

    lv_obj_t *soon = lv_label_create(scr);
    lv_label_set_text(soon, "coming soon");
    lv_obj_set_style_text_font(soon, FPW_FONT_LABEL, 0);
    lv_obj_set_style_text_color(soon, FPW_COL_WHITE_35, 0);
    lv_obj_align(soon, LV_ALIGN_CENTER, 0, 24);
}

}  // namespace

extern "C" void fpw_nav_init(void)
{
    // Watchface lives on its own screen so we can animate to/from it.
    s_watchface = lv_obj_create(nullptr);
    lv_obj_add_flag(s_watchface, LV_OBJ_FLAG_CLICKABLE);
    ui_watchface_create(s_watchface);
    lv_obj_add_event_cb(s_watchface, watchface_gesture_cb, LV_EVENT_GESTURE, nullptr);

    // Steps is a real app; the others are placeholders for now.
    // Back gesture/anim is the reverse of the entry direction.
    lv_obj_t *steps_scr = setup_app_screen(APP_STEPS, LV_DIR_BOTTOM, LV_SCR_LOAD_ANIM_OVER_BOTTOM);
    ui_steps_create(steps_scr);
    add_back_hint(steps_scr);

    add_placeholder_body(setup_app_screen(APP_CONNECT, LV_DIR_TOP, LV_SCR_LOAD_ANIM_OVER_TOP), "Connectivity");
    add_back_hint(s_apps[APP_CONNECT].scr);

    add_placeholder_body(setup_app_screen(APP_METRO, LV_DIR_RIGHT, LV_SCR_LOAD_ANIM_OVER_RIGHT), "Metronome");
    add_back_hint(s_apps[APP_METRO].scr);

    add_placeholder_body(setup_app_screen(APP_RECORD, LV_DIR_LEFT, LV_SCR_LOAD_ANIM_OVER_LEFT), "Recorder");
    add_back_hint(s_apps[APP_RECORD].scr);

    // Trackpad: opened only by the side button (toggle), not a swipe app and
    // with no status bar — the whole screen is the touch surface.
    s_trackpad = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(s_trackpad, FPW_COL_BG, 0);
    lv_obj_set_style_bg_opa(s_trackpad, LV_OPA_COVER, 0);
    lv_obj_remove_flag(s_trackpad, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_trackpad, LV_OBJ_FLAG_CLICKABLE);
    ui_trackpad_create(s_trackpad);

    lv_screen_load(s_watchface);
    lv_timer_create(refresh_cb, 5000, nullptr);
}

// Toggle the Trackpad from outside the LVGL task (the side button): open it,
// or return to the watchface if it is already showing. Takes the LVGL lock and
// triggers activity so the power manager wakes the screen.
extern "C" void fpw_nav_toggle_trackpad(void)
{
    bsp_display_lock(0);
    if (lv_screen_active() == s_trackpad) {
        load_screen(s_watchface, LV_SCR_LOAD_ANIM_OVER_TOP);
    } else {
        load_screen(s_trackpad, LV_SCR_LOAD_ANIM_OVER_BOTTOM);
    }
    lv_disp_trig_activity(nullptr);
    bsp_display_unlock();
}
