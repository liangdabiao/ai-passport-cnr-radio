// main/sq_market.h —— A 股交易时段判定（纯逻辑，不依赖平台时间源）。
//
// 设备没有 RTC；时间来自 SNTP（sq_net 连网后同步），这里只做纯换算。
// 判定按北京时间（UTC+8）：
//   周一至周五，9:15–11:35 或 12:55–15:05 视为「行情时段」——比真实交易
//   时段两头各放宽几分钟，把集合竞价和收盘成交也算进去；周末必休。
//   法定节假日没有日历数据，节假日里会照常尝试刷新——拿到的是上一交易日
//   的行情，服务器时间戳（sq_quote_batch_t.epoch）会如实标出，不误导。
#pragma once

#include <stdbool.h>
#include <stdint.h>

// 给定 Unix 时间（秒），判断是否处于行情时段（北京时间）。
bool sq_market_is_trading(uint64_t epoch);

// 给定 Unix 时间，是否周末（北京时间周六/周日）。
bool sq_market_is_weekend(uint64_t epoch);
