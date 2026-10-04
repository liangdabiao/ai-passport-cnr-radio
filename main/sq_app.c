// main/sq_app.c —— 见 sq_app.h。
#include "sq_app.h"

#include <string.h>

#include "bsp_display.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "sq_net.h"
#include "sq_sfx.h"
#include "sq_store.h"
#include "sq_wifi.h"

static const char *TAG = "sq_app";

typedef enum {
    PAGE_NONE = 0,
    PAGE_TITLE,
    PAGE_LIST,
    PAGE_DETAIL,
    PAGE_ADD,
    PAGE_WIFI,
    PAGE_SETTINGS,
} page_id_t;

static sq_watchlist_t s_watch;
static sq_quote_batch_t s_snapshot;
static bool s_snapshot_valid;
static uint16_t s_refresh = 30;
static uint8_t s_selected;
static bool s_wifi_connected;

static lv_obj_t *s_screen;
static page_id_t s_page = PAGE_NONE;
static void (*s_leave)(void);

static void replace_page(page_id_t page, lv_obj_t *(*enter)(void), void (*leave)(void))
{
    if (s_leave) s_leave();
    lv_obj_t *next = enter();
    if (s_screen) lv_obj_delete(s_screen);
    s_screen = next;
    s_page = page;
    s_leave = leave;
    lv_scr_load(s_screen);
}

void sq_app_goto_title(void) { replace_page(PAGE_TITLE, sq_page_title_enter, sq_page_title_leave); }
void sq_app_goto_list(void) { replace_page(PAGE_LIST, sq_page_list_enter, sq_page_list_leave); }
void sq_app_goto_detail(void) { replace_page(PAGE_DETAIL, sq_page_detail_enter, sq_page_detail_leave); }
void sq_app_goto_add(void) { replace_page(PAGE_ADD, sq_page_add_enter, sq_page_add_leave); }
void sq_app_goto_wifi(void) { replace_page(PAGE_WIFI, sq_page_wifi_enter, sq_page_wifi_leave); }
void sq_app_goto_settings(void) { replace_page(PAGE_SETTINGS, sq_page_settings_enter, sq_page_settings_leave); }

sq_watchlist_t *sq_app_watch(void) { return &s_watch; }
const sq_quote_batch_t *sq_app_snapshot(void) { return &s_snapshot; }
bool sq_app_snapshot_valid(void) { return s_snapshot_valid; }
uint16_t sq_app_refresh_seconds(void) { return s_refresh; }

void sq_app_cycle_refresh(void)
{
    s_refresh = s_refresh == 15 ? 30 : (s_refresh == 30 ? 60 : 15);
    if (sq_store_save_refresh(s_refresh) != ESP_OK) {
        ESP_LOGW(TAG, "刷新间隔未能持久化");
    }
}

uint8_t sq_app_selected(void) { return s_selected; }
void sq_app_set_selected(uint8_t index) { s_selected = index; }

bool sq_app_wifi_connected(void) { return s_wifi_connected; }
void sq_app_note_wifi(bool connected) { s_wifi_connected = connected; }

void sq_app_request_refresh(void)
{
    sq_net_set_watchlist(&s_watch);
    sq_net_request_refresh();
}

// 网络任务上下文回调：拿 LVGL 锁后进应用状态并通知列表页。
void sq_app_on_net_result(bool ok, const sq_quote_batch_t *batch, void *user)
{
    (void)user;
    if (!bsp_lvgl_lock(1500)) {
        ESP_LOGW(TAG, "结果投递未拿到 LVGL 锁，丢弃本次");
        return;
    }
    if (ok && batch) {
        s_snapshot = *batch;
        s_snapshot_valid = true;
    } else {
        s_snapshot_valid = false;
    }
    if (s_page == PAGE_LIST) sq_page_list_on_result(ok);
    bsp_lvgl_unlock();
}

void sq_app_key(sq_key_t key)
{
    if (!bsp_lvgl_lock(500)) {
        ESP_LOGW(TAG, "未能取得 LVGL 锁，本次按键被丢弃");
        return;
    }
    switch (s_page) {
        case PAGE_TITLE:    sq_page_title_key(key); break;
        case PAGE_LIST:     sq_page_list_key(key); break;
        case PAGE_DETAIL:   sq_page_detail_key(key); break;
        case PAGE_ADD:      sq_page_add_key(key); break;
        case PAGE_WIFI:     sq_page_wifi_key(key); break;
        case PAGE_SETTINGS: sq_page_settings_key(key); break;
        default: break;
    }
    bsp_lvgl_unlock();
}

void sq_app_start(void)
{
    const esp_err_t err = sq_store_load_watch(&s_watch);
    if (err != ESP_OK) ESP_LOGW(TAG, "自选读取未成功（%s），用出厂默认", esp_err_to_name(err));

    uint16_t refresh = 30;
    if (sq_store_load_refresh(&refresh) != ESP_OK) ESP_LOGI(TAG, "无刷新存档，用 30s");
    s_refresh = refresh;

    // 恢复上次的行情缓存（断网兜底）。
    static uint8_t blob[640];
    unsigned size = 0;
    if (sq_store_load_cache(blob, &size, sizeof(blob)) == ESP_OK &&
        sq_quote_deserialize(blob, size, &s_snapshot)) {
        s_snapshot_valid = true;
        ESP_LOGI(TAG, "行情缓存恢复：%u 条", s_snapshot.count);
    }

    sq_net_set_watchlist(&s_watch);
    sq_net_set_callback(sq_app_on_net_result, NULL);

    s_page = PAGE_NONE;
    s_leave = NULL;
    s_screen = NULL;
    sq_app_goto_title();
}
