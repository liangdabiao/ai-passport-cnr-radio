// main/sq_symbol.h —— 股票代码与东财 secid 的推断（纯逻辑）。
//
// 用户只输 6 位数字代码；市场归属按代码首位推断（A 股通行规则）：
//   6/9/5 开头 → 沪（东财 secid 前缀 "1."，如 600519 贵州茅台、688xxx 科创板）
//   0/2/3 开头 → 深（前缀 "0."，如 000001 平安银行、300xxx 创业板）
//   4/8 开头   → 北交所（东财前缀 "0."）—— 一期不做，明确拒绝。
// 指数不在此列（东财指数 secid 是 1.000001 等，与个股规则不同），指数走
// 内置预设表（sq_watch.h 的预设段），不走添加页。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define SQ_CODE_LEN 6
#define SQ_SECID_MAX 16 // "1.600519" 最长 9，留余量

// 校验 6 位数字代码并推断市场。code 必须是 '\0' 结尾的 6 位数字。
bool sq_symbol_valid(const char *code);

// 6 位代码 → 东财 secid（"1.600519" / "0.000001"）。非法代码返回 false。
bool sq_symbol_secid(const char *code, char *out, unsigned capacity);

// 市场名（"沪" / "深"），用于详情页展示；非法代码返回 ""。
const char *sq_symbol_market_name(const char *code);
