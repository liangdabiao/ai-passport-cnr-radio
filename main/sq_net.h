// main/sq_net.h —— 网络任务：行情拉取、解析与结果投递。
//
// 独立任务（不碰 LVGL）：应用登记回调与自选名单，请求刷新时任务做
// raw-socket HTTP GET（hot-daily 真机教训：esp_http_client 在 C3+lwip 不可
// 用）→ 解析 → 缓存 NVS → 回调。回调在**网络任务上下文**执行，实现方自己
// 拿 LVGL 锁。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "sq_quote.h"
#include "sq_watch.h"

// 启动网络任务。
void sq_net_start(void);

// 结果回调：ok=本次拉取并解析成功；batch 为最新快照（回调返回后失效，
// 需要保留请拷贝）。
void sq_net_set_callback(void (*on_result)(bool ok, const sq_quote_batch_t *batch,
                                           void *user),
                         void *user);

// 把自选名单的副本交给网络任务（添加/删除后调用一次）。
void sq_net_set_watchlist(const sq_watchlist_t *list);

// 请求一次刷新（网络任务空闲时执行；连击合并为一次）。
void sq_net_request_refresh(void);

// 最近一次拉取是否成功（UI 显示「已连上行情」用）。
bool sq_net_last_ok(void);


