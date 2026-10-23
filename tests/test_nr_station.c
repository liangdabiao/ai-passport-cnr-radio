#include <assert.h>
#include <string.h>

#include "nr_station.h"

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
    assert(NR_STATION_COUNT == 26);
    _Static_assert(NR_CAT_COUNT == 5, "分类数与界面文案一致");

    // 每一台：名字非空、宽度在预算内、URL 必须纯 http（设备端 raw socket
    // 无 TLS —— https 台一律不许进清单）、分类合法。
    for (uint8_t i = 0; i < NR_STATION_COUNT; i++) {
        const nr_station_t *s = &NR_STATIONS[i];
        assert(s->name && s->name[0]);
        assert(display_units(s->name) <= 16);
        assert(s->url);
        assert(strncmp(s->url, "http://", 7) == 0);
        assert(s->cat < NR_CAT_COUNT);
        assert(nr_station_valid(i));
    }
    assert(!nr_station_valid(NR_STATION_COUNT));
    assert(!nr_station_valid(255));

    // 台名不重复（换台时用户靠名字辨台）。
    for (uint8_t i = 0; i < NR_STATION_COUNT; i++) {
        for (uint8_t j = (uint8_t)(i + 1); j < NR_STATION_COUNT; j++) {
            assert(strcmp(NR_STATIONS[i].name, NR_STATIONS[j].name) != 0);
        }
    }

    // 线性循环导航（家族真机验证过的形态）：首尾相接、步进恒为 1。
    assert(nr_station_next(0) == 1);
    assert(nr_station_next(25) == 0);
    assert(nr_station_next(26) == 0); // 越界回 0
    assert(nr_station_prev(0) == 25);
    assert(nr_station_prev(1) == 0);
    assert(nr_station_prev(26) == 25); // 越界从尾接

    // 全部台名都有分类文案。
    for (uint8_t c = 0; c < NR_CAT_COUNT; c++) {
        const char *name = nr_station_cat_name(c);
        assert(name && name[0]);
    }
    assert(nr_station_cat_name(NR_CAT_COUNT)[0] == '\0');

    return 0;
}
