#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "nr_volume.h"

int main(void)
{
    // 出厂默认必须落在格点上且不是满量程（用户还能往上调）。
    _Static_assert(NR_VOLUME_DEFAULT % NR_VOLUME_STEP == 0, "default on grid");
    _Static_assert(NR_VOLUME_BASE_SFX == 58, "sfx base pinned (nr_sfx.c 同值)");

    // 加档：到顶夹住，不回绕。
    assert(nr_volume_up(0) == 10);
    assert(nr_volume_up(90) == 100);
    assert(nr_volume_up(100) == 100);

    // 减档：到底夹住。
    assert(nr_volume_down(100) == 90);
    assert(nr_volume_down(10) == 0);
    assert(nr_volume_down(0) == 0);

    // 越界输入按关处理（回到可调范围）。
    assert(nr_volume_up(200) == 10);
    assert(nr_volume_down(200) == 0);

    // 缩放：32 位路径 + 截断语义。
    assert(nr_volume_scale(100, 60) == 60);
    assert(nr_volume_scale(58, 100) == 58);
    assert(nr_volume_scale(58, 0) == 0);
    assert(nr_volume_scale(58, 50) == 29);

    // 文本：关 / 百分比 / 容量不足整体失败。
    char buf[8];
    assert(nr_volume_text(0, buf, sizeof(buf)) == 3);
    assert(strcmp(buf, "关") == 0);
    assert(nr_volume_text(60, buf, sizeof(buf)) == 3);
    assert(strcmp(buf, "60%") == 0);
    assert(nr_volume_text(100, buf, 4) == 0);
    assert(buf[0] == '\0');
    assert(nr_volume_text(60, NULL, sizeof(buf)) == 0);

    // 全档位可达：从 0 一路加到 100 再减回 0，档距恒为 10。
    uint8_t level = 0;
    for (int i = 0; i < 10; i++) level = nr_volume_up(level);
    assert(level == 100);
    for (int i = 0; i < 10; i++) level = nr_volume_down(level);
    assert(level == 0);

    printf("test_nr_volume OK\n");
    return 0;
}
