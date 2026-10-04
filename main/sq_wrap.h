// main/sq_wrap.h —— 中文按字数折行。
//
// 为什么必须自己折：LVGL 的自动换行在空格处断行，而中文没有空格 —— 交给它就会
// 得到一行溢出的长文本。所以所有可能超一行的中文都要先经过这里。
//
// 断点选择规则：一行最多放 chars_per_line 个字（**这是硬保证**），并且在不超过
// 这个宽度的前提下，断点尽量靠后，但要求「下一行的第一个字不是收尾标点」。
// 也就是在候选断点 chars_per_line、chars_per_line-1、… 里向前找第一个满足条件
// 的位置（连续标点处的推导见家族仓库同名模块，这里的实现与围棋闯关同源）。
//
// 本应用的界面文案都是单行短句（提示行、状态行），折行主要服务于可能加长的
// 说明文字 —— 模块保留，测试钉住行为。
#pragma once

#include <stddef.h>

// 一行最多支持多少字。留到 32 是余量；超过这个值的请求会明确失败（返回 0），
// 而不是在栈上越界 —— 宁可失败得响亮。
#define SQ_WRAP_MAX_LINE 32

// 折行缓冲容量。最长的界面文案是终章剧情页：约 102 个汉字（305 字节），
// 16px 每行 13 字折成 8 行（7 个换行）+ NUL，共约 313 字节 —— 取 512 留足
// 余量。这个容量必须装得下 sq_page_story.c 里的每一段剧情：sq_ui.c 的
// 编译期守卫与 tests/test_sq_wrap.c 都盯不住文案本身，改剧情文案后要跑
// tools 之外的常识检查（剧情页留白会当场给你看）。
#define SQ_WRAP_CAPACITY 512

// 把 text 折成每行 chars_per_line 个字符，换行用 '\n'。
// 成功返回写入的字节数（不含结尾 NUL）；失败（容量不足 / 参数非法）返回 0，
// 并把 out 置为空串 —— 不留半截字符串。
size_t sq_wrap_utf8(const char *text, int chars_per_line, char *out, size_t capacity);

// UTF-8 字符串的字符数（码点数）。非法首字节按 1 字节计，只求不越界读。
unsigned int sq_utf8_length(const char *text);
