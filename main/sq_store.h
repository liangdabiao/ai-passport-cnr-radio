// main/sq_store.h —— 自选名单与设置的 NVS 落盘（含行情缓存）。
//
// 三类数据分键存：watchlist blob（自选）、settings（刷新间隔）、cache
// （最后一次行情快照，断网时列表页显示缓存并标注行情时间）。缓存以
// 「序列化快照」整存整取，格式由 sq_net 的快照序列化函数定义。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "sq_quote.h"
#include "sq_watch.h"
#include "esp_err.h"

// 打开命名空间。失败时应用仍可运行，只是不持久化。
esp_err_t sq_store_init(void);

// 自选名单。首次开机不算失败：复位成出厂默认（上证指数 + 深证成指）。
esp_err_t sq_store_load_watch(sq_watchlist_t *list);
esp_err_t sq_store_save_watch(const sq_watchlist_t *list);

// 刷新秒数（15/30/60，出厂 30）。与交易时段判定配合使用。
esp_err_t sq_store_load_refresh(uint16_t *seconds);
esp_err_t sq_store_save_refresh(uint16_t seconds);

// 行情缓存（sq_net 序列化的快照）。没有缓存返回 ESP_ERR_NVS_NOT_FOUND。
esp_err_t sq_store_load_cache(uint8_t *blob, unsigned *size, unsigned capacity);
esp_err_t sq_store_save_cache(const uint8_t *blob, unsigned size);
