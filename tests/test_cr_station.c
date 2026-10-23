#include <assert.h>
#include <string.h>

#include "cr_station.h"

// 台名显示宽度（单位 = 半角）：全角 2、半角 1。24px 字库下 210px 行宽
// 最多 17 个单位（17×12=204 ≤ 210）。新台名超预算会在这里被钉住。
static int display_units(const char *s)
{
    int units = 0;
    while (*s) {
        if ((unsigned char)*s >= 0x80) {
            units += 2; // UTF-8 全角（台单只允许中日文与 ASCII）
            s += 3;
        } else {
            units += 1;
            s += 1;
        }
    }
    return units;
}

int main(void)
{
    // 清单规模与编号稳定（26 台是交付说明里写死的数字）。
    assert(CR_STATION_COUNT == 50);
    _Static_assert(CR_CAT_COUNT == 8, "分类数与界面文案一致");

    // 每一台：名字非空、宽度在预算内、URL 必须纯 http（设备端 raw socket
    // 无 TLS —— https 台一律不许进清单）、分类合法。
    for (uint8_t i = 0; i < CR_STATION_COUNT; i++) {
        const cr_station_t *s = &CR_STATIONS[i];
        assert(s->name && s->name[0]);
        assert(display_units(s->name) <= 16);
        assert(s->url);
        assert(strncmp(s->url, "http://", 7) == 0);
        // radio.cn HLS：m3u8 直链
        assert(strstr(s->url, ".m3u8"));
        assert(s->cat < CR_CAT_COUNT);
        assert(cr_station_valid(i));
    }
    assert(!cr_station_valid(CR_STATION_COUNT));
    assert(!cr_station_valid(255));

    // 台名不重复（换台时用户靠名字辨台）。
    for (uint8_t i = 0; i < CR_STATION_COUNT; i++) {
        for (uint8_t j = (uint8_t)(i + 1); j < CR_STATION_COUNT; j++) {
            assert(strcmp(CR_STATIONS[i].name, CR_STATIONS[j].name) != 0);
        }
    }

    // 线性循环导航（家族真机验证过的形态）：首尾相接、步进恒为 1。
    assert(cr_station_next(0) == 1);
    assert(cr_station_next(49) == 0);
    assert(cr_station_next(50) == 0); // 越界回 0
    assert(cr_station_prev(0) == 49);
    assert(cr_station_prev(1) == 0);
    assert(cr_station_prev(50) == 49); // 越界从尾接

    // 全部台名都有分类文案。
    for (uint8_t c = 0; c < CR_CAT_COUNT; c++) {
        const char *name = cr_station_cat_name(c);
        assert(name && name[0]);
    }
    assert(cr_station_cat_name(CR_CAT_COUNT)[0] == '\0');

    return 0;
}
