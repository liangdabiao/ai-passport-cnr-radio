// main/sq_app.h —— 导航、应用状态与页面跳转。
//
// 硬件只有三个键（上/下/确定，按下即响应，无双击）：
//   上 / 下   循环移动（菜单 / 自选行 / 落点……每页语义见各页头注）
//   确定      进入 / 确认
//   确定 长按 各页自定义：列表=手动刷新，添加=取消，配网=重新配网（两次）
//   上   长按 列表=详情；下 长按 列表=删除自选（两次）
//   全部页面：长按确定可退出或取消（语义见各页）
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lvgl.h"

#include "sq_quote.h"
#include "sq_watch.h"

typedef enum {
    SQ_KEY_UP = 0,
    SQ_KEY_DOWN,
    SQ_KEY_OK,
    SQ_KEY_BACK,
    SQ_KEY_REFRESH,  // 列表页：手动刷新
    SQ_KEY_DETAIL,   // 列表页：进详情
    SQ_KEY_REMOVE,   // 列表页：删除自选（两次确认）
    SQ_KEY_STEPBACK, // 添加页：数字轮回退一位
} sq_key_t;

void sq_app_start(void);
void sq_app_key(sq_key_t key);

void sq_app_goto_title(void);
void sq_app_goto_list(void);
void sq_app_goto_detail(void);
void sq_app_goto_add(void);
void sq_app_goto_wifi(void);
void sq_app_goto_settings(void);

// ---- 共享状态 ----
sq_watchlist_t *sq_app_watch(void);
const sq_quote_batch_t *sq_app_snapshot(void);
bool sq_app_snapshot_valid(void);
uint16_t sq_app_refresh_seconds(void);
void sq_app_cycle_refresh(void);

// 选中行（列表/详情共享），范围由页面夹取。
uint8_t sq_app_selected(void);
void sq_app_set_selected(uint8_t index);

// 网络回调入口（sq_net 的结果在这里进应用状态；内部拿 LVGL 锁）。
void sq_app_on_net_result(bool ok, const sq_quote_batch_t *batch, void *user);

// 请求一次刷新（转发给 sq_net）。
void sq_app_request_refresh(void);

// WiFi 状态缓存（sq_wifi 事件回调维护；UI 轮询读取）。
bool sq_app_wifi_connected(void);
void sq_app_note_wifi(bool connected);

// ---- 页面模块 ----
// 列表页接收网络结果（sq_app_on_net_result 在 LVGL 锁内调用）。
void sq_page_list_on_result(bool ok);

lv_obj_t *sq_page_title_enter(void);
void sq_page_title_leave(void);
void sq_page_title_key(sq_key_t key);

lv_obj_t *sq_page_list_enter(void);
void sq_page_list_leave(void);
void sq_page_list_key(sq_key_t key);

lv_obj_t *sq_page_detail_enter(void);
void sq_page_detail_leave(void);
void sq_page_detail_key(sq_key_t key);

lv_obj_t *sq_page_add_enter(void);
void sq_page_add_leave(void);
void sq_page_add_key(sq_key_t key);

lv_obj_t *sq_page_wifi_enter(void);
void sq_page_wifi_leave(void);
void sq_page_wifi_key(sq_key_t key);

lv_obj_t *sq_page_settings_enter(void);
void sq_page_settings_leave(void);
void sq_page_settings_key(sq_key_t key);
