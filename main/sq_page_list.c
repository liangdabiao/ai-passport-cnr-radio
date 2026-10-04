// main/sq_page_list.c —— 行情列表页：自选逐行显示 + 盘中自动刷新。
//
// 每行两行文本：上行「名称」，下行「价格  涨跌%」（红涨绿跌，平盘灰）。
// 键位：上/下循环选行；确定进详情；长按确定手动刷新；长按下删除自选（两次
// 确认）；返回回标题。
// 刷新：页面 1 秒心跳里判断（已联网 && 行情时段 && 距上次超过间隔）→ 请求
// sq_net；非行情时段只允许手动。
#include "sq_app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <time.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"

#include "sq_market.h"
#include "sq_net.h"
#include "sq_sfx.h"
#include "sq_store.h"
#include "sq_symbol.h"
#include "sq_watch.h"
#include "sq_ui.h"

static const char *TAG = "sq_list";

typedef struct {
    lv_obj_t *box;
    lv_obj_t *name;   // 上行：名称（左）
    lv_obj_t *code;   // 上行：代码（右）
    lv_obj_t *price;  // 下行：价格（左）
    lv_obj_t *pct;    // 下行：涨跌%（右）
} quote_row_t;

static quote_row_t s_rows[SQ_LIST_ROWS];
static lv_obj_t *s_hint;
static int s_sel;         // 选中自选下标
static int s_scroll;      // 窗口首行
static bool s_remove_armed;
static lv_timer_t *s_tick;
static int64_t s_last_fetch_us;

