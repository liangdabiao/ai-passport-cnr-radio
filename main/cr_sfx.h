// main/cr_sfx.h —— 界面音效：RTTTL 合成，电台播放时让路。
//
// 与家族其它应用（sq_sfx 七桥/股票同款架构）的差异只有两点：
//   1. 没有 BGM —— 电台本身就是声音；
//   2. 播放器可能把 codec 留在流的采样率上（44.1/48k），每道音效前先恢复
//      16kHz，杜绝「音调快一倍」的家族老坑（bsp_audio.h 头注的教训）。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

typedef enum {
    CR_TONE_MOVE = 0,  // 换台 / 移动
    CR_TONE_ENTER,     // 开始播放
    CR_TONE_BACK,      // 返回
    CR_TONE_DECLINED,  // 无效操作（未联网就按播放等）
    CR_TONE_COUNT,
} cr_tone_t;

// 启动音效服务（幂等）。失败时应用静音运行。
esp_err_t cr_sfx_start(void);

// 播一道音效。电台正在出声时整体静默（不打断节目）；音量档位 0 也静默。
void cr_sfx_play(cr_tone_t tone);

// 同步音量档位（与电台共用一个用户档）。
void cr_sfx_set_volume(uint8_t level);

void cr_sfx_stop(void);
