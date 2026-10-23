// main/cr_ui.c —— 见 cr_ui.h。
#include "cr_ui.h"

#include <stdio.h>
#include <string.h>

#include "bsp_battery.h"

_Static_assert(CR_CHARS_BODY * 16 <= CR_BODY_W, "16px 每行字数不再放得下");
_Static_assert(CR_CHARS_HINT * 16 <= CR_PAGE_W - 12, "提示栏每行字数不再放得下");

static lv_obj_t *plain_obj(lv_obj_t *parent, const int x, const int y,
                           const int w, const int h)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    return obj;
}

static lv_obj_t *filled(lv_obj_t *parent, const int x, const int y, const int w,
                        const int h, const uint32_t color)
{
    lv_obj_t *obj = plain_obj(parent, x, y, w, h);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    return obj;
}

lv_obj_t *cr_label_create(lv_obj_t *parent, const char *text,
                          const lv_font_t *font, const uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text ? text : "");
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_pad_all(label, 0, 0);
    lv_obj_set_style_border_width(label, 0, 0);
    return label;
}

lv_obj_t *cr_page_create(lv_obj_t **out_card)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(scr, 0, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_style_bg_color(scr, lv_color_hex(CR_C_SCREEN), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    lv_obj_t *card = lv_obj_create(scr);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(card, CR_PAGE_INSET, CR_PAGE_INSET);
    lv_obj_set_size(card, CR_PAGE_W, CR_PAGE_H);
    lv_obj_set_style_radius(card, CR_PAGE_RADIUS, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(CR_C_PANEL), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_clip_corner(card, true, 0);

    if (out_card) *out_card = card;
    return scr;
}

// -------------------------------------------------------------- 顶栏 ------
#define CR_BAT_W 20
#define CR_BAT_H 11
#define CR_BAT_EDGE 2

static void battery_create(lv_obj_t *bar)
{
    const int soc = bsp_battery_soc();
    const int pct = soc > 100 ? 100 : soc;

    char text[16];
    if (soc >= 0) {
        snprintf(text, sizeof(text), "%d%%", pct);
    } else {
        snprintf(text, sizeof(text), "--");
    }

    lv_obj_t *label = cr_label_create(bar, text, &cr_font_16, CR_C_MUTED);
    lv_obj_align(label, LV_ALIGN_RIGHT_MID, -40, 0);

    lv_obj_t *shell = plain_obj(bar, 0, 0, CR_BAT_W, CR_BAT_H);
    lv_obj_set_style_bg_opa(shell, LV_OPA_TRANSP, 0);
    lv_obj_align(shell, LV_ALIGN_RIGHT_MID, -13, 0);

    filled(shell, 0, 0, CR_BAT_W, CR_BAT_EDGE, CR_C_MUTED);
    filled(shell, 0, CR_BAT_H - CR_BAT_EDGE, CR_BAT_W, CR_BAT_EDGE, CR_C_MUTED);
    filled(shell, 0, 0, CR_BAT_EDGE, CR_BAT_H, CR_C_MUTED);
    filled(shell, CR_BAT_W - CR_BAT_EDGE, 0, CR_BAT_EDGE, CR_BAT_H, CR_C_MUTED);

    lv_obj_t *cap = filled(bar, 0, 0, 3, 5, CR_C_MUTED);
    lv_obj_set_style_radius(cap, 1, 0);
    lv_obj_align(cap, LV_ALIGN_RIGHT_MID, -10, 0);

    if (soc > 0) {
        const int usable = CR_BAT_W - 2 * (CR_BAT_EDGE + 1);
        int width = (usable * pct) / 100;
        if (width < 1) width = 1;
        lv_obj_t *fill = filled(shell, CR_BAT_EDGE + 1, CR_BAT_EDGE + 1, width,
                                CR_BAT_H - 2 * (CR_BAT_EDGE + 1), CR_C_MUTED);
        lv_obj_set_style_radius(fill, 2, 0);
    }
}

cr_topbar_t cr_topbar_create(lv_obj_t *card, const char *left)
{
    cr_topbar_t bar = {0};
    bar.bar = filled(card, 0, 0, CR_PAGE_W, CR_BAR_H, CR_C_PANEL_ALT);

    if (left && left[0]) {
        bar.left = cr_label_create(bar.bar, left, &cr_font_16, CR_C_AMBER);
        lv_obj_set_width(bar.left, CR_PAGE_W - 100);
        lv_obj_set_height(bar.left, CR_LINE_H_SMALL);
        lv_label_set_long_mode(bar.left, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_align(bar.left, LV_ALIGN_LEFT_MID, 14, 0);
    }
    return bar;
}

void cr_topbar_add_battery(cr_topbar_t *bar)
{
    if (!bar || !bar->bar) return;
    battery_create(bar->bar);
}
void cr_topbar_set_wifi(cr_topbar_t *bar, const bool connected, const bool show)
{
    if (!bar || !bar->bar) return;
    if (!show) {
        if (bar->wifi_dot) {
            lv_obj_delete(bar->wifi_dot);
            bar->wifi_dot = NULL;
        }
        return;
    }
    if (!bar->wifi_dot) {
        bar->wifi_dot = plain_obj(bar->bar, 0, 0, 8, 8);
        lv_obj_set_style_radius(bar->wifi_dot, 4, 0);
        lv_obj_align(bar->wifi_dot, LV_ALIGN_RIGHT_MID, -66, 0);
    }
    lv_obj_set_style_bg_color(bar->wifi_dot,
                              lv_color_hex(connected ? CR_C_OK : CR_C_MUTED), 0);
    lv_obj_set_style_bg_opa(bar->wifi_dot, LV_OPA_COVER, 0);
}

// ------------------------------------------------------------ 提示栏 ------
lv_obj_t *cr_hint_create(lv_obj_t *card, const char *text)
{
    lv_obj_t *bar = filled(card, 0, CR_PAGE_H - CR_HINT_H, CR_PAGE_W, CR_HINT_H,
                           CR_C_PANEL_ALT);
    lv_obj_t *label = cr_label_create(bar, text, &cr_font_16, CR_C_MUTED);
    lv_obj_set_width(label, CR_PAGE_W - 12);
    lv_obj_set_height(label, CR_LINE_H_SMALL);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(label);
    return label;
}

void cr_hint_set(lv_obj_t *hint, const char *text)
{
    if (hint) lv_label_set_text(hint, text ? text : "");
}
