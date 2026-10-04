// main/sq_market.c —— 见 sq_market.h。
#include "sq_market.h"

#include <string.h>
#include <time.h>

#include <string.h>
#include <time.h>

// 北京时间 = UTC + 8 小时。
#define CST_OFFSET 8 * 3600

static void cst_breakdown(uint64_t epoch, struct tm *out)
{
    const time_t cst = (time_t)(epoch + CST_OFFSET);
    // gmtime 返回静态缓冲（POSIX 的 gmtime_r 不在 C11 里，host 与设备共用
    // 本文件）；本模块的调用方都在单任务上下文，无重入风险。
    const struct tm *gmt = gmtime(&cst);
    if (gmt) *out = *gmt;
    else memset(out, 0, sizeof(*out));
}

bool sq_market_is_weekend(uint64_t epoch)
{
    struct tm tm_cst;
    cst_breakdown(epoch, &tm_cst);
    return tm_cst.tm_wday == 0 || tm_cst.tm_wday == 6;
}

bool sq_market_is_trading(uint64_t epoch)
{
    if (sq_market_is_weekend(epoch)) return false;

    struct tm tm_cst;
    cst_breakdown(epoch, &tm_cst);
    const int minutes = tm_cst.tm_hour * 60 + tm_cst.tm_min;

    // 早盘含集合竞价（9:15 起），收盘后放宽到 15:05 收尾数据。
    if (minutes >= 9 * 60 + 15 && minutes <= 11 * 60 + 35) return true;
    if (minutes >= 12 * 60 + 55 && minutes <= 15 * 60 + 5) return true;
    return false;
}
