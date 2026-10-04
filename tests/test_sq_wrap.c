// tests/test_sq_wrap.c —— 中文折行：硬性每行字数、标点不落行首、容量失败，
// 以及终章剧情文案必须折得动（真机空白页教训）。
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "sq_wrap.h"

static void test_hard_line_width(void)
{
    char out[SQ_WRAP_CAPACITY];
    // 5 个字、每行 3 个：断点只能落在 3/2。
    const size_t n = sq_wrap_utf8("围棋一盘棋", 3, out, sizeof(out));
    assert(n > 0);
    assert(strcmp(out, "围棋一\n盘棋") == 0);
    assert(sq_utf8_length("围棋一盘棋") == 5);
    puts("ok  每行字数硬保证");
}

static void test_punctuation_never_leads(void)
{
    char out[SQ_WRAP_CAPACITY];
    // 「甲，乙。丙」每行 2 个字：断点收在标点后面，逗号句号都不落行首。
    assert(sq_wrap_utf8("甲，乙。丙", 2, out, sizeof(out)) > 0);
    assert(strcmp(out, "甲，\n乙。\n丙") == 0);
    // 连续标点：向前搜索退到 1 为止（头文件里说明过的边界），下一行从标点起。
    assert(sq_wrap_utf8("甲，。乙", 2, out, sizeof(out)) > 0);
    assert(strcmp(out, "甲\n，。\n乙") == 0);
    puts("ok  收尾标点不落行首（连续标点退到 1 为止）");
}

static void test_capacity_failure_is_total(void)
{
    char tiny[4];
    // 5 个字 x 3 字节肯定装不进 4 字节；失败必须整体失败，不留半截。
    assert(sq_wrap_utf8("围棋一盘棋", 3, tiny, sizeof(tiny)) == 0);
    assert(tiny[0] == '\0');
    assert(sq_wrap_utf8(NULL, 3, tiny, sizeof(tiny)) == 0);
    assert(sq_wrap_utf8("棋", 0, tiny, sizeof(tiny)) == 0);
    assert(sq_wrap_utf8("棋", SQ_WRAP_MAX_LINE + 1, tiny, sizeof(tiny)) == 0);
    puts("ok  容量不足整体失败，不留半截字符串");
}

static void test_ascii_passthrough(void)
{
    char out[SQ_WRAP_CAPACITY];
    assert(sq_wrap_utf8("12/15 80%", 20, out, sizeof(out)) == 9);
    assert(strcmp(out, "12/15 80%") == 0);
    assert(sq_utf8_length("12/15") == 5);
    puts("ok  ASCII 原样通过");
}

static void test_long_copy_wraps(void)
{
    // 结算/说明类文案可能加长：一段接近缓冲容量的长中文必须整体折行成功，
    // 且每行字数是硬保证（第一行满 13 字）。折行失败按设计写空串 ——
    // 界面上就是空白，这里在宿主机拦住。
    const char *long_copy =
        "中国跳棋的走法：棋子每次走一格，或者跳过相邻的棋子落到其正后方的空"
        "位，跳跃可以连续进行、可以转弯，也可以随时停下；先把全部棋子送进对"
        "面营地的一方获胜。棋子进入目标营地后不得再离开，离开出发营地后也不"
        "得回去。";
    char out[SQ_WRAP_CAPACITY];
    const size_t n = sq_wrap_utf8(long_copy, 13, out, sizeof(out));
    assert(n > 0);
        const char *first_break = strchr(out, 10); // 10 = LF
assert((int)(first_break - out) == 13 * 3); // 第一行恰好 13 个汉字
    puts("ok  长文案整体折行成功且首行字数硬保证");
}

int main(void)
{
    test_hard_line_width();
    test_punctuation_never_leads();
    test_capacity_failure_is_total();
    test_ascii_passthrough();
    test_long_copy_wraps();
    puts("PASS test_sq_wrap");
    return 0;
}
