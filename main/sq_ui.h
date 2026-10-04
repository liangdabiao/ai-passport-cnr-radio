// main/sq_ui.h —— 界面层：主题、字体与可复用控件。
//
// 控件骨架与家族各应用同一套（绝对坐标 + 编译期版式断言 + 固定行高防折行），
// 主题换成行情终端自己的墨黑 + 金色 + 红涨绿跌；行情列表行（格点、棋子、光标、可达落点
// 高亮）是本应用新增的核心控件 —— 只改样式与坐标，从不重建控件树。
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lvgl.h"

#include "sq_layout.h"

// ------------------------------------------------------------- 字体 -------
// 三档字库由 tools/chinese-checkers/gen_font.py 生成到 assets/fonts/。
LV_FONT_DECLARE(sq_font_16);
LV_FONT_DECLARE(sq_font_24);
LV_FONT_DECLARE(sq_font_32);

// ------------------------------------------------------------- 控件 -------
lv_obj_t *sq_label_create(lv_obj_t *parent, const char *text,
                          const lv_font_t *font, uint32_t color);
// 把 text 按每行 chars_per_line 个字折行后写进 label（中文没有空格，必须自己折）。
void sq_text_set(lv_obj_t *label, const char *text, int chars_per_line);

lv_obj_t *sq_page_create(lv_obj_t **out_card);
lv_obj_t *sq_body_create(lv_obj_t *card);

lv_obj_t *sq_headline_create(lv_obj_t *parent, int x, int y, int w, int h,
                             uint32_t color);
lv_obj_t *sq_note_create(lv_obj_t *parent, int x, int y, int w,
                         const char *text, uint32_t color);

typedef struct {
    lv_obj_t *bar;
    lv_obj_t *left;
    lv_obj_t *right;
} sq_topbar_t;

sq_topbar_t sq_topbar_create(lv_obj_t *card, const char *left);
void sq_topbar_set_left(sq_topbar_t *bar, const char *text);
void sq_topbar_set_right(sq_topbar_t *bar, const char *text);
void sq_topbar_add_battery(sq_topbar_t *bar);

lv_obj_t *sq_hint_create(lv_obj_t *card, const char *text);
void sq_hint_set(lv_obj_t *hint, const char *text);

typedef enum {
    SQ_STATE_NORMAL = 0,
    SQ_STATE_SELECTED,
    SQ_STATE_GOOD,
    SQ_STATE_BAD,
    SQ_STATE_LOCKED,
} sq_state_t;

typedef struct {
    lv_obj_t *box;
    lv_obj_t *text;
    lv_obj_t *note;
} sq_row_t;

#define SQ_ROW_BORDER 3
#define SQ_ROW_PAD    8
#define SQ_ROW_H      28
#define SQ_ROW_GAP    3

sq_row_t sq_row_small_create(lv_obj_t *parent, int x, int y, int w, int h,
                             int note_width);
void sq_row_update(sq_row_t *row, const char *text, const char *note,
                   sq_state_t state);

// ----------------------------------------------------------- 行情行 -------
// 行情列表行由页面自建（两行文本 + 涨跌配色），sq_ui 只提供家族控件。
