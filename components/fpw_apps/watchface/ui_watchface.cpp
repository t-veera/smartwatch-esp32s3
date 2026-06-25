#include "ui_watchface.h"
#include "fpw_theme.h"
#include "fpw_statusbar.h"
#include "fpw_imu.h"
#include "fpw_pmic.h"
#include "fpw_rtc.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace {

constexpr int STEP_GOAL = 10000;

struct Watchface {
    fpw_statusbar_t sb;
    lv_obj_t *time;
    lv_obj_t *date_line;
    lv_obj_t *steps_bar;
    lv_obj_t *steps_val;
    lv_obj_t *batt_bar;
    lv_obj_t *batt_val;
    lv_obj_t *nav_hint;
};

Watchface s_wf;

// "6210" -> "6,210"
void group_thousands(uint32_t n, char *out, size_t out_sz)
{
    char raw[12];
    snprintf(raw, sizeof(raw), "%u", static_cast<unsigned>(n));
    int len = static_cast<int>(strlen(raw));
    int j = 0;
    for (int i = 0; i < len && j < static_cast<int>(out_sz) - 1; i++) {
        if (i > 0 && (len - i) % 3 == 0) {
            out[j++] = ',';
        }
        out[j++] = raw[i];
    }
    out[j] = '\0';
}

void on_screen_press(lv_event_t *)
{
    lv_obj_set_style_opa(s_wf.nav_hint, LV_OPA_COVER, 0);
    lv_obj_fade_out(s_wf.nav_hint, 400, 5000);
}

void make_data_group(lv_obj_t *parent, const char *label_text, lv_color_t fill,
                     lv_obj_t **bar_out, lv_obj_t **val_out)
{
    lv_obj_t *col = lv_obj_create(parent);
    lv_obj_remove_flag(col, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(col, 150, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(col, 0, 0);
    lv_obj_set_style_pad_all(col, 0, 0);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_row(col, 8, 0);

    lv_obj_t *label = lv_label_create(col);
    lv_label_set_text(label, label_text);
    lv_obj_set_style_text_font(label, FPW_FONT_HINT, 0);
    lv_obj_set_style_text_color(label, FPW_COL_WHITE_35, 0);
    lv_obj_set_style_text_letter_space(label, 2, 0);

    lv_obj_t *bar = lv_bar_create(col);
    lv_obj_set_size(bar, 150, 3);
    lv_obj_set_style_radius(bar, 2, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, FPW_COL_WHITE_06, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(bar, 2, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(bar, fill, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);

    lv_obj_t *val = lv_label_create(col);
    lv_label_set_text(val, "--");
    lv_obj_set_style_text_font(val, FPW_FONT_VALUE, 0);
    lv_obj_set_style_text_color(val, FPW_COL_WHITE, 0);

    if (bar_out) *bar_out = bar;
    if (val_out) *val_out = val;
}

void update_cb(lv_timer_t *)
{
    struct tm t;
    if (fpw_rtc_get_time(&t) == ESP_OK) {
        char hm[8];
        strftime(hm, sizeof(hm), "%H:%M", &t);   // 24-hour
        lv_label_set_text(s_wf.time, hm);

        char wd[12], md[10], dline[28];
        strftime(wd, sizeof(wd), "%A", &t);
        strftime(md, sizeof(md), "%b %d", &t);
        snprintf(dline, sizeof(dline), "%s  " LV_SYMBOL_BULLET "  %s", wd, md);
        lv_label_set_text(s_wf.date_line, dline);
    }

    fpw_statusbar_refresh(&s_wf.sb);

    uint32_t steps = fpw_imu_step_count();
    int steps_pct = static_cast<int>((steps * 100) / STEP_GOAL);
    if (steps_pct > 100) steps_pct = 100;
    lv_bar_set_value(s_wf.steps_bar, steps_pct, LV_ANIM_OFF);
    char sbuf[12];
    group_thousands(steps, sbuf, sizeof(sbuf));
    lv_label_set_text(s_wf.steps_val, sbuf);

    int batt = fpw_pmic_battery_percent();
    if (batt < 0) batt = 0;
    if (batt > 100) batt = 100;
    lv_bar_set_value(s_wf.batt_bar, batt, LV_ANIM_OFF);
    lv_label_set_text_fmt(s_wf.batt_val, "%d%%", batt);
}

}  // namespace

extern "C" void ui_watchface_create(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, FPW_COL_BG, 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_remove_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    fpw_statusbar_create(parent, &s_wf.sb);
    fpw_statusbar_set_ble(&s_wf.sb, false);   // no BLE yet -> dim until connected
    fpw_statusbar_set_wifi(&s_wf.sb, false);

    // --- Time: one big label, scaled up via transform (48px is the largest
    //     built-in Montserrat face). Pivot centred so it grows symmetrically. ---
    s_wf.time = lv_label_create(parent);
    lv_label_set_text(s_wf.time, "00:00");
    lv_obj_set_style_text_font(s_wf.time, FPW_FONT_CLOCK, 0);  // native large font, no transform
    lv_obj_set_style_text_color(s_wf.time, FPW_COL_WHITE, 0);
    lv_obj_align(s_wf.time, LV_ALIGN_CENTER, 0, -56);

    // --- Date line ---
    s_wf.date_line = lv_label_create(parent);
    lv_label_set_text(s_wf.date_line, "");
    lv_obj_set_style_text_font(s_wf.date_line, FPW_FONT_LABEL, 0);
    lv_obj_set_style_text_color(s_wf.date_line, FPW_COL_WHITE_35, 0);
    lv_obj_align(s_wf.date_line, LV_ALIGN_CENTER, 0, 24);

    // --- Data bars: STEPS (teal) and BATTERY (amber) ---
    lv_obj_t *data = lv_obj_create(parent);
    lv_obj_remove_flag(data, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(data, FPW_SCREEN_W - 50, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(data, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(data, 0, 0);
    lv_obj_set_style_pad_all(data, 0, 0);
    lv_obj_set_flex_flow(data, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(data, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_align(data, LV_ALIGN_CENTER, 0, 120);

    make_data_group(data, "STEPS", FPW_COL_TEAL, &s_wf.steps_bar, &s_wf.steps_val);
    make_data_group(data, "BATTERY", FPW_COL_AMBER, &s_wf.batt_bar, &s_wf.batt_val);

    // --- Nav hint ---
    s_wf.nav_hint = lv_label_create(parent);
    lv_label_set_text(s_wf.nav_hint,
                      LV_SYMBOL_UP " steps     " LV_SYMBOL_DOWN " record     " LV_SYMBOL_LEFT " music");
    lv_obj_set_style_text_font(s_wf.nav_hint, FPW_FONT_HINT, 0);
    lv_obj_set_style_text_color(s_wf.nav_hint, FPW_COL_WHITE_15, 0);
    lv_obj_align(s_wf.nav_hint, LV_ALIGN_BOTTOM_MID, 0, -16);

    lv_obj_add_event_cb(parent, on_screen_press, LV_EVENT_PRESSED, nullptr);
    lv_obj_set_style_opa(s_wf.nav_hint, LV_OPA_COVER, 0);
    lv_obj_fade_out(s_wf.nav_hint, 400, 5000);

    update_cb(nullptr);
    lv_timer_create(update_cb, 1000, nullptr);
}
