// tests/test_sq_symbol.c —— 代码校验与市场推断。
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "sq_symbol.h"

int main(void)
{
    // 沪：6/9/5。
    assert(sq_symbol_valid("600519"));
    assert(sq_symbol_valid("688018"));
    assert(sq_symbol_valid("900901"));
    char secid[SQ_SECID_MAX];
    assert(sq_symbol_secid("600519", secid, sizeof(secid)));
    assert(strcmp(secid, "1.600519") == 0);

    // 深：0/2/3。
    assert(sq_symbol_valid("000001"));
    assert(sq_symbol_valid("300750"));
    assert(sq_symbol_secid("000001", secid, sizeof(secid)));
    assert(strcmp(secid, "0.000001") == 0);

    // 北交所明确拒绝。
    assert(!sq_symbol_valid("430047"));
    assert(!sq_symbol_valid("830799"));

    // 非法输入。
    assert(!sq_symbol_valid("60051"));   // 5 位
    assert(!sq_symbol_valid("6005190")); // 7 位
    assert(!sq_symbol_valid("60051A"));  // 非数字
    assert(!sq_symbol_valid(""));
    assert(!sq_symbol_valid(NULL));

    // 市场名。
    assert(strcmp(sq_symbol_market_name("600519"), "沪") == 0);
    assert(strcmp(sq_symbol_market_name("000001"), "深") == 0);
    assert(strcmp(sq_symbol_market_name("430047"), "") == 0);

    printf("test_sq_symbol: PASS\n");
    return 0;
}
