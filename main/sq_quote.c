// main/sq_quote.c —— 见 sq_quote.h。解析用 cJSON（设备上来自 IDF 的 json
// 组件，宿主测试编译 tests/cjson/ 里的同源单文件版）。数值固定 ×100 存整型
// —— 浮点只出现在解析瞬间，显示与比较全部走整数。
#include "sq_quote.h"

#include <string.h>

#include "cJSON.h"

// 数值字段提取：缺字段 / 字符串 "-"（停牌）/ 非 number 一律按 0 处理并置
// invalid —— 停牌股显示「—」而不是 0.00，由 valid 标志驱动。
static int32_t num_x100(const cJSON *obj, const char *key, bool *ok)
{
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!item || !cJSON_IsNumber(item)) {
        if (ok) *ok = false;
        return 0;
    }
    if (ok) *ok = true;
    return (int32_t)(item->valuedouble * 100.0 + (item->valuedouble < 0 ? -0.5 : 0.5));
}

static void fill_item(sq_quote_item_t *item, const cJSON *diff)
{
    memset(item, 0, sizeof(*item));

    const cJSON *code = cJSON_GetObjectItemCaseSensitive(diff, "f12");
    if (code && cJSON_IsString(code) && code->valuestring &&
        strlen(code->valuestring) < sizeof(item->code)) {
        strcpy(item->code, code->valuestring);
    }


    bool ok = false;
    item->price_x100 = num_x100(diff, "f2", &ok);
    bool ok2 = false;
    item->pct_x100 = num_x100(diff, "f3", &ok2);
    ok = ok && ok2;
    bool ok3 = false;
    item->chg_x100 = num_x100(diff, "f4", &ok3);
    ok = ok && ok3;
    bool ok4 = false;
    item->high_x100 = num_x100(diff, "f15", &ok4);
    bool ok5 = false;
    item->low_x100 = num_x100(diff, "f16", &ok5);
    bool ok6 = false;
    item->open_x100 = num_x100(diff, "f17", &ok6);
    bool ok7 = false;
    item->prev_x100 = num_x100(diff, "f18", &ok7);
    // 高低开缺失不致命；现价/涨跌幅/涨跌额齐全才算有效。
    item->valid = ok;
}

bool sq_quote_parse(const char *json, sq_quote_batch_t *out)
{
    memset(out, 0, sizeof(*out));
    if (!json) return false;

    cJSON *root = cJSON_Parse(json);
    if (!root) return false;
    bool result = false;

    const cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
    const cJSON *diff = data ? cJSON_GetObjectItemCaseSensitive(data, "diff") : NULL;
    if (cJSON_IsArray(diff)) {
        const cJSON *node = NULL;
        cJSON_ArrayForEach(node, diff) {
            if (out->count >= SQ_BATCH_MAX) break;
            if (!cJSON_IsObject(node)) continue;
            sq_quote_item_t *item = &out->items[out->count];
            fill_item(item, node);
            if (item->code[0] == '\0') continue; // 没有代码的条目直接丢弃。
            const cJSON *epoch = cJSON_GetObjectItemCaseSensitive(node, "f124");
            if (epoch && cJSON_IsNumber(epoch) &&
                (uint32_t)epoch->valuedouble > out->epoch) {
                out->epoch = (uint32_t)epoch->valuedouble;
            }
            out->count++;
            result = true;
        }
    }

    cJSON_Delete(root);
    return result;
}

int sq_quote_direction(const sq_quote_item_t *item)
{
    if (!item || !item->valid) return 0;
    if (item->pct_x100 > 0) return 1;
    if (item->pct_x100 < 0) return -1;
    return 0;
}

// ---------------------------------------------------------- 快照序列化 -----
// 定长二进制：count(1) + epoch(4) + 每条 40 字节（code 7 + pad 1 + 7×int32
// + valid 1）。NVS 缓存整存整取，断网时列表页回放。

unsigned sq_quote_serialize_size(void)
{
    return 5 + (unsigned)SQ_BATCH_MAX * 40;
}

bool sq_quote_serialize(const sq_quote_batch_t *batch, uint8_t *out, unsigned capacity)
{
    if (!batch || !out) return false;
    if (capacity < sq_quote_serialize_size()) return false;
    memset(out, 0, capacity);
    out[0] = batch->count;
    memcpy(&out[1], &batch->epoch, 4);
    for (uint8_t i = 0; i < batch->count; i++) {
        uint8_t *slot = &out[5 + (unsigned)i * 40];
        const sq_quote_item_t *item = &batch->items[i];
        memcpy(slot, item->code, 7);
        slot[7] = 0;
        memcpy(slot + 8, &item->price_x100, 4);
        memcpy(slot + 12, &item->pct_x100, 4);
        memcpy(slot + 16, &item->chg_x100, 4);
        memcpy(slot + 20, &item->high_x100, 4);
        memcpy(slot + 24, &item->low_x100, 4);
        memcpy(slot + 28, &item->open_x100, 4);
        memcpy(slot + 32, &item->prev_x100, 4);
        slot[36] = item->valid ? 1 : 0;
    }
    return true;
}

bool sq_quote_deserialize(const uint8_t *blob, unsigned size, sq_quote_batch_t *out)
{
    if (!blob || !out) return false;
    if (size != sq_quote_serialize_size()) return false;
    memset(out, 0, sizeof(*out));
    out->count = blob[0];
    if (out->count > SQ_BATCH_MAX) return false;
    memcpy(&out->epoch, &blob[1], 4);
    for (uint8_t i = 0; i < out->count; i++) {
        const uint8_t *slot = &blob[5 + (unsigned)i * 40];
        sq_quote_item_t *item = &out->items[i];
        memcpy(item->code, slot, 7);
        item->code[6] = '\0';
        memcpy(&item->price_x100, slot + 8, 4);
        memcpy(&item->pct_x100, slot + 12, 4);
        memcpy(&item->chg_x100, slot + 16, 4);
        memcpy(&item->high_x100, slot + 20, 4);
        memcpy(&item->low_x100, slot + 24, 4);
        memcpy(&item->open_x100, slot + 28, 4);
        memcpy(&item->prev_x100, slot + 32, 4);
        item->valid = slot[36] != 0;
    }
    return true;
}
