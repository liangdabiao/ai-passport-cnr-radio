// main/nr_station.h —— 内置电台清单与分类。
//
// 清单固化在固件里（v1 不做在线搜台）。每个 URL 都在 2026-10-23 用
// tools/net-radio/station_http.py 做过「纯 http GET 收流 8 秒」实测（收满
// 32KB 才算过），被拒的 https-only 台一律不收 —— 设备端 raw socket 不带 TLS。
//
// 本模块与 ESP-IDF/LVGL 解耦，宿主测试直接编译它。
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    NR_CAT_NEWS = 0, // 新闻
    NR_CAT_MUSIC,    // 音乐
    NR_CAT_QUYI,     // 曲艺（相声评书）
    NR_CAT_STORY,    // 故事
    NR_CAT_WORLD,    // 环球（海外英文音乐台）
    NR_CAT_COUNT,
} nr_cat_t;

typedef struct {
    const char *name; // 短名：全角 ≤7 字或半角 ≤13 字符（24px 一行硬保证）
    const char *url;  // 纯 http 直链（实测存活）
    uint8_t cat;      // nr_cat_t
} nr_station_t;

// 清单本体（nr_station.c 定义，分类连续排列）。数量在头文件钉死：宿主测试
// 与页面几何都要引用；nr_station.c 里用 _Static_assert 防止两处走样。
#define NR_STATION_COUNT 26
extern const nr_station_t NR_STATIONS[NR_STATION_COUNT];

// 下一个 / 上一个台（线性循环，越界回绕；这是家族真机验证过的导航形态）。
uint8_t nr_station_next(uint8_t index);
uint8_t nr_station_prev(uint8_t index);

// 分类名（"新闻"/"音乐"/"曲艺"/"故事"/"环球"），越界返回空串。
const char *nr_station_cat_name(uint8_t cat);

// 索引合法性（范围与空指针防护：清单为空时任何索引都非法）。
bool nr_station_valid(uint8_t index);
