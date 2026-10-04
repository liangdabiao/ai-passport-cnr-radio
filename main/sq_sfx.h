// main/sq_sfx.h —— 界面音效 + 背景乐：全部 RTTTL 合成，不依赖音频素材。
//
// I2S 出口只有 rtttl_player 一个写入者。背景乐用 play_loop 循环，音效用
// play 抢断 —— 一道音效会打断 BGM，播完后由本模块的仲裁定时器在空闲间隙
// 把 BGM 从头续上（七桥同款架构）。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

// 音效名。跳与走音色不同，输赢各有情绪。
typedef enum {
    SQ_TONE_MOVE = 0,   // 光标 / 候选移动
    SQ_TONE_PICK,       // 选中己方棋子
    SQ_TONE_PLACE,      // 走一格
    SQ_TONE_HOP,        // 跳一跳（比走格清脆）
    SQ_TONE_AI_PLACE,   // AI 落子（低半度）
    SQ_TONE_UNDO,       // 悔棋
    SQ_TONE_ENTER,      // 确认、进入
    SQ_TONE_BACK,       // 返回
    SQ_TONE_WIN,        // 通关（上行琶音）
    SQ_TONE_LOSE,       // 输棋（下行）
    SQ_TONE_DECLINED,   // 无效操作
    SQ_TONE_COUNT,
} sq_tone_t;

// 背景乐曲目。菜单曲在标题 / 结算页循环，对弈曲在对局页循环。
typedef enum {
    SQ_BGM_MENU = 0,
    SQ_BGM_PLAY,
} sq_bgm_kind_t;

// 启动播放器与仲裁定时器。codec 不可用时返回错误，应用继续以静音方式运行。
esp_err_t sq_sfx_start(void);

// 停止任务与定时器（生命周期用）。调用后可以再次 start。
void sq_sfx_stop(void);

// 播放一道音效。音量为关（0 档）时静默丢弃。
void sq_sfx_play(sq_tone_t tone);

// 应用音量档位（0 = 关）。开机与切档时调用；内部换算成 codec 音量。
void sq_sfx_set_volume(uint8_t level);

// 背景乐开关（存档里的「音乐」设置）。关：立刻掐掉且不再续播。
void sq_sfx_set_bgm(bool on);

// 当前页面属于哪类场景，仲裁器据此选循环曲。切页时调用。
void sq_sfx_set_bgm_kind(sq_bgm_kind_t kind);
