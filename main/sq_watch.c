// main/sq_watch.c —— 见 sq_watch.h。
//
// 显示名用 ASCII 简称（英文方案：字库只需 UI 子集）。指数的东财 secid：
//   上证指数 1.000001、深证成指 0.399001、创业板指 0.399006、沪深300 1.000300。
#include "sq_watch.h"

#include <string.h>

// 内置预设：三大指数 + 常见宽基与热门个股样本。
const sq_preset_t SQ_PRESETS[] = {
    { "000001", "SSE", true },
    { "399001", "SZSE", true },
    { "399006", "ChiNext", true },
    { "000300", "CSI300", true },
    { "600519", "MOUTAI", false },
    { "000001", "PINGAN", false },
    { "300750", "CATL", false },
};
const uint8_t SQ_PRESET_COUNT = (uint8_t)(sizeof(SQ_PRESETS) / sizeof(SQ_PRESETS[0]));

// 指数代码 → 东财 secid（查预设映射；未收录返回 false）。
static bool index_secid(const char *code, char *out, unsigned capacity)
{
    static const char *const INDEX_SECIDS[][2] = {
        { "000001", "1.000001" }, // 上证指数
        { "399001", "0.399001" }, // 深证成指
        { "399006", "0.399006" }, // 创业板指
        { "000300", "1.000300" }, // 沪深300
    };
    for (unsigned m = 0; m < sizeof(INDEX_SECIDS) / sizeof(INDEX_SECIDS[0]); m++) {
        if (strcmp(INDEX_SECIDS[m][0], code) == 0) {
            if (strlen(INDEX_SECIDS[m][1]) + 1 > capacity) return false;
            strcpy(out, INDEX_SECIDS[m][1]);
            return true;
        }
    }
    return false;
}

void sq_watch_reset(sq_watchlist_t *list)
{
    memset(list, 0, sizeof(*list));
    // 出厂默认自选：上证指数 + 深证成指 —— 开箱就有内容可看。
    list->count = 2;
    strcpy(list->entries[0].code, "000001");
    list->entries[0].is_index = true;
    strcpy(list->entries[1].code, "399001");
    list->entries[1].is_index = true;
}

bool sq_watch_add(sq_watchlist_t *list, const char *code, bool is_index)
{
    if (!list || !code || strlen(code) != SQ_CODE_LEN) return false;
    // 同 code 同域才算重复：000001 在指数域（上证）与个股域（平安）可并存。
    for (uint8_t i = 0; i < list->count; i++) {
        if (strcmp(list->entries[i].code, code) == 0 &&
            list->entries[i].is_index == is_index) {
            return false;
        }
    }
    if (list->count >= SQ_WATCH_CAP) return false;
    if (!is_index && !sq_symbol_valid(code)) return false; // 个股须过市场校验

    sq_watch_entry_t *e = &list->entries[list->count];
    memset(e, 0, sizeof(*e));
    memcpy(e->code, code, SQ_CODE_LEN);
    e->is_index = is_index;
    list->count++;
    return true;
}

bool sq_watch_remove(sq_watchlist_t *list, const char *code)
{
    if (!list || !code) return false;
    for (uint8_t i = 0; i < list->count; i++) {
        if (strcmp(list->entries[i].code, code) != 0) continue;
        for (uint8_t k = (uint8_t)(i + 1); k < list->count; k++) {
            list->entries[k - 1] = list->entries[k];
        }
        list->count--;
        memset(&list->entries[list->count], 0, sizeof(list->entries[list->count]));
        return true;
    }
    return false;
}

bool sq_watch_has(const sq_watchlist_t *list, const char *code)
{
    if (!list || !code) return false;
    for (uint8_t i = 0; i < list->count; i++) {
        if (strcmp(list->entries[i].code, code) == 0) return true;
    }
    return false;
}

bool sq_watch_secid(const sq_watchlist_t *list, uint8_t index,
                    char *out, unsigned capacity)
{
    if (!list || index >= list->count) return false;
    const sq_watch_entry_t *e = &list->entries[index];
    if (e->is_index) return index_secid(e->code, out, capacity);
    return sq_symbol_secid(e->code, out, capacity);
}

bool sq_watch_serialize(const sq_watchlist_t *list, uint8_t *out, unsigned capacity)
{
    if (!list || !out || capacity < SQ_WATCH_BLOB_SIZE) return false;
    memset(out, 0, capacity);
    out[0] = list->count;
    for (uint8_t i = 0; i < list->count; i++) {
        uint8_t *slot = &out[1 + (unsigned)i * 8];
        memcpy(slot, list->entries[i].code, SQ_CODE_LEN);
        slot[7] = list->entries[i].is_index ? 1 : 0;
    }
    return true;
}

bool sq_watch_deserialize(sq_watchlist_t *list, const uint8_t *blob, unsigned size)
{
    if (!list || !blob || size != SQ_WATCH_BLOB_SIZE) return false;
    sq_watchlist_t parsed;
    memset(&parsed, 0, sizeof(parsed));
    if (blob[0] == 0 || blob[0] > SQ_WATCH_CAP) return false;
    for (uint8_t i = 0; i < blob[0]; i++) {
        const uint8_t *slot = &blob[1 + (unsigned)i * 8];
        for (unsigned k = 0; k < SQ_CODE_LEN; k++) {
            if (slot[k] < '0' || slot[k] > '9') return false;
        }
        memcpy(parsed.entries[i].code, slot, SQ_CODE_LEN);
        parsed.entries[i].code[SQ_CODE_LEN] = '\0';
        parsed.entries[i].is_index = slot[7] != 0;
        parsed.count++;
    }
    *list = parsed;
    return true;
}

const char *sq_watch_preset_name(const char *code, bool is_index)
{
    if (!code) return NULL;
    for (uint8_t i = 0; i < SQ_PRESET_COUNT; i++) {
        if (SQ_PRESETS[i].is_index == is_index &&
            strcmp(SQ_PRESETS[i].code, code) == 0) {
            return SQ_PRESETS[i].name;
        }
    }
    return NULL;
}
