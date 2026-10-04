// tests/test_sq_volume.c —— 音量档位：表、回绕、缩放算术、文本。
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "sq_volume.h"

static void test_steps_and_wrap(void)
{
    assert(SQ_VOLUME_STEP_COUNT == 6);
    assert(sq_volume_steps[0] == SQ_VOLUME_OFF);
    assert(sq_volume_steps[SQ_VOLUME_STEP_COUNT - 1] == SQ_VOLUME_MAX);
    // 到顶回绕到关；不认识的档位回关（而不是卡死在一个不存在的档位上）。
    assert(sq_volume_next(SQ_VOLUME_MAX) == SQ_VOLUME_OFF);
    assert(sq_volume_next(SQ_VOLUME_OFF) == 20);
    assert(sq_volume_next(37) == SQ_VOLUME_OFF);
    puts("ok  档位表与回绕");
}

static void test_scale(void)
{
    // 基准 58（与 sq_sfx.c 的 SQ_SFX_BASE_VOLUME 一致）：默认档 80% = 46。
    assert(SQ_VOLUME_BASE_SFX == 58);
    assert(sq_volume_scale(58, SQ_VOLUME_DEFAULT) == 46);
    assert(sq_volume_scale(58, SQ_VOLUME_OFF) == 0);
    assert(sq_volume_scale(58, SQ_VOLUME_MAX) == 58);
    puts("ok  缩放算术");
}

static void test_text(void)
{
    char text[16];
    assert(sq_volume_text(SQ_VOLUME_OFF, text, sizeof(text)) == 3);
    assert(strcmp(text, "关") == 0);
    assert(sq_volume_text(80, text, sizeof(text)) == 3);
    assert(strcmp(text, "80%") == 0);
    char tiny[2];
    assert(sq_volume_text(80, tiny, sizeof(tiny)) == 0);
    assert(tiny[0] == '\0');
    puts("ok  档位文本");
}

int main(void)
{
    test_steps_and_wrap();
    test_scale();
    test_text();
    puts("PASS test_sq_volume");
    return 0;
}
