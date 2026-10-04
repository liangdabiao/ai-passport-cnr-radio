// main/sq_watch.h —— 自选股名单（纯逻辑，NVS 读写见 sq_store）。
//
// 内置预设（指数，代码为东财指数代码的数字部分，添加进自选后与个股同权），
// 预设不可删除但可不加入自选；用户自选上限 SQ_WATCH_CAP 只，去重。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "sq_symbol.h"

#define SQ_WATCH_CAP 8

typedef struct {
    char code[SQ_CODE_LEN + 1]; // 6 位数字代码（指数代码同样是 6 位数字）
    bool is_index;              // 指数（预设表成员，secid 规则与个股不同）
} sq_watch_entry_t;

typedef struct {
    sq_watch_entry_t entries[SQ_WATCH_CAP];
    uint8_t count;
} sq_watchlist_t;

// 内置预设：常用指数 + 热门个股。显示名用英文简称（代码+市场后缀的
// ASCII 方案 —— 服务器中文名不显示，字库只需 UI 子集）。
typedef struct {
    const char *code;
    const char *name; // ASCII 简称（SSE / MOUTAI …），进字库
    bool is_index;
} sq_preset_t;

// 预设的 ASCII 简称（按 code+域查）；非预设返回 NULL（页面显示 code.MKT）。
const char *sq_watch_preset_name(const char *code, bool is_index);

extern const sq_preset_t SQ_PRESETS[];
extern const uint8_t SQ_PRESET_COUNT;

void sq_watch_reset(sq_watchlist_t *list);

// 加入（去重；满员返回 false）。
bool sq_watch_add(sq_watchlist_t *list, const char *code, bool is_index);
// 移除（不存在返回 false）。
bool sq_watch_remove(sq_watchlist_t *list, const char *code);
bool sq_watch_has(const sq_watchlist_t *list, const char *code);

// secid 拼接：个股走推断，指数走内置映射（指数代码 → 东财 secid）。
// 指数代码与东财 secid 的映射放进预设表（sq_watch.c 内建查询）。
bool sq_watch_secid(const sq_watchlist_t *list, uint8_t index,
                    char *out, unsigned capacity);

// 序列化（NVS blob）：[0]=count，之后每项 7 字节 code + 1 字节标志。
#define SQ_WATCH_BLOB_SIZE (1 + SQ_WATCH_CAP * 8)
bool sq_watch_serialize(const sq_watchlist_t *list, uint8_t *out, unsigned capacity);
bool sq_watch_deserialize(sq_watchlist_t *list, const uint8_t *blob, unsigned size);
