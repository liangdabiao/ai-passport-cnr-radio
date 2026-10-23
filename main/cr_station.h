// main/cr_station.h —— 内置电台清单与分类（radio.cn 央广 + 省级台精选）。
//
// 清单固化在固件里（v1 不做在线搜台）。每个地址都是 2026-10-23 用
// tools/cnr-radio 的拉取脚本从 radio.cn 官方接口取得，并逐台做过「取播放列表
// → 下载一个 10 秒切片（≥16KB）」的存活验证 —— 设备端 HLS+AAC 直连。
//
// 本模块与 ESP-IDF/LVGL 解耦，宿主测试直接编译它。
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    CR_CAT_CNR = 0,    // 央广（国家台）
    CR_CAT_NORTH,      // 华北
    CR_CAT_NE,         // 东北
    CR_CAT_EAST,       // 华东
    CR_CAT_CENTRAL,    // 华中
    CR_CAT_SOUTH,      // 华南
    CR_CAT_SW,         // 西南
    CR_CAT_NW,         // 西北
    CR_CAT_COUNT,
} cr_cat_t;

typedef struct {
    const char *name; // 短名：全角 ≤7 字或半角 ≤13 字符（24px 一行硬保证）
    const char *url;  // radio.cn HLS 播放列表（纯 http，长效 key）
    uint8_t cat;      // cr_cat_t
} cr_station_t;

// 清单数量钉在头文件：宿主测试与页面几何都引用；cr_station.c 里用
// _Static_assert 防止两处走样。
#define CR_STATION_COUNT 50
extern const cr_station_t CR_STATIONS[CR_STATION_COUNT];

// 下一个 / 上一个台（线性循环，越界回绕）。
uint8_t cr_station_next(uint8_t index);
uint8_t cr_station_prev(uint8_t index);

// 分类名（"央广"/"华北"/…），越界返回空串。
const char *cr_station_cat_name(uint8_t cat);

// 索引合法性（范围与空指针防护：清单为空时任何索引都非法）。
bool cr_station_valid(uint8_t index);
