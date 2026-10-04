// tests/test_sq_market.c —— 交易时段判定（北京时间边界）。
//
// 锚点：2026-10-05（周一）UTC 00:00 = 北京时间 08:00。
#include <assert.h>
#include <stdio.h>

#include "sq_market.h"

// 北京时间 y-m-d h:m → Unix 秒（从 2026-10-05 周一锚点推，或直接算）。
// Howard Hinnant days_from_civil：把「当作 UTC」的年月日时分转 Unix 秒，
// 无需 timegm（POSIX）。北京时间时刻 = 该值 - 8h。
static uint64_t civil_utc(int y, int mo, int d, int h, int mi)
{
    y -= mo <= 2;
    const long long era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (unsigned)(mo + (mo > 2 ? -3 : 9)) + 2) / 5 + (unsigned)d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    const long long days = era * 146097 + (long long)doe - 719468;
    return (uint64_t)(days * 86400LL + h * 3600LL + mi * 60LL);
}

static uint64_t cst(int y, int mo, int d, int h, int mi)
{
    // 北京时间 = UTC + 8h，所以同一钟面时刻的 Unix 秒 = UTC 面值 - 8h。
    return civil_utc(y, mo, d, h, mi) - 8 * 3600;
};


int main(void)
{
    const uint64_t monday = cst(2026, 10, 5, 0, 0); // 周一 00:00 北京时间

    // 盘前 / 开盘集合竞价 / 盘中 / 午休 / 尾盘 / 收盘后。
    assert(!sq_market_is_trading(monday + 9 * 3600 + 14 * 60));      // 09:14
    assert(sq_market_is_trading(monday + 9 * 3600 + 15 * 60));       // 09:15
    assert(sq_market_is_trading(monday + 10 * 3600));                // 10:00
    assert(sq_market_is_trading(monday + 11 * 3600 + 30 * 60));      // 11:30
    assert(!sq_market_is_trading(monday + 11 * 3600 + 36 * 60));     // 11:36 午休
    assert(!sq_market_is_trading(monday + 12 * 3600));               // 12:00 午休
    assert(sq_market_is_trading(monday + 13 * 3600));                // 13:00
    assert(sq_market_is_trading(monday + 15 * 3600 + 5 * 60));       // 15:05
    assert(!sq_market_is_trading(monday + 15 * 3600 + 6 * 60));      // 15:06

    // 周末必休。
    const uint64_t saturday = cst(2026, 10, 3, 10, 0);
    const uint64_t sunday = cst(2026, 10, 4, 10, 0);
    assert(sq_market_is_weekend(saturday));
    assert(sq_market_is_weekend(sunday));
    assert(!sq_market_is_trading(saturday));
    assert(!sq_market_is_trading(sunday));

    // 工作日午外时段。
    assert(!sq_market_is_trading(monday + 8 * 3600));
    assert(!sq_market_is_trading(monday + 16 * 3600));

    printf("test_sq_market: PASS\n");
    return 0;
}
