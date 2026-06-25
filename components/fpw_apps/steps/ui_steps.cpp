#include "ui_steps.h"
#include "fpw_theme.h"
#include "fpw_imu.h"

#include <cstdio>
#include <cstring>

namespace {

struct StepsUi {
    lv_obj_t *count;
    lv_obj_t *dist;
    lv_obj_t *avg;
    lv_obj_t *max;
};

StepsUi s_ui;

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

// Format a value to one decimal place WITHOUT %f (unsupported by the nano /
// LVGL printf — it would print a literal 'f').
void fmt_1dp(char *buf, size_t sz, float v)
{
    if (v < 0.0f) {
        v = 0.0f;
    }
    long scaled = static_cast<long>(v * 10.0f + 0.5f);
    snprintf(buf, sz, "%ld.%ld", scaled / 10, scaled % 10);
}

void update_cb(lv_timer_t *)
{
    char buf[16];

    group_thousands(fpw_imu_step_count(), buf, sizeof(buf));
    lv_label_set_text(s_ui.count, buf);

    fmt_1dp(buf, sizeof(buf), fpw_imu_distance_m() / 1000.0f);
    lv_label_set_text(s_ui.dist, buf);

    fmt_1dp(buf, sizeof(buf), fpw_imu_avg_speed_mps() * 3.6f);
    lv_label_set_text(s_ui.avg, buf);

    fmt_1dp(buf, sizeof(buf), fpw_imu_max_speed_mps() * 3.6f);
    lv_label_set_text(s_ui.max, buf);
}

void reset_cb(lv_event_t *)
{
    fpw_imu_reset_stats();
    update_cb(nullptr);
}

// A stat cell: caption / value / unit. Returns the value label.
lv_obj_t *make_cell(lv_obj_t *parent, const char *caption, const lv_font_t *val_font,
                    lv_color_t val_color, const char *unit)
{
    lv_obj_t *col = lv_obj_create(parent);
    lv_obj_remove_flag(col, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(col, 170, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(col, 0, 0);
    lv_obj_set_style_pad_all(col, 0, 0);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(col, 2, 0);

    lv_obj_t *cap = lv_label_create(col);
    lv_label_set_text(cap, caption);
    lv_obj_set_style_text_font(cap, FPW_FONT_LABEL, 0);
    lv_obj_set_style_text_color(cap, FPW_COL_WHITE_15, 0);
    lv_obj_set_style_text_letter_space(cap, 2, 0);

    lv_obj_t *val = lv_label_create(col);
    lv_label_set_text(val, "0");
    lv_obj_set_style_text_font(val, val_font, 0);
    lv_obj_set_style_text_color(val, val_color, 0);

    lv_obj_t *u = lv_label_create(col);
    lv_label_set_text(u, unit);
    lv_obj_set_style_text_font(u, FPW_FONT_HINT, 0);
    lv_obj_set_style_text_color(u, FPW_COL_WHITE_35, 0);

    return val;
}

lv_obj_t *make_row(lv_obj_t *parent, int y)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(row, FPW_SCREEN_W - 20, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_align(row, LV_ALIGN_CENTER, 0, y);
    return row;
}

}  // namespace

extern "C" void ui_steps_create(lv_obj_t *parent)
{
    // Row 1: steps + distance.
    lv_obj_t *row1 = make_row(parent, -64);
    s_ui.count = make_cell(row1, "STEPS", FPW_FONT_TIME, FPW_COL_TEAL, "today");
    s_ui.dist  = make_cell(row1, "DISTANCE", FPW_FONT_TIMER, FPW_COL_WHITE, "km");

    // Row 2: average + max speed.
    lv_obj_t *row2 = make_row(parent, 40);
    s_ui.avg = make_cell(row2, "AVG SPEED", FPW_FONT_TIMER, FPW_COL_WHITE, "km/h");
    s_ui.max = make_cell(row2, "MAX SPEED", FPW_FONT_TIMER, FPW_COL_WHITE, "km/h");

    // Reset button.
    lv_obj_t *btn = lv_button_create(parent);
    lv_obj_set_size(btn, 160, 46);
    lv_obj_set_style_bg_color(btn, FPW_COL_SURFACE, 0);
    lv_obj_set_style_radius(btn, 23, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, 130);
    lv_obj_add_event_cb(btn, reset_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *btn_lbl = lv_label_create(btn);
    lv_label_set_text(btn_lbl, "RESET");
    lv_obj_set_style_text_font(btn_lbl, FPW_FONT_LABEL, 0);
    lv_obj_set_style_text_color(btn_lbl, FPW_COL_TEAL, 0);
    lv_obj_center(btn_lbl);

    update_cb(nullptr);
    lv_timer_create(update_cb, 1000, nullptr);
}
