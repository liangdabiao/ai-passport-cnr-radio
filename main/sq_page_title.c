// main/sq_page_title.c —— 标题页：行情入口与设置。
//
// 菜单四项：行情列表 / 添加股票 / Wi-Fi 配网 / 刷新设置。顶行常显网络状态
// （已联网 / 未联网），配网页在菜单第二层。
#include "sq_app.h"

#include <stdio.h>
#include <string.h>

#include "sq_sfx.h"
#include "sq_ui.h"

#define TITLE_ITEMS 4

static const char *const TITLE_LABEL[TITLE_ITEMS] = {
    "行情列表",
    "添加股票",
    "Wi-Fi 配网",
    "刷新设置",
};

static sq_row_t s_rows[TITLE_ITEMS];
static lv_obj_t *s_hint;
static lv_obj_t *s_status;
static int s_sel;

static void title_refresh(void)
{
    for (int index = 0; index < TITLE_ITEMS; index++) {
        sq_row_update(&s_rows[index], TITLE_LABEL[index], "",
                      index == s_sel ? SQ_STATE_SELECTED : SQ_STATE_NORMAL);
    }
    if (s_status) {
        char status[64];
        if (sq_app_wifi_connected()) {
            snprintf(status, sizeof(status), "已联网 · 自选%u只",
                     (unsigned)sq_app_watch()->count);
        } else {
            snprintf(status, sizeof(status), "未联网 · 请先在 Wi-Fi 配网连接");
        }
        lv_label_set_text(s_status, status);
    }
    if (s_hint) {
        sq_hint_set(s_hint, "上下选择 · 确定进入");
    }
}

lv_obj_t *sq_page_title_enter(void)
{
    s_sel = 0;
    s_hint = NULL;
    s_status = NULL;
    for (int index = 0; index < TITLE_ITEMS; index++) s_rows[index] = (sq_row_t){0};

    lv_obj_t *card = NULL;
    lv_obj_t *screen = sq_page_create(&card);
    if (!card) return screen;

    sq_topbar_t bar = sq_topbar_create(card, "A股大行情");
    sq_topbar_add_battery(&bar);
    s_hint = sq_hint_create(card, "");
    lv_obj_t *body = sq_body_create(card);

    s_status = sq_note_create(body, SQ_BODY_X, 8, SQ_BODY_W, "", SQ_C_MUTED);

    for (int index = 0; index < TITLE_ITEMS; index++) {
        const int y = 36 + index * (SQ_ROW_H + SQ_ROW_GAP);
        s_rows[index] = sq_row_small_create(body, SQ_BODY_X, y, SQ_BODY_W, SQ_ROW_H, 0);
    }

#define TITLE_MENU_END (36 + (TITLE_ITEMS - 1) * (SQ_ROW_H + SQ_ROW_GAP) + SQ_ROW_H)
    _Static_assert(TITLE_MENU_END <= SQ_BODY_H, "菜单超出正文区");
#undef TITLE_MENU_END

    title_refresh();
    return screen;
}

void sq_page_title_leave(void)
{
    s_hint = NULL;
    s_status = NULL;
    for (int index = 0; index < TITLE_ITEMS; index++) s_rows[index] = (sq_row_t){0};
}

void sq_page_title_key(sq_key_t key)
{
    switch (key) {
        case SQ_KEY_UP:
            if (s_sel == 0) return;
            s_sel--;
            sq_sfx_play(SQ_TONE_MOVE);
            title_refresh();
            return;
        case SQ_KEY_DOWN:
            if (s_sel + 1 >= TITLE_ITEMS) return;
            s_sel++;
            sq_sfx_play(SQ_TONE_MOVE);
            title_refresh();
            return;
        case SQ_KEY_OK:
            sq_sfx_play(SQ_TONE_ENTER);
            if (s_sel == 0) sq_app_goto_list();
            else if (s_sel == 1) sq_app_goto_add();
            else if (s_sel == 2) sq_app_goto_wifi();
            else sq_app_goto_settings();
            return;
        default:
            return;
    }
}
