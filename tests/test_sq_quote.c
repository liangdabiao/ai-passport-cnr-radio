// tests/test_sq_quote.c —— 东财 JSON 解析：真实报文形状、停牌占位、坏 JSON、
// 序列化回读。
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "sq_quote.h"

// 按东财 ulist.np/get 的真实响应形状构造（fltt=2 时数值直接为小数）。
static const char *REAL_SHAPE =
    "{\"rc\":0,\"rt\":17,\"svr\":182354438,\"lt\":1,\"full\":1,"
    "\"data\":{\"total\":2,\"diff\":["
    "{\"f1\":2,\"f2\":179.73,\"f3\":3.33,\"f4\":5.80,\"f12\":\"688018\",\"f13\":1,"
    "\"f14\":\"华兴源创\",\"f15\":183.83,\"f16\":168.19,\"f17\":172.00,"
    "\"f18\":173.93,\"f124\":1747121676},"
    "{\"f1\":2,\"f2\":12.15,\"f3\":-1.22,\"f4\":-0.15,\"f12\":\"000001\",\"f13\":0,"
    "\"f14\":\"平安银行\",\"f15\":12.40,\"f16\":12.10,\"f17\":12.30,"
    "\"f18\":12.30,\"f124\":1747121676}]}}";

static const char *SUSPENDED_SHAPE =
    "{\"data\":{\"diff\":["
    "{\"f2\":\"-\",\"f3\":\"-\",\"f4\":\"-\",\"f12\":\"600519\",\"f13\":1,"
    "\"f14\":\"贵州茅台\",\"f15\":\"-\",\"f16\":\"-\",\"f17\":\"-\","
    "\"f18\":1700.00,\"f124\":1747121676}]}}";

static void test_parse_real_shape(void)
{
    sq_quote_batch_t batch;
    assert(sq_quote_parse(REAL_SHAPE, &batch));
    assert(batch.count == 2);
    assert(batch.epoch == 1747121676u);

    const sq_quote_item_t *a = &batch.items[0];
    assert(strcmp(a->code, "688018") == 0);
    assert(a->valid);
    assert(a->price_x100 == 17973);
    assert(a->pct_x100 == 333);
    assert(a->chg_x100 == 580);
    assert(a->high_x100 == 18383);
    assert(a->low_x100 == 16819);
    assert(a->open_x100 == 17200);
    assert(a->prev_x100 == 17393);
    assert(sq_quote_direction(a) == 1);

    const sq_quote_item_t *b = &batch.items[1];
    assert(strcmp(b->code, "000001") == 0);
    assert(b->pct_x100 == -122);
    assert(sq_quote_direction(b) == -1);
}

static void test_suspended_is_invalid_not_zero(void)
{
    sq_quote_batch_t batch;
    assert(sq_quote_parse(SUSPENDED_SHAPE, &batch));
    assert(batch.count == 1);
    // 停牌：数值字段是字符串 "-" —— 必须标记无效，绝不显示 0.00。
    assert(!batch.items[0].valid);
    assert(strcmp(batch.items[0].code, "600519") == 0);
    assert(sq_quote_direction(&batch.items[0]) == 0);
}

static void test_bad_input(void)
{
    sq_quote_batch_t batch;
    assert(!sq_quote_parse(NULL, &batch));
    assert(!sq_quote_parse("", &batch));
    assert(!sq_quote_parse("not json", &batch));
    assert(!sq_quote_parse("{\"data\":{\"diff\":{}}}", &batch)); // diff 非数组
    // 空数组：结构对但没有条目。
    assert(!sq_quote_parse("{\"data\":{\"diff\":[]}}", &batch));
}

static void test_serialize_roundtrip(void)
{
    sq_quote_batch_t batch;
    assert(sq_quote_parse(REAL_SHAPE, &batch));

    uint8_t blob[sq_quote_serialize_size()];
    assert(sq_quote_serialize(&batch, blob, sizeof(blob)));

    sq_quote_batch_t restored;
    assert(sq_quote_deserialize(blob, sizeof(blob), &restored));
    assert(restored.count == batch.count);
    assert(restored.epoch == batch.epoch);
    for (uint8_t i = 0; i < batch.count; i++) {
        assert(strcmp(restored.items[i].code, batch.items[i].code) == 0);
        assert(restored.items[i].price_x100 == batch.items[i].price_x100);
        assert(restored.items[i].pct_x100 == batch.items[i].pct_x100);
        assert(restored.items[i].valid == batch.items[i].valid);
    }
    // 尺寸不符的 blob 拒绝。
    assert(!sq_quote_deserialize(blob, sizeof(blob) - 1, &restored));
}

int main(void)
{
    test_parse_real_shape();
    test_suspended_is_invalid_not_zero();
    test_bad_input();
    test_serialize_roundtrip();
    printf("test_sq_quote: PASS\n");
    return 0;
}
