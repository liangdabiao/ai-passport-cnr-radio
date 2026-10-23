// main/nr_station.c —— 见 nr_station.h。
//
// 台源备注（2026-10-23 实测）：
//   lhttp.qingting.fm / lhttp.qtfm.cn —— 蜻蜓 FM 直播 CDN（上海/华为双入口，
//     同一台两个域名都可用；纯 http GET 可用，HEAD 会被 400 拒掉）。
//   m.tyradio.uk —— 听云系列（相声/评书专线，128kbps）。
//   ice*.somafm.com 等海外台 —— 直连可达，128kbps MP3。
#include "nr_station.h"

#include <stddef.h>

const nr_station_t NR_STATIONS[] = {
    // ---- 新闻 ----
    { "中国之声", "http://lhttp.qtfm.cn/live/15318317/64k.mp3",               NR_CAT_NEWS },
    { "北京新闻", "http://lhttp.qtfm.cn/live/339/64k.mp3",                    NR_CAT_NEWS },
    { "上海新闻", "http://lhttp.qingting.fm/live/270/64k.mp3",                NR_CAT_NEWS },
    { "深圳新闻", "http://lhttp.qingting.fm/live/1270/64k.mp3",               NR_CAT_NEWS },
    { "第一财经", "http://lhttp.qingting.fm/live/276/64k.mp3",                NR_CAT_NEWS },
    { "RFI中文",  "http://rfienchinois64k.ice.infomaniak.ch//rfienchinois-64.mp3", NR_CAT_NEWS },
    // ---- 音乐 ----
    { "华语经典", "http://lhttp.qtfm.cn/live/5022308/64k.mp3",                NR_CAT_MUSIC },
    { "北京音乐", "http://lhttp.qtfm.cn/live/332/64k.mp3",                    NR_CAT_MUSIC },
    { "上海音乐", "http://lhttp.qingting.fm/live/273/64k.mp3",                NR_CAT_MUSIC },
    { "广东音乐", "http://lhttp.qtfm.cn/live/1260/64k.mp3",                   NR_CAT_MUSIC },
    { "广州金曲", "http://lhttp.qingting.fm/live/20192/64k.mp3",              NR_CAT_MUSIC },
    { "亚洲音乐", "http://lhttp.qingting.fm/live/4581/64k.mp3",               NR_CAT_MUSIC },
    // ---- 曲艺 ----
    { "相声台",   "http://m.tyradio.uk/tyxs.mp3",                             NR_CAT_QUYI },
    { "评书台",   "http://m.tyradio.uk/typs.mp3",                             NR_CAT_QUYI },
    { "衡水评书", "http://lhttp.qtfm.cn/live/5021940/64k.mp3",                NR_CAT_QUYI },
    { "小说评书", "http://lhttp.qtfm.cn/live/1951/64k.mp3",                   NR_CAT_QUYI },
    // ---- 故事 ----
    { "阿基米德", "http://lhttp.qtfm.cn/live/20500182/64k.mp3",               NR_CAT_STORY },
    { "济南故事", "http://lhttp.qtfm.cn/live/1672/64k.mp3",                   NR_CAT_STORY },
    { "合肥故事", "http://lhttp.qingting.fm/live/1961/64k.mp3",               NR_CAT_STORY },
    { "山西故事", "http://lhttp.qingting.fm/live/5022511/64k.mp3",            NR_CAT_STORY },
    { "江西故事", "http://lhttp.qtfm.cn/live/20500092/64k.mp3",               NR_CAT_STORY },
    // ---- 环球 ----
    { "Groove Salad", "http://ice6.somafm.com/groovesalad-128-mp3",           NR_CAT_WORLD },
    { "Secret Agent", "http://ice6.somafm.com/secretagent-128-mp3",           NR_CAT_WORLD },
    { "Underground 80s", "http://ice2.somafm.com/u80s-128-mp3",               NR_CAT_WORLD },
    { "101 Smooth Jazz", "http://jking.cdnstream1.com/b22139_128mp3",         NR_CAT_WORLD },
    { "977 Smooth Jazz", "http://29319.live.streamtheworld.com:3690/977_SMOOJAZZ_SC", NR_CAT_WORLD },
};

_Static_assert(sizeof(NR_STATIONS) / sizeof(NR_STATIONS[0]) == NR_STATION_COUNT,
               "清单数量与头文件钉死的 NR_STATION_COUNT 不一致");

uint8_t nr_station_next(const uint8_t index)
{
    if (NR_STATION_COUNT == 0) return 0;
    if (index >= NR_STATION_COUNT - 1u) return 0;
    return (uint8_t)(index + 1u);
}

uint8_t nr_station_prev(const uint8_t index)
{
    if (NR_STATION_COUNT == 0) return 0;
    if (index == 0) return (uint8_t)(NR_STATION_COUNT - 1u);
    return (uint8_t)(index - 1u);
}

const char *nr_station_cat_name(const uint8_t cat)
{
    static const char *const NAMES[NR_CAT_COUNT] = {
        [NR_CAT_NEWS]  = "新闻",
        [NR_CAT_MUSIC] = "音乐",
        [NR_CAT_QUYI]  = "曲艺",
        [NR_CAT_STORY] = "故事",
        [NR_CAT_WORLD] = "环球",
    };
    if (cat >= NR_CAT_COUNT) return "";
    return NAMES[cat];
}

bool nr_station_valid(const uint8_t index)
{
    return NR_STATION_COUNT > 0 && index < NR_STATION_COUNT;
}