static void list_refresh(void)
{
    const sq_watchlist_t *watch = sq_app_watch();
    const sq_quote_batch_t *snap = sq_app_snapshot();

    // 选中夹取 + 窗口跟随。
    if (watch->count == 0) s_sel = 0;
    else if (s_sel >= watch->count) s_sel = watch->count - 1;
    if (s_sel < s_scroll) s_scroll = s_sel;
    if (s_sel >= s_scroll + SQ_LIST_ROWS) s_scroll = s_sel - SQ_LIST_ROWS + 1;
    if (s_scroll < 0) s_scroll = 0;

    char buf[32];
    for (uint8_t row = 0; row < SQ_LIST_ROWS; row++) {
        const int index = s_scroll + row;
        quote_row_t *r = &s_rows[row];
        if (!r->box) continue;
        if (index >= watch->count) {
            lv_obj_add_flag(r->box, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_remove_flag(r->box, LV_OBJ_FLAG_HIDDEN);

        const sq_quote_item_t *item = NULL;
        for (uint8_t k = 0; k < snap->count; k++) {
            if (strcmp(snap->items[k].code, watch->entries[index].code) == 0) {
                item = &snap->items[k];
            }
        }

        const bool selected = index == s_sel;
        lv_obj_set_style_border_color(r->box,
                                      lv_color_hex(selected ? SQ_C_GOLD : SQ_C_LINE), 0);
        lv_obj_set_style_bg_color(r->box,
                                  lv_color_hex(selected ? SQ_C_GOLD_S : SQ_C_PANEL_ALT),
                                  0);

        // 上行：预设简称（ASCII），非预设显示 代码.市场 后缀。
        const char *preset = sq_watch_preset_name(watch->entries[index].code,
                                                  watch->entries[index].is_index);
        char name_text[20];
        if (preset) {
            snprintf(name_text, sizeof(name_text), "%s", preset);
        } else {
            const char sh = watch->entries[index].code[0];
            snprintf(name_text, sizeof(name_text), "%s.%s", watch->entries[index].code,
                     (sh == '6' || sh == '9' || sh == '5') ? "SH" : "SZ");
        }
        lv_label_set_text(r->name, name_text);
        snprintf(buf, sizeof(buf), "%u/%u", (unsigned)(index + 1),
                 (unsigned)watch->count);
        lv_label_set_text(r->code, buf);

        // 下行：价格 + 涨跌%，红涨绿跌。
        if (!item || !item->valid) {
            lv_label_set_text(r->price, "--");
            lv_label_set_text(r->pct, item ? "停牌/无数据" : "待刷新");
            lv_obj_set_style_text_color(r->pct, lv_color_hex(SQ_C_FLAT), 0);
        } else {
            snprintf(buf, sizeof(buf), "%d.%02d", (int)(item->price_x100 / 100),
                     abs((int)item->price_x100) % 100);
            lv_label_set_text(r->price, buf);
            const int pct = (int)item->pct_x100;
            snprintf(buf, sizeof(buf), "%s%d.%02d%%", (int)pct > 0 ? "+" : "", pct / 100,
                     abs(pct) % 100);
            lv_label_set_text(r->pct, buf);
            const uint32_t color = pct > 0 ? SQ_C_UP : ((int)pct < 0 ? SQ_C_DOWN : SQ_C_FLAT);
            lv_obj_set_style_text_color(r->pct, lv_color_hex(color), 0);
            lv_obj_set_style_text_color(r->price, lv_color_hex(color), 0);
        }
    }

    if (s_hint) {
        if (!sq_app_wifi_connected()) {
            sq_hint_set(s_hint, sq_app_snapshot_valid() ? "未联网 · 显示缓存"
                                                        : "未联网 · 请先配网");
        } else if (s_remove_armed) {
            sq_hint_set(s_hint, "再长按下键删除此自选");
        } else if (sq_app_snapshot_valid()) {
            sq_hint_set(s_hint, "确定详情 · 长按确定刷新");
        } else {
            sq_hint_set(s_hint, "正在拉取行情…");
        }
    }
}

// 1 秒心跳：自动刷新调度 + 网络状态提示。
static void tick_cb(lv_timer_t *timer)
{
    (void)timer;
    static int64_t last_request = 0;
    const int64_t now = esp_timer_get_time();

    if (sq_app_wifi_connected()) {
        const time_t now_s = time(NULL);
        const bool trading = now_s > 1700000000 && sq_market_is_trading((uint64_t)now_s);
        const int64_t elapsed_s = (now - s_last_fetch_us) / 1000000;
        if (trading && elapsed_s >= sq_app_refresh_seconds() &&
            now - last_request > 3000000) {
            last_request = now;
            s_last_fetch_us = now;
            sq_app_request_refresh();
        }
    }
    list_refresh();
}

lv_obj_t *sq_page_list_enter(void)
{
    s_sel = sq_app_selected();
    s_scroll = 0;
    s_remove_armed = false;
    s_last_fetch_us = 0;
    s_hint = NULL;
    memset(s_rows, 0, sizeof(s_rows));

    lv_obj_t *card = NULL;
    lv_obj_t *screen = sq_page_create(&card);
    if (!card) return screen;

    sq_topbar_t bar = sq_topbar_create(card, "A股大行情");
    sq_topbar_add_battery(&bar);
    s_hint = sq_hint_create(card, "");
    lv_obj_t *body = sq_body_create(card);

    for (uint8_t row = 0; row < SQ_LIST_ROWS; row++) {
        const int y = row * (SQ_ROW_H + SQ_ROW_GAP);
        quote_row_t *r = &s_rows[row];
        r->box = lv_obj_create(body);
        lv_obj_remove_flag(r->box, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_pos(r->box, SQ_BODY_X, y);
        lv_obj_set_size(r->box, SQ_BODY_W, SQ_ROW_H);
        lv_obj_set_style_radius(r->box, 8, 0);
        lv_obj_set_style_border_width(r->box, 2, 0);
        lv_obj_set_style_pad_all(r->box, 0, 0);
        lv_obj_set_style_bg_opa(r->box, LV_OPA_COVER, 0);

        r->name = sq_label_create(r->box, "", &sq_font_16, SQ_C_INK);
        lv_obj_set_pos(r->name, 8, 1);
        lv_obj_set_width(r->name, 120);
        lv_obj_set_height(r->name, SQ_LINE_H_SMALL);
        lv_label_set_long_mode(r->name, LV_LABEL_LONG_MODE_DOTS);

        r->code = sq_label_create(r->box, "", &sq_font_16, SQ_C_MUTED);
        lv_obj_set_pos(r->code, 128, 1);
        lv_obj_set_width(r->code, 74);
        lv_obj_set_height(r->code, SQ_LINE_H_SMALL);
        lv_label_set_long_mode(r->code, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_style_text_align(r->code, LV_TEXT_ALIGN_RIGHT, 0);

        r->price = sq_label_create(r->box, "", &sq_font_16, SQ_C_INK);
        lv_obj_set_pos(r->price, 8, SQ_ROW_H - SQ_LINE_H_SMALL - 2);
        lv_obj_set_width(r->price, 100);
        lv_obj_set_height(r->price, SQ_LINE_H_SMALL);
        lv_label_set_long_mode(r->price, LV_LABEL_LONG_MODE_DOTS);

        r->pct = sq_label_create(r->box, "", &sq_font_16, SQ_C_FLAT);
        lv_obj_set_pos(r->pct, 108, SQ_ROW_H - SQ_LINE_H_SMALL - 2);
        lv_obj_set_width(r->pct, 94);
        lv_obj_set_height(r->pct, SQ_LINE_H_SMALL);
        lv_label_set_long_mode(r->pct, LV_LABEL_LONG_MODE_DOTS);
        lv_obj_set_style_text_align(r->pct, LV_TEXT_ALIGN_RIGHT, 0);
    }

    _Static_assert(SQ_LIST_ROWS * SQ_ROW_H + (SQ_LIST_ROWS - 1) * SQ_ROW_GAP <=
                   SQ_BODY_H, "行情列表超出正文区");

    // 网络结果到来时刷新（回调已拿 LVGL 锁）。
    s_tick = lv_timer_create(tick_cb, 1000, NULL);
    list_refresh();
    return screen;
}

void sq_page_list_leave(void)
{
    if (s_tick) {
        lv_timer_del(s_tick);
        s_tick = NULL;
    }
    s_hint = NULL;
    memset(s_rows, 0, sizeof(s_rows));
}

void sq_page_list_on_result(bool ok)
{
    if (ok) sq_sfx_play(SQ_TONE_ENTER);
    list_refresh();
}

void sq_page_list_key(sq_key_t key)
{
    const sq_watchlist_t *watch = sq_app_watch();

    switch (key) {
        case SQ_KEY_UP:
            if (watch->count == 0) return;
            s_sel = s_sel == 0 ? watch->count - 1 : s_sel - 1;
            sq_app_set_selected(s_sel);
            sq_sfx_play(SQ_TONE_MOVE);
            list_refresh();
            return;
        case SQ_KEY_DOWN:
            if (watch->count == 0) return;
            s_sel = s_sel + 1 >= watch->count ? 0 : s_sel + 1;
            sq_app_set_selected(s_sel);
            sq_sfx_play(SQ_TONE_MOVE);
            list_refresh();
            return;
        case SQ_KEY_OK:
            if (watch->count == 0) {
                sq_sfx_play(SQ_TONE_DECLINED);
                return;
            }
            sq_app_set_selected(s_sel);
            sq_sfx_play(SQ_TONE_ENTER);
            sq_app_goto_detail();
            return;
        case SQ_KEY_REFRESH:
            if (!sq_app_wifi_connected()) {
                sq_sfx_play(SQ_TONE_DECLINED);
                return;
            }
            s_last_fetch_us = esp_timer_get_time();
            sq_app_request_refresh();
            sq_sfx_play(SQ_TONE_ENTER);
            return;
        case SQ_KEY_REMOVE: {
            if (watch->count == 0) return;
            if (!s_remove_armed) {
                s_remove_armed = true;
                list_refresh();
                return;
            }
            sq_watchlist_t *mutable_watch = sq_app_watch();
            (void)sq_watch_remove(mutable_watch,
                                  mutable_watch->entries[s_sel].code);
            (void)sq_store_save_watch(mutable_watch);
            sq_app_request_refresh();
            s_remove_armed = false;
            sq_sfx_play(SQ_TONE_BACK);
            list_refresh();
            return;
        }
        case SQ_KEY_BACK:
            sq_sfx_play(SQ_TONE_BACK);
            sq_app_goto_title();
            return;
        default:
            return;
    }
}
