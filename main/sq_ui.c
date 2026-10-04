// main/sq_ui.c —— 见 sq_ui.h。控件骨架与家族同源；主题为行情终端配色。
#include "sq_ui.h"

#include <stdio.h>
#include <string.h>

#include "bsp_battery.h"
#include "sq_wrap.h"

// 「每行几个字」是算术，不是手感。三个预算各自乘以自己的字号，都必须放得进
// 内容区的 210px —— 放不进就说明内容区的宽度或者字号被改过，而另一处没跟上。
_Static_assert(SQ_CHARS_HEADLINE * 32 <= SQ_BODY_W, "32px 每行字数不再放得下");
_Static_assert(SQ_CHARS_BODY * 24 <= SQ_BODY_W, "24px 每行字数不再放得下");
_Static_assert(SQ_CHARS_SMALL * 16 <= SQ_BODY_W, "16px 每行字数不再放得下");

static lv_obj_t *plain_obj(lv_obj_t *parent, int x, int y, int w, int h)
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

static lv_obj_t *filled(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t *obj = plain_obj(parent, x, y, w, h);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    return obj;
}

lv_obj_t *sq_label_create(lv_obj_t *parent, const char *text,
                          const lv_font_t *font, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text ? text : "");
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_pad_all(label, 0, 0);
    lv_obj_set_style_border_width(label, 0, 0);
    return label;
}

// 折行缓冲。最长的界面文案是提示与状态行（单行短句），但终局说明可能加长
// —— SQ_WRAP_CAPACITY (512) 与七桥同款余量，守卫钉住算术。
#if SQ_WRAP_CAPACITY < 384
#error "sq_wrap.h 的折行缓冲装不下界面文案"
#endif

static char s_wrapped[SQ_WRAP_CAPACITY];

void sq_text_set(lv_obj_t *label, const char *text, int chars_per_line)
{
    if (!label) return;
    const size_t length =
        sq_wrap_utf8(text, chars_per_line, s_wrapped, sizeof(s_wrapped));
    if (length == 0) {
        lv_label_set_text(label, "");
        return;
    }
    lv_label_set_text(label, s_wrapped);
}

lv_obj_t *sq_page_create(lv_obj_t **out_card)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(scr, 0, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_style_bg_color(scr, lv_color_hex(SQ_C_SCREEN), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);

    lv_obj_t *card = lv_obj_create(scr);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(card, SQ_PAGE_INSET, SQ_PAGE_INSET);
    lv_obj_set_size(card, SQ_PAGE_W, SQ_PAGE_H);
    lv_obj_set_style_radius(card, SQ_PAGE_RADIUS, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(SQ_C_PANEL), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_clip_corner(card, true, 0);

    if (out_card) *out_card = card;
    return scr;
}

lv_obj_t *sq_body_create(lv_obj_t *card)
{
    lv_obj_t *body = plain_obj(card, 0, SQ_BODY_TOP, SQ_PAGE_W, SQ_BODY_H);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    return body;
}

#define SQ_BAT_W 20
#define SQ_BAT_H 11
#define SQ_BAT_EDGE 2

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

    lv_obj_t *label = sq_label_create(bar, text, &sq_font_16, SQ_C_HINTINK);
    lv_obj_align(label, LV_ALIGN_RIGHT_MID, -40, 0);

    lv_obj_t *shell = plain_obj(bar, 0, 0, SQ_BAT_W, SQ_BAT_H);
    lv_obj_set_style_bg_opa(shell, LV_OPA_TRANSP, 0);
    lv_obj_align(shell, LV_ALIGN_RIGHT_MID, -13, 0);

    filled(shell, 0, 0, SQ_BAT_W, SQ_BAT_EDGE, SQ_C_HINTINK);
    filled(shell, 0, SQ_BAT_H - SQ_BAT_EDGE, SQ_BAT_W, SQ_BAT_EDGE, SQ_C_HINTINK);
    filled(shell, 0, 0, SQ_BAT_EDGE, SQ_BAT_H, SQ_C_HINTINK);
    filled(shell, SQ_BAT_W - SQ_BAT_EDGE, 0, SQ_BAT_EDGE, SQ_BAT_H, SQ_C_HINTINK);

    lv_obj_t *cap = filled(bar, 0, 0, 3, 5, SQ_C_HINTINK);
    lv_obj_set_style_radius(cap, 1, 0);
    lv_obj_align(cap, LV_ALIGN_RIGHT_MID, -10, 0);

    if (soc > 0) {
        const int usable = SQ_BAT_W - 2 * (SQ_BAT_EDGE + 1);
        int width = (usable * pct) / 100;
        if (width < 1) width = 1;
        lv_obj_t *fill = filled(shell, SQ_BAT_EDGE + 1, SQ_BAT_EDGE + 1, width,
                                SQ_BAT_H - 2 * (SQ_BAT_EDGE + 1), SQ_C_MUTED);
        lv_obj_set_style_radius(fill, 2, 0);
    }
}

