// main/cr_sfx.c —— 见 cr_sfx.h。
#include "cr_sfx.h"

#include <stddef.h>

#include "bsp_audio.h"
#include "esp_err.h"

#include "cr_player.h"
#include "cr_volume.h"
#include "rtttl_player.h"

#define RTTTL_HZ 16000 // 与 rtttl_player.c 的 RTTTL_SAMPLE_RATE 一致

// 音效在用户音量 100% 时的 codec 音量基准；必须与 cr_volume.h 的
// CR_VOLUME_BASE_SFX 一致 —— 由 tests/test_cr_volume.c 钉住。
#define CR_SFX_BASE_VOLUME 58

// 音效旋律（RTTTL）。少而轻：移动嗒、开始叮、返回噔、拒绝噔噔。
static const char *const TONES[CR_TONE_COUNT] = {
    [CR_TONE_MOVE]     = "mv:d=4,o=5,b=500:c7",
    [CR_TONE_ENTER]    = "en:d=8,o=5,b=300:e",
    [CR_TONE_BACK]     = "bk:d=8,o=5,b=300:g,e",
    [CR_TONE_DECLINED] = "no:d=4,o=4,b=500:c",
};

static uint8_t s_level = CR_VOLUME_DEFAULT;

esp_err_t cr_sfx_start(void)
{
    return rtttl_player_start();
}

void cr_sfx_play(const cr_tone_t tone)
{
    if (tone >= CR_TONE_COUNT) return;
    if (s_level == CR_VOLUME_OFF) return;
    if (cr_player_is_streaming()) return; // 电台正在出声：不打断节目

    // 播放器可能把 codec 留在流采样率上；无条件恢复 16kHz（同格式重复调用
    // 是廉价的，见 bsp_audio.h）。
    (void)bsp_audio_set_format(RTTTL_HZ, 16, 1);
    bsp_audio_set_volume(cr_volume_scale(CR_SFX_BASE_VOLUME, s_level));
    (void)rtttl_player_play(TONES[tone]);
}

void cr_sfx_set_volume(const uint8_t level)
{
    s_level = level;
}

void cr_sfx_stop(void)
{
    rtttl_player_stop();
}
