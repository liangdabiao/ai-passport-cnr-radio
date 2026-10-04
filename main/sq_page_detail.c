// main/sq_page_detail.c —— 详情页：单只标的的完整行情字段。
//
// 上：名称 + 代码；中：现价大字 + 涨跌；下：今开 / 昨收 / 最高 / 最低 /
// 行情时间。返回回列表。
#include "sq_app.h"

#include <stdio.h>
#include <string.h>

#include <time.h>

#include "sq_sfx.h"
#include "sq_watch.h"
#include "sq_ui.h"

static lv_obj_t *s_name;
static lv_obj_t *s_price;
static lv_obj_t *s_pct;
static lv_obj_t *s_fields;
static lv_obj_t *s_hint;

static void detail_refresh(void)
{
    const sq_watchlist_t *watch = sq_app_watch();
    const uint8_t sel = sq_app_selected();
    if (sel >= watch->count) return;
    const char *code = watch->entries[sel].code;

    const sq_quote_batch_t *snap = sq_app_snapshot();
    const sq_quote_item_t *item = NULL;
    for (uint8_t k = 0; k < snap->count; k++) {
        if (strcmp(snap->items[k].code, code) == 0) item = &snap->items[k];
    }

    char buf[128];
    if (s_name) {
        const char *preset = sq_watch_preset_name(code, watch->entries[sel].is_index);
        snprintf(buf, sizeof(buf), "%s %s.%s",
                 preset ? preset : "", code,
                 watch->entries[sel].code[0] == '6' ? "SH" : "SZ");
        lv_label_set_text(s_name, buf);
    }
    if (!item || !item->valid) {
        if (s_price) lv_label_set_text(s_price, "--.--");
        if (s_pct) lv_label_set_text(s_pct, "停牌 / 无数据");
        if (s_fields) lv_label_set_text(s_fields, "暂无行情，联网后自动更新");
        return;
    }

    if (s_price) {
        snprintf(buf, sizeof(buf), "%d.%02d", (int)(item->price_x100 / 100),
                 abs((int)item->price_x100) % 100);
        lv_label_set_text(s_price, buf);
    }
    if (s_pct) {
        snprintf(buf, sizeof(buf), "%s%d.%02d%% (%s%d.%02d)",
                 item->pct_x100 > 0 ? "+" : "", (int)(item->pct_x100 / 100),
                 abs((int)item->pct_x100) % 100,
                 item->chg_x100 > 0 ? "+" : "", (int)(item->chg_x100 / 100),
                 abs((int)item->chg_x100) % 100);
        lv_label_set_text(s_pct, buf);
        const uint32_t color = item->pct_x100 > 0 ? SQ_C_UP
                               : item->pct_x100 < 0 ? SQ_C_DOWN : SQ_C_FLAT;
        lv_obj_set_style_text_color(s_price, lv_color_hex(color), 0);
        lv_obj_set_style_text_color(s_pct, lv_color_hex(color), 0);
    }
    if (s_fields) {
        char tbuf[24] = "--";
        if (item == &sq_app_snapshot()->items[0] || sq_app_snapshot()->epoch > 0) {
            const time_t et = (time_t)sq_app_snapshot()->epoch;
            struct tm tm_et;
            gmtime_r(&et, &tm_et); // epoch 已是北京时间的秒（f124 本地时间戳需换算）
            // f124 是东财本地时间戳（秒），直接按偏移展示。
            snprintf(tbuf, sizeof(tbuf), "%02d-%02d %02d:%02d", tm_et.tm_mon + 1,
                     tm_et.tm_mday, (tm_et.tm_hour + 8) % 24, tm_et.tm_min);
        }
        snprintf(buf, sizeof(buf),
                 "昨收 %d.%02d\n今开 --\n最高 %d.%02d\n最低 %d.%02d\n时间 %s",
                 (int)(item->prev_x100 / 100), abs((int)item->prev_x100) % 100,
                 (int)(item->high_x100 / 100), abs((int)item->high_x100) % 100,
                 (int)(item->low_x100 / 100), abs((int)item->low_x100) % 100, tbuf);
        sq_text_set(s_fields, buf, 13); // 单字段行，按 13 字折行即可
    }
}

lv_obj_t *sq_page_detail_enter(void)
{
    s_name = s_price = s_pct = s_fields = s_hint = NULL;

    lv_obj_t *card = NULL;
    lv_obj_t *screen = sq_page_create(&card);
    if (!card) return screen;

    sq_topbar_t bar = sq_topbar_create(card, "行情详情");
    sq_topbar_add_battery(&bar);
    s_hint = sq_hint_create(card, "返回 回列表");
    lv_obj_t *body = sq_body_create(card);

    s_name = sq_label_create(body, "", &sq_font_16, SQ_C_INK);
    lv_obj_set_pos(s_name, SQ_BODY_X, 6);
    lv_obj_set_width(s_name, SQ_BODY_W);
    lv_obj_set_height(s_name, SQ_LINE_H_SMALL);
    lv_label_set_long_mode(s_name, LV_LABEL_LONG_MODE_DOTS);

    s_price = sq_label_create(body, "", &sq_font_32, SQ_C_INK);
    lv_obj_set_pos(s_price, SQ_BODY_X, 34);
    lv_obj_set_width(s_price, 120);
    lv_obj_set_height(s_price, 40);

    s_pct = sq_label_create(body, "", &sq_font_16, SQ_C_FLAT);
    lv_obj_set_pos(s_pct, SQ_BODY_X, 78);
    lv_obj_set_width(s_pct, SQ_BODY_W);
    lv_obj_set_height(s_pct, SQ_LINE_H_SMALL);

    s_fields = sq_label_create(body, "", &sq_font_16, SQ_C_MUTED);
    lv_obj_set_pos(s_fields, SQ_BODY_X, 104);
    lv_obj_set_width(s_fields, SQ_BODY_W);
    lv_obj_set_height(s_fields, 120);
    lv_label_set_long_mode(s_fields, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_line_space(s_fields, 4, 0);

    detail_refresh();
    return screen;
}

void sq_page_detail_leave(void)
{
    s_name = s_price = s_pct = s_fields = s_hint = NULL;
}

void sq_page_detail_key(sq_key_t key)
{
    if (key == SQ_KEY_BACK || key == SQ_KEY_OK) {
        sq_sfx_play(SQ_TONE_BACK);
        sq_app_goto_list();
    }
}
