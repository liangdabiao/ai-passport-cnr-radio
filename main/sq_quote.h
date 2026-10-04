// main/sq_quote.h —— 行情快照与东财 JSON 解析（纯逻辑，cJSON）。
//
// 请求（由 sq_net 拼）：
//   http://push2.eastmoney.com/api/qt/ulist.np/get?fltt=2&invt=2
//        &fields=f2,f3,f4,f12,f13,f14,f15,f16,f18,f124&secids=1.600519,0.000001
// 响应（UTF-8 JSON，一次带回全部自选）：
//   {"data":{"total":2,"diff":[
//     {"f2":179.73,"f3":3.33,"f4":5.80,"f12":"688018","f13":1,
//      "f14":"华兴源创","f15":183.83,"f16":168.19,"f18":173.93,"f124":1747121676},
//     ...]}}
// 停牌等异常时数值字段会是字符串 "-" —— 解析器按「缺失」处理并置 valid=false
// 的对应位，绝不猜数。
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define SQ_BATCH_MAX 8   // 与自选上限一致

typedef struct {
    char code[7];         // "600519"
    int32_t price_x100;   // 现价 ×100
    int32_t pct_x100;     // 涨跌幅 % ×100（正 = 涨）
    int32_t chg_x100;     // 涨跌额 ×100
    int32_t high_x100;
    int32_t low_x100;
    int32_t open_x100;    // 今开
    int32_t prev_x100;    // 昨收
    bool valid;           // 本条是否拿到了有效数值
} sq_quote_item_t;

typedef struct {
    sq_quote_item_t items[SQ_BATCH_MAX];
    uint8_t count;        // 解析出的条数
    uint32_t epoch;       // 服务器行情时间（取 diff 里最大的 f124）
} sq_quote_batch_t;

// 解析 ulist.np/get 的 JSON 文本。成功返回 true 并填 out；JSON 结构不对
// （无 data.diff）返回 false。单条里缺数值字段只影响该条 valid，不整体失败。
bool sq_quote_parse(const char *json, sq_quote_batch_t *out);

// 涨跌色语义（A 股：红涨绿跌）：返回 +1 涨 / -1 跌 / 0 平或无效。
int sq_quote_direction(const sq_quote_item_t *item);

// 快照序列化（NVS 缓存用）：count(1) + epoch(4) + 每条 40 字节定长槽位。
unsigned sq_quote_serialize_size(void);
bool sq_quote_serialize(const sq_quote_batch_t *batch, uint8_t *out, unsigned capacity);
bool sq_quote_deserialize(const uint8_t *blob, unsigned size, sq_quote_batch_t *out);
