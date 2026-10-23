// main/cr_station.c —— 见 cr_station.h。
//
// 台源来源：radio.cn（央广网官方互联网电台平台）。清单由 tools 下的拉取
// 脚本生成，URL 带平台长效 key —— 平台换 key 时需要重新生成（拉取脚本用
// 官方接口签名，可随时重跑）。
#include "cr_station.h"

#include <stddef.h>

const cr_station_t CR_STATIONS[CR_STATION_COUNT] = {
    { "中国之声", "http://ytcast2.radio.cn/110/radios/30639/index_30639.m3u8?type=1&key=004dce06735138cb4ae8180ab82f8812&time=6adb35fc", CR_CAT_CNR },
    { "经济之声", "http://ytcast2.radio.cn/110/radios/30640/index_30640.m3u8?type=1&key=a66bf7364bd2e87a48e64ac13487bce3&time=6adb35fc", CR_CAT_CNR },
    { "环球资讯广播", "http://ytcast2.radio.cn/110/radios/30692/index_30692.m3u8?type=1&key=d1f13fb032662a196fddc781ca773e0c&time=6adb35fc", CR_CAT_CNR },
    { "音乐之声", "http://ytcast2.radio.cn/110/radios/30641/index_30641.m3u8?type=1&key=e40f85c4ab7be5475f6f72e8c691da4a&time=6adb35fc", CR_CAT_CNR },
    { "交通广播", "http://ytcast2.radio.cn/110/radios/30653/index_30653.m3u8?type=1&key=93abe6af6f60a0f6b4d17dac3e0bb54b&time=6adb35fc", CR_CAT_CNR },
    { "文艺之声", "http://ytcast2.radio.cn/110/radios/30648/index_30648.m3u8?type=1&key=6a0c3e8b07d58893e71621eeef73bd82&time=6adb35fc", CR_CAT_CNR },
    { "大湾区之声", "http://ytcast2.radio.cn/110/radios/30645/index_30645.m3u8?type=1&key=c991ddc90eb427a5c770709b36eb62c9&time=6adb35fc", CR_CAT_CNR },
    { "经典音乐", "http://ytcast2.radio.cn/110/radios/30642/index_30642.m3u8?type=1&key=f973e46f84853df29aabd56ee5226299&time=6adb35fc", CR_CAT_CNR },
    { "台海之声", "http://ytcast2.radio.cn/110/radios/30643/index_30643.m3u8?type=1&key=5465c3cbbec7b76f93828ad969fc7c61&time=6adb35fc", CR_CAT_CNR },
    { "神州之声", "http://ytcast2.radio.cn/110/radios/30644/index_30644.m3u8?type=1&key=3b7c1e78f720a88fa5d1fc50da8d7e4d&time=6adb35fc", CR_CAT_CNR },
    { "香港之声", "http://ytcast2.radio.cn/110/radios/30646/index_30646.m3u8?type=1&key=fcf7233d5faa7d60fb625c7a6ca5d204&time=6adb35fc", CR_CAT_CNR },
    { "民族之声", "http://ytcast2.radio.cn/110/radios/30647/index_30647.m3u8?type=1&key=ff30e325a831c8170318c2deebef773c&time=6adb35fc", CR_CAT_CNR },
    { "老年之声", "http://ytcast2.radio.cn/110/radios/30649/index_30649.m3u8?type=1&key=3fb3c549423d3aaa9489b5233b2f216c&time=6adb35fc", CR_CAT_CNR },
    { "藏语广播", "http://ytcast2.radio.cn/110/radios/30650/index_30650.m3u8?type=1&key=8c2deb1c2883464d67f79085a3630700&time=6adb35fc", CR_CAT_CNR },
    { "维语广播", "http://ytcast2.radio.cn/110/radios/30651/index_30651.m3u8?type=1&key=707881074ef5e5a78b423020dad9fc44&time=6adb35fc", CR_CAT_CNR },
    { "乡村之声", "http://ytcast2.radio.cn/110/radios/30654/index_30654.m3u8?type=1&key=10b174a946f9bd88c13b84830582257a&time=6adb35fc", CR_CAT_CNR },
    { "哈语广播", "http://ytcast2.radio.cn/110/radios/30655/index_30655.m3u8?type=1&key=de5677eeb2acdee9e131f2d5ecd937d2&time=6adb35fc", CR_CAT_CNR },
    { "南海之声", "http://ytcast2.radio.cn/110/radios/30664/index_30664.m3u8?type=1&key=5e92492c09508cee781d052134d5952c&time=6adb35fc", CR_CAT_CNR },
    { "CGTN英语", "http://ytcast.radio.cn/128/radios/110734/index_110734.m3u8?type=1&key=d879b9826ef550cb02632751ae188fd3&time=6adb35fc", CR_CAT_CNR },
    { "北京城市广播", "http://ytcast.radio.cn/128/radios/110357/index_110357.m3u8?type=1&key=20c40b6eba2fd3f84da4168d41458ca9&time=6adb3601", CR_CAT_NORTH },
    { "北京交通广播", "http://ytcast.radio.cn/128/radios/110354/index_110354.m3u8?type=1&key=b5da5180c3d45ec1bc13b1c5831f699c&time=6adb3601", CR_CAT_NORTH },
    { "北京音乐广播", "http://ytcast.radio.cn/128/radios/110356/index_110356.m3u8?type=1&key=1ca946cf1a9b1763f41c6399317020e5&time=6adb3601", CR_CAT_NORTH },
    { "黑龙江爱家广播", "http://ytcast.radio.cn/129/radios/110558/index_110558.m3u8?type=1&key=664961dac13854e207d2ade2cc758ea2&time=6adb3614", CR_CAT_NE },
    { "黑龙江朝语广播", "http://ytcast.radio.cn/128/radios/110554/index_110554.m3u8?type=1&key=80b899134e295425f6d82f8e542d9c31&time=6adb3614", CR_CAT_NE },
    { "吉林经济广播", "http://ytcast.radio.cn/128/radios/110545/index_110545.m3u8?type=1&key=6e2b325963d5eaacec7c3ab19c811c88&time=6adb361a", CR_CAT_NE },
    { "安徽经济广播", "http://ytcast.radio.cn/129/radios/110600/index_110600.m3u8?type=1&key=9beaa7ed05699ff9845e8894ce5b514e&time=6adb35ff", CR_CAT_EAST },
    { "江西财经广播", "http://ytcast.radio.cn/129/radios/110507/index_110507.m3u8?type=1&key=0d27363525f6fc141dc466c5d239afe9&time=6adb361f", CR_CAT_EAST },
    { "中国交通广播", "http://ytcast.radio.cn/129/radios/110744/index_110744.m3u8?type=1&key=45a921c09e8c84475619fc2cc1242414&time=6adb40c1", CR_CAT_EAST },
    { "第一财经广播", "http://ytcast.radio.cn/129/radios/110370/index_110370.m3u8?type=1&key=8e02c98d6c29468fcfcbe71fb30b46c9&time=6adb40c1", CR_CAT_EAST },
    { "新疆兵团综合广播", "http://ytcast.radio.cn/130/radios/110738/index_110738.m3u8?type=1&key=b082608992e5f04fde8295679b9aad29&time=6adb40c9", CR_CAT_EAST },
    { "河南经济广播", "http://ytcast.radio.cn/129/radios/110392/index_110392.m3u8?type=1&key=70ac81aedca2c5e937f85a9abc4dba5d&time=6adb3612", CR_CAT_CENTRAL },
    { "湖南电台潇湘之声", "http://ytcast.radio.cn/129/radios/110526/index_110526.m3u8?type=1&key=b768344822de03217d9e3ea65065eb6d&time=6adb3618", CR_CAT_CENTRAL },
    { "湖南经济广播", "http://ytcast.radio.cn/129/radios/111056/index_111056.m3u8?type=1&key=9bf62b6805f119be0996850db1c090ac&time=6adb3618", CR_CAT_CENTRAL },
    { "广东城市之声", "http://ytcast.radio.cn/130/radios/110491/index_110491.m3u8?type=1&key=620be7bbef0b2b3e68bc4f2db562a732&time=6adb3607", CR_CAT_SOUTH },
    { "广东股市广播", "http://ytcast.radio.cn/129/radios/110488/index_110488.m3u8?type=1&key=1002c236ac83f27a3281f4b94bfadae8&time=6adb3607", CR_CAT_SOUTH },
    { "广西北部湾之声", "http://ytcast.radio.cn/130/radios/110589/index_110589.m3u8?type=1&key=9eb0179f6e0d5fb983f2cc52f32a3f7a&time=6adb360a", CR_CAT_SOUTH },
    { "广西经济广播", "http://ytcast.radio.cn/130/radios/110587/index_110587.m3u8?type=1&key=774c65b01dfbf91209d6598d2310790a&time=6adb360a", CR_CAT_SOUTH },
    { "海南交通广播", "http://ytcast.radio.cn/130/radios/110542/index_110542.m3u8?type=1&key=5fd5ba1ba54ddf19531ac3c865b67d01&time=6adb360d", CR_CAT_SOUTH },
    { "海南新闻广播", "http://ytcast.radio.cn/130/radios/110541/index_110541.m3u8?type=1&key=b68c743b865dc2bd08e1fbd244f6f1d8&time=6adb360d", CR_CAT_SOUTH },
    { "贵州音乐广播", "http://ytcast.radio.cn/130/radios/110435/index_110435.m3u8?type=1&key=9fef91ac77b5cc570664569854b281c8&time=6adb360c", CR_CAT_SW },
    { "四川城市之音", "http://ytcast.radio.cn/130/radios/110450/index_110450.m3u8?type=1&key=cec9c293c275955b9ea8c73e19e89519&time=6adb40c3", CR_CAT_SW },
    { "西藏汉语广播", "http://ytcast.radio.cn/130/radios/110628/index_110628.m3u8?type=1&key=d48d0c89d8bce92f775a14afa0846f29&time=6adb40c5", CR_CAT_SW },
    { "云南国际广播", "http://ytcast.radio.cn/130/radios/110597/index_110597.m3u8?type=1&key=19d49519a62f3e0c573c0f39bf96fd5c&time=6adb40cc", CR_CAT_SW },
    { "云南民族广播", "http://ytcast.radio.cn/130/radios/110596/index_110596.m3u8?type=1&key=44ab5854a7065b2907ff5e45e28a5ea4&time=6adb40cc", CR_CAT_SW },
    { "甘肃交通广播", "http://ytcast.radio.cn/130/radios/110483/index_110483.m3u8?type=1&key=944160fadece9100bf55f839a64a20a6&time=6adb3606", CR_CAT_NW },
    { "甘肃农村广播", "http://ytcast.radio.cn/130/radios/110486/index_110486.m3u8?type=1&key=e01a2da05ef4bf3c7a512c9d09230379&time=6adb3606", CR_CAT_NW },
    { "宁夏新闻广播", "http://ytcast.radio.cn/130/radios/110469/index_110469.m3u8?type=1&key=855aeaa572ef095792082dcaee90ef7c&time=6adb3625", CR_CAT_NW },
    { "青海经济广播", "http://ytcast.radio.cn/130/radios/110617/index_110617.m3u8?type=1&key=2d9a8abe12d0d57544032d08d69bb0a1&time=6adb3626", CR_CAT_NW },
    { "青海交通音乐广播", "http://ytcast.radio.cn/130/radios/110616/index_110616.m3u8?type=1&key=3918274c131d79b9a7feebb184976d3b&time=6adb3626", CR_CAT_NW },
    { "新疆哈萨克语", "http://ytcast.radio.cn/130/radios/110620/index_110620.m3u8?type=1&key=634333b82b32241e0ef0c466c01e510f&time=6adb40c7", CR_CAT_NW },
};

