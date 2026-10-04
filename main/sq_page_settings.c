// main/sq_page_settings.c —— 设置页：刷新间隔循环（15/30/60 秒）。
//
// 仅盘中自动刷新受此间隔影响；手动刷新（列表页长按确定）不受限制。
#include "sq_app.h"

#include <stdio.h>
#include <string.h>

#include "sq_sfx.h"
#include "sq_store.h"
#include "sq_ui.h"

static lv_obj_t *s_value;
static lv_obj_t *s_hint;

static void settings_refresh(void)
{
    if (s_value) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%u 秒", (unsigned)sq_app_refresh_seconds());
        lv_label_set_text(s_value, buf);
    }
    if (s_hint) {
        sq_hint_set(s_hint, "确定切换 · 返回回标题");
    }
}

lv_obj_t *sq_page_settings_enter(void)
{
    s_value = s_hint = NULL;

    lv_obj_t *card = NULL;
    lv_obj_t *screen = sq_page_create(&card);
    if (!card) return screen;

    sq_topbar_t bar = sq_topbar_create(card, "刷新设置");
    sq_topbar_add_battery(&bar);
    s_hint = sq_hint_create(card, "");
    lv_obj_t *body = sq_body_create(card);

    sq_note_create(body, SQ_BODY_X, 40, SQ_BODY_W, "盘中自动刷新间隔", SQ_C_MUTED);

    s_value = sq_label_create(body, "", &sq_font_32, SQ_C_GOLD);
    lv_obj_set_pos(s_value, SQ_BODY_X, 80);
    lv_obj_set_width(s_value, SQ_BODY_W);
    lv_obj_set_height(s_value, 44);
    lv_obj_set_style_text_align(s_value, LV_TEXT_ALIGN_CENTER, 0);

    settings_refresh();
    return screen;
}

void sq_page_settings_leave(void)
{
    s_value = s_hint = NULL;
}

void sq_page_settings_key(sq_key_t key)
{
    switch (key) {
        case SQ_KEY_OK:
            sq_app_cycle_refresh();
            sq_sfx_play(SQ_TONE_ENTER);
            settings_refresh();
            return;
        case SQ_KEY_BACK:
            sq_sfx_play(SQ_TONE_BACK);
            sq_app_goto_title();
            return;
        default:
            return;
    }
}
