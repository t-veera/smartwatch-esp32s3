#include "ui_kartlog.h"
#include "fpw_theme.h"
#include "fpw_log.h"

namespace {

struct KartUi {
    lv_obj_t *status;
    lv_obj_t *cal;
    lv_obj_t *elapsed;
    lv_obj_t *samples;
    lv_obj_t *fname;
    lv_obj_t *sd;
};

KartUi s_ui;

void update_cb(lv_timer_t *)
{
    bool active = fpw_log_active();
    lv_label_set_text(s_ui.status, active ? "REC" : "IDLE");
    lv_obj_set_style_text_color(s_ui.status, active ? FPW_COL_RED : FPW_COL_WHITE_35, 0);

    if (fpw_log_is_calibrated()) {
        lv_label_set_text(s_ui.cal, "calibrated");
        lv_obj_set_style_text_color(s_ui.cal, FPW_COL_TEAL, 0);
    } else if (fpw_log_cal_rejected()) {
        lv_label_set_text(s_ui.cal, "NOT CALIBRATED - press BOOT");
        lv_obj_set_style_text_color(s_ui.cal, FPW_COL_RED, 0);
    } else {
        lv_label_set_text(s_ui.cal, "press BOOT to calibrate");
        lv_obj_set_style_text_color(s_ui.cal, FPW_COL_AMBER, 0);
    }

    uint32_t ms = fpw_log_elapsed_ms();
    lv_label_set_text_fmt(s_ui.elapsed, "%02u:%02u",
                          static_cast<unsigned>(ms / 60000),
                          static_cast<unsigned>((ms / 1000) % 60));

    lv_label_set_text_fmt(s_ui.samples, "%u samples",
                          static_cast<unsigned>(fpw_log_sample_count()));

    const char *fn = fpw_log_filename();
    lv_label_set_text(s_ui.fname, (fn && fn[0]) ? fn : "-");

    lv_label_set_text(s_ui.sd, fpw_log_sd_ok() ? "SD ready" : "no SD card");
    lv_obj_set_style_text_color(s_ui.sd, fpw_log_sd_ok() ? FPW_COL_WHITE_35 : FPW_COL_AMBER, 0);
}

}  // namespace

extern "C" void ui_kartlog_create(lv_obj_t *parent)
{
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "KART LOG");
    lv_obj_set_style_text_font(title, FPW_FONT_HINT, 0);
    lv_obj_set_style_text_color(title, FPW_COL_WHITE_35, 0);
    lv_obj_set_style_text_letter_space(title, 3, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 60);

    s_ui.status = lv_label_create(parent);
    lv_label_set_text(s_ui.status, "IDLE");
    lv_obj_set_style_text_font(s_ui.status, FPW_FONT_TIMER, 0);
    lv_obj_set_style_text_color(s_ui.status, FPW_COL_WHITE_35, 0);
    lv_obj_align(s_ui.status, LV_ALIGN_CENTER, 0, -78);

    s_ui.cal = lv_label_create(parent);
    lv_label_set_text(s_ui.cal, "press BOOT to calibrate");
    lv_obj_set_style_text_font(s_ui.cal, FPW_FONT_HINT, 0);
    lv_obj_set_style_text_color(s_ui.cal, FPW_COL_AMBER, 0);
    lv_obj_align(s_ui.cal, LV_ALIGN_CENTER, 0, -40);

    s_ui.elapsed = lv_label_create(parent);
    lv_label_set_text(s_ui.elapsed, "00:00");
    lv_obj_set_style_text_font(s_ui.elapsed, FPW_FONT_VALUE, 0);
    lv_obj_set_style_text_color(s_ui.elapsed, FPW_COL_WHITE, 0);
    lv_obj_align(s_ui.elapsed, LV_ALIGN_CENTER, 0, 2);

    s_ui.samples = lv_label_create(parent);
    lv_label_set_text(s_ui.samples, "0 samples");
    lv_obj_set_style_text_font(s_ui.samples, FPW_FONT_LABEL, 0);
    lv_obj_set_style_text_color(s_ui.samples, FPW_COL_WHITE_55, 0);
    lv_obj_align(s_ui.samples, LV_ALIGN_CENTER, 0, 44);

    s_ui.fname = lv_label_create(parent);
    lv_label_set_text(s_ui.fname, "-");
    lv_obj_set_style_text_font(s_ui.fname, FPW_FONT_HINT, 0);
    lv_obj_set_style_text_color(s_ui.fname, FPW_COL_WHITE_15, 0);
    lv_obj_align(s_ui.fname, LV_ALIGN_CENTER, 0, 82);

    s_ui.sd = lv_label_create(parent);
    lv_label_set_text(s_ui.sd, "SD ready");
    lv_obj_set_style_text_font(s_ui.sd, FPW_FONT_HINT, 0);
    lv_obj_set_style_text_color(s_ui.sd, FPW_COL_WHITE_35, 0);
    lv_obj_align(s_ui.sd, LV_ALIGN_CENTER, 0, 108);

    lv_obj_t *hint = lv_label_create(parent);
    lv_label_set_text(hint, "BOOT: calibrate   PWR: start / stop");
    lv_obj_set_style_text_font(hint, FPW_FONT_HINT, 0);
    lv_obj_set_style_text_color(hint, FPW_COL_WHITE_15, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -42);

    update_cb(nullptr);
    lv_timer_create(update_cb, 500, nullptr);
}
