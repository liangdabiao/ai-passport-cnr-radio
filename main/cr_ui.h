// main/cr_ui.h —— 界面层：复古收音机主题、字体与可复用骨架。
//
// 绝对坐标 + 编译期版式断言 + 固定行高防折行（家族同一套纪律）；主题是
// 本应用自己的深胡桃 + 琥珀 + 米金刻字。调谐刻度盘与音量浮层是本应用的
// 核心控件，住 cr_page_tuner.c。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#include "cr_layout.h"

// ------------------------------------------------------------- 字体 -------
// 三档字库由 tools/cnr-radio/gen_font.py 生成到 assets/fonts/（界面文案 +
// 全部台名的并集字符集）。
LV_FONT_DECLARE(cr_font_16);
LV_FONT_DECLARE(cr_font_24);
LV_FONT_DECLARE(cr_font_32);

// ------------------------------------------------------------- 骨架 -------
lv_obj_t *cr_label_create(lv_obj_t *parent, const char *text,
                          const lv_font_t *font, uint32_t color);

// 全屏底 + 圆角卡片（家族同款物理框架，复古配色）。*out_card 收卡片。
lv_obj_t *cr_page_create(lv_obj_t **out_card);

// 顶栏：左侧标题 + 右侧电池与联网点。返回句柄供刷新联网指示。
typedef struct {
    lv_obj_t *bar;
    lv_obj_t *left;
    lv_obj_t *wifi_dot;
} cr_topbar_t;

cr_topbar_t cr_topbar_create(lv_obj_t *card, const char *left);
void cr_topbar_add_battery(cr_topbar_t *bar);
// 联网点：联网 = 绿点；未联网 = 灰点。传 NULL 隐藏。位置在电池左侧。
void cr_topbar_set_wifi(cr_topbar_t *bar, bool connected, bool show);

// 底部提示栏。
lv_obj_t *cr_hint_create(lv_obj_t *card, const char *text);
void cr_hint_set(lv_obj_t *hint, const char *text);
