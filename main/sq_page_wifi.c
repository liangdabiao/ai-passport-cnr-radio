// main/sq_page_wifi.c —— Wi-Fi 配网页。
//
// 进入即确认设备处于配网热点模式（组件已管理），页面显示手机操作步骤与
// 当前状态（未配网：等待配置；已连上：显示已联网）。长按确定清除已存
// 凭据并重开热点（两次确认）。
#include "sq_app.h"

#include <stdio.h>
#include <string.h>

#include <time.h>

#include "esp_log.h"
#include "esp_sntp.h"
#include "lvgl.h"

#include "sq_sfx.h"
#include "sq_ui.h"
#include "sq_wifi.h"

static const char *TAG = "sq_wifi";

static lv_obj_t *s_status;
static lv_obj_t *s_hint;
static bool s_reset_armed;
static lv_timer_t *s_tick;
static bool s_dns_done;

static void wifi_refresh(void)
{
    if (s_status) {
        char buf[96];
        if (sq_app_wifi_connected()) {
            time_t now = time(NULL);
            snprintf(buf, sizeof(buf), "已联网%s",
                     now > 1700000000 ? "（时间已同步）" : "（时间同步中…）");
        } else {
            snprintf(buf, sizeof(buf),
                     "等待配置：手机连热点 AStock，\n访问 192.168.4.1 输入家中 Wi-Fi");
        }
        lv_label_set_text(s_status, buf);
    }
    if (s_hint) {
        if (s_reset_armed) {
            sq_hint_set(s_hint, "再长按一次清除凭据重配");
        } else if (sq_app_wifi_connected()) {
            sq_hint_set(s_hint, "已联网 · 长按确定重新配网");
        } else {
            sq_hint_set(s_hint, "长按确定清除凭据重配");
        }
    }
}

static void tick_cb(lv_timer_t *timer)
{
    (void)timer;
    if (sq_app_wifi_connected() && !s_dns_done) {
        s_dns_done = true;
        sq_wifi_ensure_dns();
        // 时间同步在 sq_net 里做；这里只负责页面刷新。
    }
    wifi_refresh();
}

lv_obj_t *sq_page_wifi_enter(void)
{
    s_status = s_hint = NULL;
    s_reset_armed = false;
    s_dns_done = sq_app_wifi_connected();

    lv_obj_t *card = NULL;
    lv_obj_t *screen = sq_page_create(&card);
    if (!card) return screen;

    sq_topbar_t bar = sq_topbar_create(card, "Wi-Fi 配网");
    sq_topbar_add_battery(&bar);
    s_hint = sq_hint_create(card, "");
    lv_obj_t *body = sq_body_create(card);

    s_status = sq_label_create(body, "", &sq_font_16, SQ_C_INK);
    lv_obj_set_pos(s_status, SQ_BODY_X, 12);
    lv_obj_set_width(s_status, SQ_BODY_W);
    lv_obj_set_height(s_status, 120);
    lv_label_set_long_mode(s_status, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_line_space(s_status, 6, 0);

    s_tick = lv_timer_create(tick_cb, 1000, NULL);
    wifi_refresh();
    return screen;
}

void sq_page_wifi_leave(void)
{
    if (s_tick) {
        lv_timer_del(s_tick);
        s_tick = NULL;
    }
    s_status = s_hint = NULL;
}

void sq_page_wifi_key(sq_key_t key)
{
    switch (key) {
        case SQ_KEY_OK:
            if (!s_reset_armed) {
                s_reset_armed = true;
                sq_sfx_play(SQ_TONE_ENTER);
                wifi_refresh();
                return;
            }
            // 清凭据 + 重开配网热点。
            ESP_LOGI(TAG, "清除 Wi-Fi 凭据并重开配网热点");
            sq_wifi_reset_credentials();
            s_reset_armed = false;
            sq_sfx_play(SQ_TONE_BACK);
            wifi_refresh();
            return;
        case SQ_KEY_BACK:
            s_reset_armed = false;
            sq_sfx_play(SQ_TONE_BACK);
            sq_app_goto_title();
            return;
        default:
            return;
    }
}
