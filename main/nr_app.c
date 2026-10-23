// main/nr_app.c —— 见 nr_app.h。
#include "nr_app.h"

#include <string.h>

#include "bsp_display.h"
#include "esp_log.h"

#include "nr_player.h"
#include "nr_sfx.h"
#include "nr_station.h"
#include "nr_store.h"
#include "nr_volume.h"

// 页面实现（nr_page_*.c）。
lv_obj_t *nr_page_title_enter(void);
void nr_page_title_leave(void);
void nr_page_title_key(nr_key_t key);
lv_obj_t *nr_page_radio_enter(void);
void nr_page_radio_leave(void);
void nr_page_radio_key(nr_key_t key);
// 音量变化后由 nr_app 通知收音机页刷新浮层（只在收音机页有效）。
void nr_page_radio_on_volume(void);
lv_obj_t *nr_page_wifi_enter(void);
void nr_page_wifi_leave(void);
void nr_page_wifi_key(nr_key_t key);

static const char *TAG = "nr_app";

typedef enum {
    PAGE_NONE = 0,
    PAGE_TITLE,
    PAGE_RADIO,
    PAGE_WIFI,
} page_id_t;

static uint8_t s_station;
static uint8_t s_volume = NR_VOLUME_DEFAULT;
static bool s_wifi_connected;

static lv_obj_t *s_screen;
static page_id_t s_page = PAGE_NONE;
static void (*s_leave)(void);

static void replace_page(const page_id_t page, lv_obj_t *(*enter)(void),
                         void (*leave)(void))
{
    if (s_leave) s_leave();
    lv_obj_t *next = enter();
    if (s_screen) lv_obj_delete(s_screen);
    s_screen = next;
    s_page = page;
    s_leave = leave;
    lv_scr_load(s_screen);
}

void nr_app_goto_title(void) { replace_page(PAGE_TITLE, nr_page_title_enter, nr_page_title_leave); }
void nr_app_goto_radio(void) { replace_page(PAGE_RADIO, nr_page_radio_enter, nr_page_radio_leave); }
void nr_app_goto_wifi(void) { replace_page(PAGE_WIFI, nr_page_wifi_enter, nr_page_wifi_leave); }

uint8_t nr_app_station(void) { return s_station; }

void nr_app_set_station(const uint8_t index)
{
    if (!nr_station_valid(index)) return;
    s_station = index;
    (void)nr_store_save_station(index);
}

uint8_t nr_app_volume(void) { return s_volume; }

void nr_app_volume_step(const int dir)
{
    s_volume = dir > 0 ? nr_volume_up(s_volume) : nr_volume_down(s_volume);
    (void)nr_store_save_volume(s_volume);
    nr_player_set_volume(s_volume);
    nr_sfx_set_volume(s_volume);
    nr_page_radio_on_volume(); // 只在收音机页有实体；其他页调用无害
}

bool nr_app_wifi_connected(void) { return s_wifi_connected; }
void nr_app_note_wifi(const bool connected) { s_wifi_connected = connected; }

void nr_app_key(const nr_key_t key)
{
    if (!bsp_lvgl_lock(500)) {
        ESP_LOGW(TAG, "未能取得 LVGL 锁，本次按键被丢弃");
        return;
    }
    switch (s_page) {
        case PAGE_TITLE: nr_page_title_key(key); break;
        case PAGE_RADIO: nr_page_radio_key(key); break;
        case PAGE_WIFI:  nr_page_wifi_key(key); break;
        default: break;
    }
    bsp_lvgl_unlock();
}

void nr_app_start(void)
{
    uint8_t saved = 0;
    if (nr_store_load_station(&saved) == ESP_OK) {
        s_station = saved;
        ESP_LOGI(TAG, "恢复上次电台：%u %s", s_station, NR_STATIONS[s_station].name);
    } else {
        ESP_LOGI(TAG, "无电台存档，从第 1 台开始");
    }

    uint8_t volume = NR_VOLUME_DEFAULT;
    if (nr_store_load_volume(&volume) == ESP_OK) {
        s_volume = volume;
    } else {
        ESP_LOGI(TAG, "无音量存档，用 %u%%", NR_VOLUME_DEFAULT);
    }
    nr_player_set_volume(s_volume);

    s_page = PAGE_NONE;
    s_leave = NULL;
    s_screen = NULL;
    nr_app_goto_title();
}