_Static_assert(sizeof(CR_STATIONS) / sizeof(CR_STATIONS[0]) == CR_STATION_COUNT,
               "清单数量与头文件钉死的 CR_STATION_COUNT 不一致");

uint8_t cr_station_next(const uint8_t index)
{
    if (CR_STATION_COUNT == 0) return 0;
    if (index >= CR_STATION_COUNT - 1u) return 0;
    return (uint8_t)(index + 1u);
}

uint8_t cr_station_prev(const uint8_t index)
{
    if (CR_STATION_COUNT == 0) return 0;
    if (index == 0) return (uint8_t)(CR_STATION_COUNT - 1u);
    return (uint8_t)(index - 1u);
}

const char *cr_station_cat_name(const uint8_t cat)
{
    static const char *const NAMES[CR_CAT_COUNT] = {
        [CR_CAT_CNR]     = "央广",
        [CR_CAT_NORTH]   = "华北",
        [CR_CAT_NE]      = "东北",
        [CR_CAT_EAST]    = "华东",
        [CR_CAT_CENTRAL] = "华中",
        [CR_CAT_SOUTH]   = "华南",
        [CR_CAT_SW]      = "西南",
        [CR_CAT_NW]      = "西北",
    };
    if (cat >= CR_CAT_COUNT) return "";
    return NAMES[cat];
}

bool cr_station_valid(const uint8_t index)
{
    return CR_STATION_COUNT > 0 && index < CR_STATION_COUNT;
}
