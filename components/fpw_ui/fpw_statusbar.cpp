#include "fpw_statusbar.h"
#include "fpw_theme.h"
#include "fpw_pmic.h"

namespace {
constexpr int BAR_H   = 44;
constexpr int BAR_TOP = 10;   // push below the panel's rounded top corners
constexpr int INSET   = 30;   // keep icons clear of the rounded corners

lv_obj_t *make_icon(lv_obj_t *parent, const char *symbol, lv_color_t color)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, symbol);
    lv_obj_set_style_text_font(l, FPW_FONT_LABEL, 0);
    lv_obj_set_style_text_color(l, color, 0);
    return l;
}
}  // namespace

extern "C" void fpw_statusbar_create(lv_obj_t *parent, fpw_statusbar_t *out)
{
    lv_obj_t *bar = lv_obj_create(parent);
    lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(bar, FPW_SCREEN_W, BAR_H);
    lv_obj_align(bar, LV_ALIGN_TOP_MID, 0, BAR_TOP);
    lv_obj_set_style_bg_opa(bar, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);

    lv_obj_t *bt = make_icon(bar, LV_SYMBOL_BLUETOOTH, FPW_COL_TEAL);
    lv_obj_align(bt, LV_ALIGN_LEFT_MID, INSET, 0);

    lv_obj_t *batt = lv_label_create(bar);
    lv_label_set_text(batt, "--%");
    lv_obj_set_style_text_font(batt, FPW_FONT_LABEL, 0);
    lv_obj_set_style_text_color(batt, FPW_COL_WHITE_55, 0);
    lv_obj_align(batt, LV_ALIGN_RIGHT_MID, -INSET, 0);

    lv_obj_t *wifi = make_icon(bar, LV_SYMBOL_WIFI, FPW_COL_WHITE_15);
    lv_obj_align_to(wifi, batt, LV_ALIGN_OUT_LEFT_MID, -12, 0);

    if (out) {
        out->root = bar;
        out->bt = bt;
        out->wifi = wifi;
        out->batt = batt;
    }
}

extern "C" void fpw_statusbar_set_ble(fpw_statusbar_t *sb, bool connected)
{
    if (sb && sb->bt) {
        lv_obj_set_style_text_color(sb->bt, connected ? FPW_COL_TEAL : FPW_COL_WHITE_15, 0);
    }
}

extern "C" void fpw_statusbar_set_wifi(fpw_statusbar_t *sb, bool connected)
{
    if (sb && sb->wifi) {
        lv_obj_set_style_text_color(sb->wifi, connected ? FPW_COL_TEAL : FPW_COL_WHITE_15, 0);
    }
}

extern "C" void fpw_statusbar_refresh(fpw_statusbar_t *sb)
{
    if (!sb || !sb->batt) {
        return;
    }
    int pct = fpw_pmic_battery_percent();
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;

    if (fpw_pmic_is_charging()) {
        lv_label_set_text_fmt(sb->batt, LV_SYMBOL_CHARGE " %d%%", pct);
    } else {
        lv_label_set_text_fmt(sb->batt, "%d%%", pct);
    }
    // Battery text width changes with the charge bolt; keep WiFi just left of it.
    if (sb->wifi) {
        lv_obj_align_to(sb->wifi, sb->batt, LV_ALIGN_OUT_LEFT_MID, -12, 0);
    }
}
