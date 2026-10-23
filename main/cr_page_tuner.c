// main/cr_page_tuner.c —— 收音机主页：复古刻度盘 + 台名 + 状态 + 音量浮层。
//
// 复古拟物三件套：
//   刻度盘 —— 88~108 MHz 一排刻度（主刻度带数字），指针按台号均分游走，
//             换台时指针滑动过去；
//   调谐指示 —— 播放中五根琥珀竖条按正弦起伏（lv_anim），暂停时齐平；
//   音量浮层 —— 长按上下出现，1.6 秒自动消隐。
// 页面只做展示与按键路由；播放请求全部交给 cr_player，状态靠 300ms 轮询。
#include "cr_app.h"

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"

#include "cr_player.h"
#include "cr_sfx.h"
#include "cr_station.h"
#include "cr_ui.h"
#include "cr_volume.h"

static const char *TAG = "cr_tuner";

static lv_obj_t *s_pointer;      // 调谐指针
static lv_obj_t *s_cat_label;
static lv_obj_t *s_name_label;
static lv_obj_t *s_status_label;
static lv_obj_t *s_info_label;
static lv_obj_t *s_vu[5];        // 调谐指示条
static bool s_vu_dancing;
static lv_obj_t *s_vol_panel;
static lv_obj_t *s_vol_bar;
static lv_obj_t *s_vol_text;
static lv_timer_t *s_vol_timer;
static lv_timer_t *s_tick;
static cr_topbar_t s_bar;
static lv_obj_t *s_hint_label;
static bool s_started;           // 本次开机是否按过播放（区分「待机」与「已暂停」）
static int64_t s_suppress_until; // 瞬时提示（如「未联网」）在此前不被轮询覆盖

// ------------------------------------------------------------- 刻度盘 ------
static int dial_x_for(const uint8_t index)
{
    // 台号均分到 [DIAL_PAD, DIAL_W - DIAL_PAD]。
    const int span = CR_DIAL_W - 2 * CR_DIAL_PAD;
    return CR_DIAL_X + CR_DIAL_PAD +
           ((int)index * span) / (CR_STATION_COUNT > 1 ? CR_STATION_COUNT - 1 : 1);
}

static void pointer_move_to(const uint8_t index, const bool animate)
{
    if (!s_pointer) return;
    const int x = dial_x_for(index) - 1; // 3px 宽指针居中于刻度点
    if (!animate) {
        lv_obj_set_x(s_pointer, x);
        return;
    }
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_pointer);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_x);
    lv_anim_set_values(&a, lv_obj_get_x(s_pointer), x);
    lv_anim_set_duration(&a, 180);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

