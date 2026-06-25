#include "ui_trackpad.h"
#include "fpw_theme.h"

namespace {

lv_obj_t *s_status;

constexpr int GRID = 48;

void grid_line(lv_obj_t *parent, int x, int y, int w, int h, lv_opa_t opa)
{
    lv_obj_t *l = lv_obj_create(parent);
    lv_obj_remove_flag(l, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(l, w, h);
    lv_obj_set_pos(l, x, y);
    lv_obj_set_style_border_width(l, 0, 0);
    lv_obj_set_style_radius(l, 0, 0);
    lv_obj_set_style_pad_all(l, 0, 0);
    lv_obj_set_style_bg_color(l, FPW_COL_TEAL, 0);
    lv_obj_set_style_bg_opa(l, opa, 0);
}

}  // namespace

extern "C" void ui_trackpad_create(lv_obj_t *parent)
{
    // Futuristic grid covering the whole screen — the entire surface is the pad.
    for (int x = GRID; x < FPW_SCREEN_W; x += GRID) {
        grid_line(parent, x, 0, 1, FPW_SCREEN_H, LV_OPA_20);
    }
    for (int y = GRID; y < FPW_SCREEN_H; y += GRID) {
        grid_line(parent, 0, y, FPW_SCREEN_W, 1, LV_OPA_20);
    }
    // Brighter centre crosshair.
    grid_line(parent, FPW_SCREEN_W / 2, 0, 1, FPW_SCREEN_H, LV_OPA_50);
    grid_line(parent, 0, FPW_SCREEN_H / 2, FPW_SCREEN_W, 1, LV_OPA_50);

    // Minimal connection status, tucked at the bottom.
    s_status = lv_label_create(parent);
    lv_label_set_text(s_status, "not connected");
    lv_obj_set_style_text_font(s_status, FPW_FONT_HINT, 0);
    lv_obj_set_style_text_color(s_status, FPW_COL_WHITE_35, 0);
    lv_obj_align(s_status, LV_ALIGN_BOTTOM_MID, 0, -14);
}

extern "C" void ui_trackpad_set_connected(bool connected)
{
    if (!s_status) {
        return;
    }
    lv_label_set_text(s_status, connected ? "connected" : "not connected");
    lv_obj_set_style_text_color(s_status, connected ? FPW_COL_TEAL : FPW_COL_WHITE_35, 0);
}
