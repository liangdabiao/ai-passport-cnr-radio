// main/sq_page_add.c —— 添加自选：6 位数字轮盘。
//
// 光标停在某一位数字上，上/下加/减该位；确定进入下一位；最后一位按确定
// 弹出确认行（再按确定入自选）。长按确定取消；长按下键回退一位。
// 市场归属自动推断（6/9/5 沪、0/2/3 深，4/8 拒绝 —— 见 sq_symbol）。
// 长按下键 = 回退一位；长按确定 = 取消返回标题。
#include "sq_app.h"

#include <stdio.h>
#include <string.h>

#include "sq_sfx.h"
#include "sq_symbol.h"
#include "sq_store.h"
#include "sq_ui.h"

static char s_digits[SQ_CODE_LEN + 1];
static int s_pos;          // 当前编辑位 0..5
static bool s_confirming;  // 已输满 6 位，进入确认
static lv_obj_t *s_wheel_label;
static lv_obj_t *s_market_label;
static lv_obj_t *s_hint;

static void add_refresh(void)
{
    char wheel[SQ_CODE_LEN * 3 + 1];
    for (int i = 0; i < SQ_CODE_LEN; i++) {
        const char *fmt = i == s_pos && !s_confirming ? "[%c]" : " %c ";
        snprintf(wheel + i * 3, 4, fmt, s_digits[i]);
    }
    if (s_wheel_label) lv_label_set_text(s_wheel_label, wheel);

    if (s_market_label) {
        if (s_confirming) {
            char line[48];
            snprintf(line, sizeof(line), "确认添加：%s %s", s_digits,
                     sq_symbol_market_name(s_digits));
            lv_label_set_text(s_market_label, line);
        } else {
            lv_label_set_text(s_market_label,
                              sq_symbol_valid(s_digits) ? sq_symbol_market_name(s_digits)
                                                        : "代码无效（4/8 开头暂不支持）");
        }
    }
    if (s_hint) {
        sq_hint_set(s_hint, s_confirming ? "确定加入自选 · 长按取消"
                                         : "上下换数字 · 确定下一位");
    }
}

lv_obj_t *sq_page_add_enter(void)
{
    strcpy(s_digits, "600000");
    s_pos = 0;
    s_confirming = false;
    s_wheel_label = s_market_label = s_hint = NULL;

    lv_obj_t *card = NULL;
    lv_obj_t *screen = sq_page_create(&card);
    if (!card) return screen;

    sq_topbar_t bar = sq_topbar_create(card, "添加自选");
    sq_topbar_add_battery(&bar);
    s_hint = sq_hint_create(card, "");
    lv_obj_t *body = sq_body_create(card);

    s_wheel_label = sq_label_create(body, "", &sq_font_32, SQ_C_GOLD);
    lv_obj_set_pos(s_wheel_label, SQ_BODY_X, 60);
    lv_obj_set_width(s_wheel_label, SQ_BODY_W);
    lv_obj_set_height(s_wheel_label, 44);
    lv_obj_set_style_text_align(s_wheel_label, LV_TEXT_ALIGN_CENTER, 0);

    s_market_label = sq_label_create(body, "", &sq_font_16, SQ_C_MUTED);
    lv_obj_set_pos(s_market_label, SQ_BODY_X, 130);
    lv_obj_set_width(s_market_label, SQ_BODY_W);
    lv_obj_set_height(s_market_label, SQ_LINE_H_SMALL);
    lv_obj_set_style_text_align(s_market_label, LV_TEXT_ALIGN_CENTER, 0);

    add_refresh();
    return screen;
}

void sq_page_add_leave(void)
{
    s_wheel_label = s_market_label = s_hint = NULL;
}

void sq_page_add_key(sq_key_t key)
{
    switch (key) {
        case SQ_KEY_UP:
            if (s_confirming) return;
            s_digits[s_pos] = s_digits[s_pos] == '0' ? '9' : s_digits[s_pos] - 1;
            sq_sfx_play(SQ_TONE_MOVE);
            add_refresh();
            return;
        case SQ_KEY_DOWN:
            if (s_confirming) return;
            s_digits[s_pos] = s_digits[s_pos] == '9' ? '0' : s_digits[s_pos] + 1;
            sq_sfx_play(SQ_TONE_MOVE);
            add_refresh();
            return;
        case SQ_KEY_OK:
            if (!s_confirming) {
                if (s_pos + 1 < SQ_CODE_LEN) {
                    s_pos++;
                } else {
                    s_confirming = true;
                }
                sq_sfx_play(SQ_TONE_ENTER);
                add_refresh();
                return;
            }
            // 确认加入。
            {
                sq_watchlist_t *watch = sq_app_watch();
                if (!sq_symbol_valid(s_digits)) {
                    sq_sfx_play(SQ_TONE_DECLINED);
                    return;
                }
                if (!sq_watch_add(watch, s_digits, false)) {
                    sq_sfx_play(SQ_TONE_DECLINED);
                    if (s_hint) sq_hint_set(s_hint, "已在自选或名单已满（8）");
                    return;
                }
                (void)sq_store_save_watch(watch);
                sq_app_request_refresh();
                sq_sfx_play(SQ_TONE_WIN);
                if (s_hint) sq_hint_set(s_hint, "已加入自选");
                sq_app_goto_list();
                return;
            }
        case SQ_KEY_STEPBACK:
            // 长按下键 = 回退一位（已到首位则退出确认态）。
            if (s_confirming) {
                s_confirming = false;
            } else if (s_pos > 0) {
                s_pos--;
            }
            sq_sfx_play(SQ_TONE_BACK);
            add_refresh();
            return;
        case SQ_KEY_BACK:
            sq_sfx_play(SQ_TONE_BACK);
            sq_app_goto_title();
            return;
        default:
            return;
    }
}
