// main/sq_sfx.c —— 见 sq_sfx.h。
#include "sq_sfx.h"

#include <stddef.h>

#include "bsp_audio.h"
#include "esp_err.h"
#include "esp_timer.h"

#include "sq_volume.h"
#include "rtttl_player.h"

// 音效旋律。RTTTL 格式：名字:默认(d=时长,o=八度,b=速度):音符序列。
// 行情工具音效少而轻：移动嗒、确认叮、拒绝噔、刷新成功上行两音。
// 每道音效标注近似时长（毫秒），仲裁器用它推算何时续播。
static const struct {
    const char *song;
    uint32_t ms;
} TONES[SQ_TONE_COUNT] = {
    [SQ_TONE_MOVE]     = { "mv:d=4,o=5,b=500:c7", 70 },
    [SQ_TONE_PICK]     = { "pk:d=8,o=6,b=400:e", 125 },
    [SQ_TONE_PLACE]    = { "pl:d=16,o=4,b=500:g", 90 },
    [SQ_TONE_HOP]      = { "hp:d=8,o=5,b=400:c,e", 250 },
    [SQ_TONE_AI_PLACE] = { "ap:d=16,o=4,b=500:f#", 90 },
    [SQ_TONE_UNDO]     = { "ud:d=8,o=5,b=300:e,c", 320 },
    [SQ_TONE_ENTER]    = { "en:d=8,o=5,b=300:e", 160 },
    [SQ_TONE_BACK]     = { "bk:d=8,o=5,b=300:g,e", 320 },
    [SQ_TONE_WIN]      = { "wn:d=8,o=5,b=200:c,e,g,c6,e6", 800 },
    [SQ_TONE_LOSE]     = { "ls:d=4,o=4,b=220:g,e", 400 },
    [SQ_TONE_DECLINED] = { "no:d=4,o=4,b=500:c", 80 },
};

// 背景乐循环曲。五声音阶（C 宫系），方波合成下不刺耳：
//   《漆盘》：菜单曲，b=84 慢速，约 24 秒一轮；
//   《跳枝》：对弈曲，b=112 稍快、跳跃动机，约 15 秒一轮。
static const char *const BGM_SONG[2] = {
    [SQ_BGM_MENU] = "astock:d=4,o=5,b=84:"
                    "8g,8a,4c6,8a,8g,4e,8g,8a,4g,8e,8d,4c,8d,8e,4g,8p,"
                    "8a,8c6,4d6,8c6,8a,4g,8e,8g,4a,8g,8e,4d,4c,4p",
    [SQ_BGM_PLAY] = "hangqing:d=4,o=5,b=112:"
                    "8c,8e,8g,4e,8g,8a,4g,8p,8e,8g,8c6,4a,8g,8e,4d,8p,"
                    "8d,8f,8a,4f,8a,8c6,4a,8p,8g,8e,8d,4e,8d,4c,4p",
};

// 音效结束后的歇息时间：盖过尾音，也给 BGM 留一点呼吸感。
#define BGM_HOLD_AFTER_SFX_MS 150
// 仲裁周期：BGM 被掐掉后最多这么久就会续上。
#define BGM_ARBITER_PERIOD_MS 300

// RTTTL 播放器出厂的 codec 音量（rtttl_player.c 的 RTTTL_VOLUME）。
// 音量档位以它为 100% 基准缩放；这个数必须与 sq_volume.h 的
// SQ_VOLUME_BASE_SFX 一致 —— 由 tests/test_sq_volume.c 钉住。
#define SQ_SFX_BASE_VOLUME 58

static uint8_t s_level = SQ_VOLUME_DEFAULT;
static bool s_bgm_on;
static sq_bgm_kind_t s_bgm_kind = SQ_BGM_MENU;
static esp_timer_handle_t s_arbiter;
static volatile int64_t s_hold_until_us;

static void arbiter(void *argument)
{
    (void)argument;
    if (!s_bgm_on || s_level == SQ_VOLUME_OFF) return;
    if (rtttl_player_is_playing()) return;
    if (esp_timer_get_time() < s_hold_until_us) return;
    (void)rtttl_player_play_loop(BGM_SONG[s_bgm_kind]);
}

void sq_sfx_play(sq_tone_t tone)
{
    if (tone >= SQ_TONE_COUNT) return;
    if (s_level == SQ_VOLUME_OFF) return;
    s_hold_until_us = esp_timer_get_time() +
                      (int64_t)(TONES[tone].ms + BGM_HOLD_AFTER_SFX_MS) * 1000;
    (void)rtttl_player_play(TONES[tone].song);
}

void sq_sfx_set_volume(uint8_t level)
{
    s_level = level;
    bsp_audio_set_volume(sq_volume_scale(SQ_SFX_BASE_VOLUME, level));
}

void sq_sfx_set_bgm(bool on)
{
    s_bgm_on = on;
    if (!on) {
        rtttl_player_stop();
        s_hold_until_us = 0;
    }
}

void sq_sfx_set_bgm_kind(sq_bgm_kind_t kind)
{
    if (kind == s_bgm_kind) return;
    s_bgm_kind = kind;
    rtttl_player_stop();
    s_hold_until_us = 0;
}

esp_err_t sq_sfx_start(void)
{
    const esp_err_t err = rtttl_player_start();
    if (err != ESP_OK) return err;
    if (s_arbiter) return ESP_OK;

    const esp_timer_create_args_t args = {
        .callback = &arbiter,
        .name = "sq_bgm",
    };
    if (esp_timer_create(&args, &s_arbiter) != ESP_OK) return ESP_ERR_NO_MEM;
    if (esp_timer_start_periodic(s_arbiter, BGM_ARBITER_PERIOD_MS * 1000) != ESP_OK) {
        esp_timer_delete(s_arbiter);
        s_arbiter = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void sq_sfx_stop(void)
{
    if (s_arbiter) {
        esp_timer_stop(s_arbiter);
        esp_timer_delete(s_arbiter);
        s_arbiter = NULL;
    }
    rtttl_player_stop();
}
