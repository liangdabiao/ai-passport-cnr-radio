// main/nr_sfx.c —— 见 nr_sfx.h。
#include "nr_sfx.h"

#include <stddef.h>

#include "bsp_audio.h"
#include "esp_err.h"

#include "nr_player.h"
#include "nr_volume.h"
#include "rtttl_player.h"

#define RTTTL_HZ 16000 // 与 rtttl_player.c 的 RTTTL_SAMPLE_RATE 一致

// 音效在用户音量 100% 时的 codec 音量基准；必须与 nr_volume.h 的
// NR_VOLUME_BASE_SFX 一致 —— 由 tests/test_nr_volume.c 钉住。
#define NR_SFX_BASE_VOLUME 58

// 音效旋律（RTTTL）。少而轻：移动嗒、开始叮、返回噔、拒绝噔噔。
static const char *const TONES[NR_TONE_COUNT] = {
    [NR_TONE_MOVE]     = "mv:d=4,o=5,b=500:c7",
    [NR_TONE_ENTER]    = "en:d=8,o=5,b=300:e",
    [NR_TONE_BACK]     = "bk:d=8,o=5,b=300:g,e",
    [NR_TONE_DECLINED] = "no:d=4,o=4,b=500:c",
};

static uint8_t s_level = NR_VOLUME_DEFAULT;

esp_err_t nr_sfx_start(void)
{
    return rtttl_player_start();
}

void nr_sfx_play(const nr_tone_t tone)
{
    if (tone >= NR_TONE_COUNT) return;
    if (s_level == NR_VOLUME_OFF) return;
    if (nr_player_is_streaming()) return; // 电台正在出声：不打断节目

    // 播放器可能把 codec 留在流采样率上；无条件恢复 16kHz（同格式重复调用
    // 是廉价的，见 bsp_audio.h）。
    (void)bsp_audio_set_format(RTTTL_HZ, 16, 1);
    bsp_audio_set_volume(nr_volume_scale(NR_SFX_BASE_VOLUME, s_level));
    (void)rtttl_player_play(TONES[tone]);
}

void nr_sfx_set_volume(const uint8_t level)
{
    s_level = level;
}

void nr_sfx_stop(void)
{
    rtttl_player_stop();
}