static void dial_create(lv_obj_t *card)
{
    lv_obj_t *panel = lv_obj_create(card);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(panel, CR_DIAL_X, CR_DIAL_Y);
    lv_obj_set_size(panel, CR_DIAL_W, CR_DIAL_H);
    lv_obj_set_style_radius(panel, 10, 0);
    lv_obj_set_style_bg_color(panel, lv_color_hex(CR_C_SCREEN), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel, 2, 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(CR_C_LINE), 0);
    lv_obj_set_style_pad_all(panel, 0, 0);

    // 刻度：2MHz 一格（11 条），只在 88/96/104 标数字 —— 控件数是 LVGL 池的
    // 硬开销（24KB 池曾因本页对象过多分配失败崩溃），能省就省。
    const int span = CR_DIAL_W - 2 * CR_DIAL_PAD;
    for (int f = CR_DIAL_FREQ_LO; f <= CR_DIAL_FREQ_HI; f += 2) {
        const int x = CR_DIAL_PAD + ((f - CR_DIAL_FREQ_LO) * span) /
                                    (CR_DIAL_FREQ_HI - CR_DIAL_FREQ_LO);
        const bool major = (f - CR_DIAL_FREQ_LO) % 8 == 0;
        lv_obj_t *tick = lv_obj_create(panel);
        lv_obj_remove_flag(tick, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(tick, major ? 2 : 1, major ? 12 : 7);
        lv_obj_set_pos(tick, x, 16);
        lv_obj_set_style_radius(tick, 0, 0);
        lv_obj_set_style_border_width(tick, 0, 0);
        lv_obj_set_style_pad_all(tick, 0, 0);
        lv_obj_set_style_bg_opa(tick, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(tick, lv_color_hex(major ? CR_C_AMBER_D
                                                           : CR_C_LINE), 0);
        if (major) {
            char text[8];
            snprintf(text, sizeof(text), "%d", f);
            lv_obj_t *num = cr_label_create(panel, text, &cr_font_16, CR_C_MUTED);
            lv_obj_set_width(num, 30);
            lv_obj_set_height(num, CR_LINE_H_SMALL);
            lv_obj_set_style_text_align(num, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_pos(num, x - 15, 32);
        }
    }

    // 指针：3px 竖线 + 顶部小圆点，琥珀橙。
    s_pointer = lv_obj_create(panel);
    lv_obj_remove_flag(s_pointer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(s_pointer, 3, CR_DIAL_H - 8);
    lv_obj_set_style_radius(s_pointer, 1, 0);
    lv_obj_set_style_border_width(s_pointer, 0, 0);
    lv_obj_set_style_pad_all(s_pointer, 0, 0);
    lv_obj_set_style_bg_color(s_pointer, lv_color_hex(CR_C_POINTER), 0);
    lv_obj_set_style_bg_opa(s_pointer, LV_OPA_COVER, 0);
    // 指针是面板子对象：面板内坐标 = 全局刻度 x - 面板原点。
    lv_obj_set_pos(s_pointer, dial_x_for(cr_app_station()) - CR_DIAL_X - 1, 4);
}

// ----------------------------------------------------------- 指示条 --------
static void vu_set_height(lv_obj_t *bar, const int h)
{
    lv_obj_set_height(bar, h);
    lv_obj_set_y(bar, CR_RADIO_VU_Y + CR_RADIO_VU_H - h);
}

static void vu_exec_cb(void *var, const int32_t v)
{
    vu_set_height((lv_obj_t *)var, (int)v);
}

static void vu_stop(void)
{
    if (!s_vu_dancing) return;
    for (int i = 0; i < 5; i++) {
        lv_anim_del(s_vu[i], vu_exec_cb);
        vu_set_height(s_vu[i], 6);
    }
    s_vu_dancing = false;
}

static void vu_start(void)
{
    if (s_vu_dancing) return;
    for (int i = 0; i < 5; i++) {
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, s_vu[i]);
        lv_anim_set_exec_cb(&a, vu_exec_cb);
        lv_anim_set_values(&a, 6, CR_RADIO_VU_H - (i % 2) * 8);
        lv_anim_set_duration(&a, 520 + i * 90);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_playback_duration(&a, 430 + i * 70);
        lv_anim_start(&a);
    }
    s_vu_dancing = true;
}

static void vu_create(lv_obj_t *card)
{
    const int total = 5 * 6 + 4 * 10;
    int x = (CR_PAGE_W - total) / 2;
    for (int i = 0; i < 5; i++) {
        s_vu[i] = lv_obj_create(card);
        lv_obj_remove_flag(s_vu[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(s_vu[i], 6, 6);
        vu_set_height(s_vu[i], 6);
        lv_obj_set_x(s_vu[i], x);
        lv_obj_set_style_radius(s_vu[i], 2, 0);
        lv_obj_set_style_border_width(s_vu[i], 0, 0);
        lv_obj_set_style_pad_all(s_vu[i], 0, 0);
        lv_obj_set_style_bg_color(s_vu[i], lv_color_hex(i % 2 ? CR_C_AMBER_D
                                                              : CR_C_AMBER), 0);
        lv_obj_set_style_bg_opa(s_vu[i], LV_OPA_COVER, 0);
        x += 16;
    }
    s_vu_dancing = false;
}

// ------------------------------------------------------------ 音量浮层 ------
static void vol_hide_cb(lv_timer_t *timer)
{
    (void)timer;
    if (s_vol_panel) lv_obj_add_flag(s_vol_panel, LV_OBJ_FLAG_HIDDEN);
}

static void vol_refresh(void)
{
    if (!s_vol_panel) return;
    const uint8_t level = cr_app_volume();
    lv_bar_set_value(s_vol_bar, level, LV_ANIM_OFF);
    char text[8];
    (void)cr_volume_text(level, text, sizeof(text));
    lv_label_set_text(s_vol_text, text);
    lv_obj_clear_flag(s_vol_panel, LV_OBJ_FLAG_HIDDEN);
    // 重置一次性消隐计时。
    if (s_vol_timer) {
        lv_timer_reset(s_vol_timer);
        lv_timer_resume(s_vol_timer);
    }
}

void cr_page_tuner_on_volume(void)
{
    vol_refresh();
}

// -------------------------------------------------------------- 刷新 -------
static void status_apply(const cr_play_status_t *st)
{
    uint32_t color = CR_C_MUTED;
    char line[64];

    switch (st->state) {
        case CR_PLAY_PLAYING:
            snprintf(line, sizeof(line), "%s", st->msg[0] ? st->msg : "正在播放");
            color = CR_C_AMBER;
            vu_start();
            break;
        case CR_PLAY_CONNECT:
        case CR_PLAY_BUFFERING:
            snprintf(line, sizeof(line), "%s",
                     st->msg[0] ? st->msg : "连接中…");
            vu_stop();
            break;
        case CR_PLAY_ERROR:
            snprintf(line, sizeof(line), "%s", st->msg[0] ? st->msg : "出错");
            color = CR_C_ERR;
            vu_stop();
            break;
        case CR_PLAY_IDLE:
        default:
            snprintf(line, sizeof(line), "%s", s_started ? "已暂停" : "按 OK 收听");
            vu_stop();
            break;
    }
    static char last_line[64];
    static uint32_t last_color;
    if (strcmp(last_line, line) != 0) {
        snprintf(last_line, sizeof(last_line), "%s", line);
        lv_label_set_text(s_status_label, line);
    }
    if (last_color != color) {
        last_color = color;
        lv_obj_set_style_text_color(s_status_label, lv_color_hex(color), 0);
    }

    if (st->state == CR_PLAY_PLAYING && st->kbps > 0 && st->sample_hz > 0) {
        char info[40];
        snprintf(info, sizeof(info), "%ukbps · %u.%ukHz%s",
                 st->kbps, (unsigned)(st->sample_hz / 1000),
                 (unsigned)((st->sample_hz % 1000) / 100),
                 st->channels == 1 ? " 单声道" : "");
        static char last_info[40];
        if (strcmp(last_info, info) != 0) {
            snprintf(last_info, sizeof(last_info), "%s", info);
            lv_label_set_text(s_info_label, info);
        }
    } else if (st->state == CR_PLAY_IDLE || st->state == CR_PLAY_ERROR) {
        lv_label_set_text(s_info_label, "");
    }
}

static void station_apply(void)
{
    const uint8_t index = cr_app_station();
    lv_label_set_text(s_cat_label, cr_station_cat_name(CR_STATIONS[index].cat));
    lv_label_set_text(s_name_label, CR_STATIONS[index].name);
    pointer_move_to(index, true);
}

static void tick_cb(lv_timer_t *timer)
{
    (void)timer;
    cr_topbar_set_wifi(&s_bar, cr_app_wifi_connected(), true);
    if (esp_timer_get_time() / 1000 < s_suppress_until) return; // 瞬时提示展示期
    cr_play_status_t st;
    cr_player_status(&st);
    status_apply(&st);

}

// -------------------------------------------------------------- 页面 -------
lv_obj_t *cr_page_tuner_enter(void)
{
    s_hint_label = NULL;
    s_vol_panel = NULL;
    s_vol_bar = NULL;
    s_vol_text = NULL;
    s_vol_timer = NULL;
    s_tick = NULL;

    lv_obj_t *card = NULL;
    lv_obj_t *screen = cr_page_create(&card);
    if (!card) return screen;

    s_bar = cr_topbar_create(card, "央广播音机");
    cr_topbar_add_battery(&s_bar);
    cr_topbar_set_wifi(&s_bar, cr_app_wifi_connected(), true);

    dial_create(card);

    s_cat_label = cr_label_create(card, "", &cr_font_16, CR_C_MUTED);
    lv_obj_set_width(s_cat_label, CR_PAGE_W - 20);
    lv_obj_set_height(s_cat_label, CR_LINE_H_SMALL);
    lv_obj_set_style_text_align(s_cat_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_cat_label, 10, CR_RADIO_CAT_Y);

    s_name_label = cr_label_create(card, "", &cr_font_24, CR_C_AMBER);
    lv_obj_set_width(s_name_label, CR_PAGE_W - 20);
    lv_obj_set_height(s_name_label, CR_LINE_H_BODY);
    lv_label_set_long_mode(s_name_label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(s_name_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_name_label, 10, CR_RADIO_NAME_Y);

    s_status_label = cr_label_create(card, "", &cr_font_16, CR_C_MUTED);
    lv_obj_set_width(s_status_label, CR_PAGE_W - 20);
    lv_obj_set_height(s_status_label, CR_LINE_H_SMALL);
    lv_label_set_long_mode(s_status_label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(s_status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_status_label, 10, CR_RADIO_STATUS_Y);

    s_info_label = cr_label_create(card, "", &cr_font_16, CR_C_MUTED);
    lv_obj_set_width(s_info_label, CR_PAGE_W - 20);
    lv_obj_set_height(s_info_label, CR_LINE_H_SMALL);
    lv_label_set_long_mode(s_info_label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_set_style_text_align(s_info_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_info_label, 10, CR_RADIO_INFO_Y);

    vu_create(card);

    s_hint_label = cr_hint_create(card, "上下换台 · OK 播放暂停 · 长按音量");

    // 音量浮层（最后创建，压在最上层；初始隐藏）。
    s_vol_panel = lv_obj_create(card);
    lv_obj_remove_flag(s_vol_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(s_vol_panel, CR_VOL_W, CR_VOL_H);
    lv_obj_set_pos(s_vol_panel, CR_VOL_X, CR_VOL_Y);
    lv_obj_set_style_radius(s_vol_panel, 12, 0);
    lv_obj_set_style_bg_color(s_vol_panel, lv_color_hex(CR_C_PANEL_ALT), 0);
    lv_obj_set_style_bg_opa(s_vol_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_vol_panel, 2, 0);
    lv_obj_set_style_border_color(s_vol_panel, lv_color_hex(CR_C_AMBER_D), 0);
    lv_obj_set_style_pad_all(s_vol_panel, 0, 0);
    lv_obj_add_flag(s_vol_panel, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *vol_title = cr_label_create(s_vol_panel, "音量", &cr_font_16, CR_C_INK);
    lv_obj_align(vol_title, LV_ALIGN_LEFT_MID, 10, 0);

    s_vol_bar = lv_bar_create(s_vol_panel);
    lv_obj_set_size(s_vol_bar, 62, 8);
    lv_bar_set_range(s_vol_bar, 0, 100);
    lv_bar_set_value(s_vol_bar, cr_app_volume(), LV_ANIM_OFF);
    lv_obj_align(s_vol_bar, LV_ALIGN_LEFT_MID, 48, 0);
    lv_obj_set_style_bg_color(s_vol_bar, lv_color_hex(CR_C_LINE), 0);
    lv_obj_set_style_bg_opa(s_vol_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(s_vol_bar, lv_color_hex(CR_C_AMBER), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_vol_bar, LV_OPA_COVER, LV_PART_INDICATOR);

    s_vol_text = cr_label_create(s_vol_panel, "", &cr_font_16, CR_C_AMBER);
    lv_obj_align(s_vol_text, LV_ALIGN_RIGHT_MID, -8, 0);


    station_apply();
    // 真机诊断（确认后移除）：LVGL 池水位——池耗尽会让 lv_label_set_text
    // 的文本缓冲分配失败，随后渲染空指针崩溃。
    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);
    ESP_LOGD(TAG, "LVGL 池: 已用 %u%% 空闲 %u 字节 最大块 %u",
             (unsigned)mon.used_pct, (unsigned)mon.free_size,
             (unsigned)mon.free_biggest_size);
    s_tick = lv_timer_create(tick_cb, 300, NULL);
    s_vol_timer = lv_timer_create(vol_hide_cb, CR_VOL_HIDE_MS, NULL);
    lv_timer_pause(s_vol_timer); // 到点才恢复（vol_refresh 里 resume）
    return screen;
}

void cr_page_tuner_leave(void)
{
    if (s_tick) {
        lv_timer_del(s_tick);
        s_tick = NULL;
    }
    if (s_vol_timer) {
        lv_timer_del(s_vol_timer);
        s_vol_timer = NULL;
    }
    // 离开主页 = 收听结束（返回键路径已停播；这里是兜底）。
    cr_player_stop();
    s_pointer = NULL;
    s_cat_label = s_name_label = s_status_label = s_info_label = NULL;
    memset(s_vu, 0, sizeof(s_vu));
    s_vol_panel = s_vol_bar = s_vol_text = NULL;
}

// 三键语义（与家族标准一致；幻影按键根源已除——真凶修复后连续播放 4 分钟
// 零幻影实测，功放电流稳定不再扰 ADC）：
//   上下单击   换台（播放中热切换）
//   OK 单击    播放/暂停
//   长按上下   音量 ±
//   长按 OK    回标题页并停播

void cr_page_tuner_key(const cr_key_t key)
{
    switch (key) {
        case CR_KEY_UP:
        case CR_KEY_DOWN: {
            const uint8_t index = key == CR_KEY_UP
                                      ? cr_station_prev(cr_app_station())
                                      : cr_station_next(cr_app_station());
            cr_app_set_station(index);
            cr_sfx_play(CR_TONE_MOVE);
            station_apply();
            // 正在收听：立即切台；待机/暂停：只换选择。
            cr_play_status_t st;
            cr_player_status(&st);
            if (st.state != CR_PLAY_IDLE && st.state != CR_PLAY_ERROR) {
                cr_player_play(index);
            }
            break;
        }
        case CR_KEY_OK: {
            cr_play_status_t st;
            cr_player_status(&st);
            if (st.state == CR_PLAY_CONNECT || st.state == CR_PLAY_BUFFERING ||
                st.state == CR_PLAY_PLAYING) {
                cr_player_stop();
                cr_sfx_play(CR_TONE_BACK);
                break;
            }
            if (!cr_app_wifi_connected()) {
                cr_sfx_play(CR_TONE_DECLINED);
                lv_label_set_text(s_status_label, "未联网 · 长按确定去配网");
                lv_obj_set_style_text_color(s_status_label,
                                            lv_color_hex(CR_C_ERR), 0);
                s_suppress_until = esp_timer_get_time() / 1000 + 2500;
                break;
            }
            s_started = true;
            cr_sfx_play(CR_TONE_ENTER);
            cr_player_play(cr_app_station());
            break;
        }
        case CR_KEY_VOL_UP:
            cr_app_volume_step(1);
            break;
        case CR_KEY_VOL_DOWN:
            cr_app_volume_step(-1);
            break;
        case CR_KEY_BACK:
            cr_sfx_play(CR_TONE_BACK);
            cr_player_stop();
            cr_app_goto_title();
            break;
        default:
            break;
    }
}
