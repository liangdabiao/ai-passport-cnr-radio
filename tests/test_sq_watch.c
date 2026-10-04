// tests/test_sq_watch.c —— 自选名单：预设、增删、去重（同 code 两域并存）、
// 容量上限、序列化回读。
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "sq_watch.h"

static void test_defaults(void)
{
    sq_watchlist_t list;
    sq_watch_reset(&list);
    assert(list.count == 2);
    assert(strcmp(list.entries[0].code, "000001") == 0 && list.entries[0].is_index);
    assert(strcmp(list.entries[1].code, "399001") == 0 && list.entries[1].is_index);

    // 预设 secid：指数走内置映射。
    char secid[SQ_SECID_MAX];
    assert(sq_watch_secid(&list, 0, secid, sizeof(secid)));
    assert(strcmp(secid, "1.000001") == 0); // 上证指数
    assert(sq_watch_secid(&list, 1, secid, sizeof(secid)));
    assert(strcmp(secid, "0.399001") == 0); // 深证成指

    // 预设表完整性与英文简称。
    assert(SQ_PRESET_COUNT >= 5);
    assert(strcmp(sq_watch_preset_name("000001", true), "SSE") == 0);
    assert(strcmp(sq_watch_preset_name("600519", false), "MOUTAI") == 0);
    assert(sq_watch_preset_name("600519", true) == NULL);
}

static void test_add_remove_cap(void)
{
    sq_watchlist_t list;
    sq_watch_reset(&list);

    assert(sq_watch_add(&list, "600519", false));
    assert(sq_watch_add(&list, "300750", false));
    // 重复（同 code 同域）拒绝。
    assert(!sq_watch_add(&list, "600519", false));
    // 000001 两域并存：指数已在，个股允许。
    assert(sq_watch_add(&list, "000001", false));
    assert(sq_watch_has(&list, "000001"));

    // 填满到 8。
    while (list.count < SQ_WATCH_CAP) {
        static char code[7];
        snprintf(code, sizeof(code), "60%04d", list.count * 111);
        assert(sq_watch_add(&list, code, false));
    }
    assert(list.count == SQ_WATCH_CAP);
    assert(!sq_watch_add(&list, "000002", false)); // 满员拒绝。

    // 删除。
    assert(sq_watch_remove(&list, "600519"));
    assert(!sq_watch_has(&list, "600519"));
    assert(!sq_watch_remove(&list, "600519")); // 再删不存在。
}

static void test_individual_needs_valid_code(void)
{
    sq_watchlist_t list;
    sq_watch_reset(&list);
    assert(!sq_watch_add(&list, "430047", false)); // 北交所拒绝
    assert(!sq_watch_add(&list, "ABCDEF", false)); // 非数字
}

static void test_serialize_roundtrip(void)
{
    sq_watchlist_t list;
    sq_watch_reset(&list);
    assert(sq_watch_add(&list, "600519", false));

    uint8_t blob[SQ_WATCH_BLOB_SIZE];
    assert(sq_watch_serialize(&list, blob, sizeof(blob)));

    sq_watchlist_t restored;
    assert(sq_watch_deserialize(&restored, blob, sizeof(blob)));
    assert(restored.count == list.count);
    for (uint8_t i = 0; i < list.count; i++) {
        assert(strcmp(restored.entries[i].code, list.entries[i].code) == 0);
        assert(restored.entries[i].is_index == list.entries[i].is_index);
    }
    // 坏 blob 拒绝。
    assert(!sq_watch_deserialize(&restored, blob, sizeof(blob) - 1));
    blob[1] = 'X';
    assert(!sq_watch_deserialize(&restored, blob, sizeof(blob)));
}

int main(void)
{
    test_defaults();
    test_add_remove_cap();
    test_individual_needs_valid_code();
    test_serialize_roundtrip();
    printf("test_sq_watch: PASS\n");
    return 0;
}