#define SQ_BAR_PAD     14
#define SQ_BAR_RIGHT_W 56
#define SQ_BAR_LEFT_W  (SQ_PAGE_W - SQ_BAR_PAD - SQ_BAR_RIGHT_W - 6)

sq_topbar_t sq_topbar_create(lv_obj_t *card, const char *left)
{
    sq_topbar_t bar = {0};
    bar.bar = filled(card, 0, 0, SQ_PAGE_W, SQ_BAR_H, SQ_C_PANEL_ALT);

    if (left && left[0]) {
        bar.left = sq_label_create(bar.bar, left, &sq_font_16, SQ_C_INK);
        lv_obj_set_width(bar.left, SQ_BAR_LEFT_W);
        lv_obj_set_height(bar.left, SQ_LINE_H_SMALL);
        lv_label_set_long_mode(bar.left, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_align(bar.left, LV_ALIGN_LEFT_MID, SQ_BAR_PAD, 0);
    }
    return bar;
}

void sq_topbar_set_left(sq_topbar_t *bar, const char *text)
{
    if (!bar || !bar->left) return;
    lv_label_set_text(bar->left, text ? text : "");
}

void sq_topbar_set_right(sq_topbar_t *bar, const char *text)
{
    if (!bar || !bar->bar) return;
    if (bar->right) {
        lv_obj_delete(bar->right);
        bar->right = NULL;
    }
    bar->right = sq_label_create(bar->bar, text ? text : "", &sq_font_16, SQ_C_HINTINK);
    lv_obj_set_width(bar->right, SQ_BAR_RIGHT_W);
    lv_obj_set_height(bar->right, SQ_LINE_H_SMALL);
    lv_label_set_long_mode(bar->right, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(bar->right, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(bar->right, LV_ALIGN_RIGHT_MID, -SQ_BAR_PAD, 0);
}

void sq_topbar_add_battery(sq_topbar_t *bar)
{
    if (!bar || !bar->bar) return;
    battery_create(bar->bar);
}

lv_obj_t *sq_hint_create(lv_obj_t *card, const char *text)
{
    lv_obj_t *bar = filled(card, 0, SQ_PAGE_H - SQ_HINT_H, SQ_PAGE_W, SQ_HINT_H,
                           SQ_C_PANEL_ALT);
    lv_obj_t *label = sq_label_create(bar, text, &sq_font_16, SQ_C_HINTINK);
    lv_obj_set_width(label, SQ_PAGE_W - 12);
    lv_obj_set_height(label, SQ_LINE_H_SMALL);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(label);
    return label;
}

void sq_hint_set(lv_obj_t *hint, const char *text)
{
    if (hint) lv_label_set_text(hint, text ? text : "");
}

lv_obj_t *sq_headline_create(lv_obj_t *parent, int x, int y, int w, int h,
                             uint32_t color)
{
    lv_obj_t *label = sq_label_create(parent, "", &sq_font_32, color);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, w);
    lv_obj_set_height(label, h);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(label, 4, 0);
    return label;
}

lv_obj_t *sq_note_create(lv_obj_t *parent, int x, int y, int w,
                         const char *text, uint32_t color)
{
    lv_obj_t *label = sq_label_create(parent, text, &sq_font_16, color);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, w);
    lv_obj_set_height(label, SQ_LINE_H_SMALL);
    lv_label_set_long_mode(label, LV_LABEL_LONG_MODE_DOTS);
    return label;
}

static void row_apply(sq_row_t *row, sq_state_t state)
{
    if (!row || !row->box) return;

    uint32_t bg = SQ_C_PANEL_ALT;
    uint32_t border = SQ_C_LINE;
    uint32_t ink = SQ_C_INK;
    uint32_t note_ink = SQ_C_MUTED;

    switch (state) {
        case SQ_STATE_SELECTED:
            bg = SQ_C_GOLD_S;
            border = SQ_C_GOLD;
            ink = SQ_C_GOLD_D;
            note_ink = SQ_C_GOLD_D;
            break;
        case SQ_STATE_GOOD:
            bg = SQ_C_PANEL_ALT;
            border = SQ_C_UP;
            ink = SQ_C_UP;
            note_ink = SQ_C_UP;
            break;
        case SQ_STATE_BAD:
            bg = SQ_C_PANEL_ALT;
            border = SQ_C_UP;
            ink = SQ_C_RED;
            note_ink = SQ_C_RED;
            break;
        case SQ_STATE_LOCKED:
            bg = SQ_C_PANEL;
            border = SQ_C_LINE;
            ink = SQ_C_MUTED;
            note_ink = SQ_C_MUTED;
            break;
        case SQ_STATE_NORMAL:
        default:
            break;
    }

    lv_obj_set_style_bg_color(row->box, lv_color_hex(bg), 0);
    lv_obj_set_style_border_color(row->box, lv_color_hex(border), 0);
    if (row->text) lv_obj_set_style_text_color(row->text, lv_color_hex(ink), 0);
    if (row->note) lv_obj_set_style_text_color(row->note, lv_color_hex(note_ink), 0);
}

static sq_row_t row_build(lv_obj_t *parent, int x, int y, int w, int h, int note_width)
{
    sq_row_t row = {0};
    row.box = plain_obj(parent, x, y, w, h);
    lv_obj_set_style_radius(row.box, 8, 0);
    lv_obj_set_style_bg_opa(row.box, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(row.box, SQ_ROW_BORDER, 0);

    const int note_w = note_width > 0 ? note_width : 0;
    const int inner = w - 2 * SQ_ROW_BORDER - 2 * SQ_ROW_PAD - note_w;

    if (note_w > 0) {
        row.note = sq_label_create(row.box, "", &sq_font_16, SQ_C_MUTED);
        lv_obj_set_width(row.note, note_w);
        lv_obj_set_height(row.note, SQ_LINE_H_SMALL);
        lv_label_set_long_mode(row.note, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_style_text_align(row.note, LV_TEXT_ALIGN_RIGHT, 0);
        lv_obj_align(row.note, LV_ALIGN_RIGHT_MID, -SQ_ROW_PAD, 0);
    }

    row.text = sq_label_create(row.box, "", &sq_font_16, SQ_C_INK);
    lv_obj_set_width(row.text, inner);
    lv_obj_set_height(row.text, SQ_LINE_H_SMALL);
    lv_label_set_long_mode(row.text, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_align(row.text, LV_ALIGN_LEFT_MID, SQ_ROW_PAD, 0);

    row_apply(&row, SQ_STATE_NORMAL);
    return row;
}

sq_row_t sq_row_small_create(lv_obj_t *parent, int x, int y, int w, int h,
                             int note_width)
{
    return row_build(parent, x, y, w, h, note_width);
}

void sq_row_update(sq_row_t *row, const char *text, const char *note, sq_state_t state)
{
    if (!row || !row->box) return;
    if (row->text) lv_label_set_text(row->text, text ? text : "");
    if (row->note) lv_label_set_text(row->note, note ? note : "");
    row_apply(row, state);
}

