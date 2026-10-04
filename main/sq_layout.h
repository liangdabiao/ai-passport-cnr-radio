// main/sq_layout.h —— 版式几何、每行字数预算与配色。
//
// 刻意不依赖 LVGL：这些数字是「一屏放得下」的算术两端，界面（sq_ui）与宿主
// 测试必须引用**同一份**定义 —— 复制一份迟早走样。
#pragma once

// ---------------------------------------------------------------- 配色 -----
// 本应用自己的主题：墨黑行情终端底 + 金色强调 + **红涨绿跌**（A 股惯例，与
// 家族其他应用的语义刻意相反——这里红是好消息）。全部由十六进制值转成
// RGB565（R5 G6 B5），转换式 (r>>3)<<11 | (g>>2)<<5 | (b>>3) 记在这里。
#define SQ_C_SCREEN     0x1082   // 逻辑屏底：#101820 行情终端黑
#define SQ_C_PANEL      0x2104   // 页面卡片：#202830 深灰面板
#define SQ_C_PANEL_ALT  0x2965   // 次级面板：#28323c 行底（未选中）
#define SQ_C_INK        0xEF9D   // 正文：#eef2e8 亮灰白
#define SQ_C_MUTED      0x8C51   // 次级文字：#98a0a8 冷灰
#define SQ_C_GOLD       0xF609   // 金：#f0c34c 强调与当前选择
#define SQ_C_GOLD_D     0xFECF   // 亮金：#ffd97a 选中行里的文字
#define SQ_C_GOLD_S     0x49E3   // 金淡：#4a3d1a 选中行的底
#define SQ_C_LINE       0x3287   // 行边框：#33503f
#define SQ_C_HINTINK    0x8C51   // 底栏提示文字
#define SQ_C_UP         0xD0E4   // 涨（红）：#e03e28 —— A 股红涨
#define SQ_C_DOWN       0x2704   // 跌（绿）：#1f8440 —— A 股绿跌
#define SQ_C_FLAT       0x8C51   // 平盘/停牌：中性灰
#define SQ_C_GREEN      0x2704   // 语义绿 = 跌色（家族控件 GOOD 用）
#define SQ_C_RED        0xD0E4   // 语义红 = 涨色（家族控件 BAD 用）

// --------------------------------------------------------------- 版式 ------
// 与家族其它应用同一块屏（240x320，四角 30px 圆角遮罩），物理事实相同。
#define SQ_SCREEN_W     240
#define SQ_SCREEN_H     320
#define SQ_PAGE_INSET   5
#define SQ_PAGE_W       (SQ_SCREEN_W - 2 * SQ_PAGE_INSET)          // 230
#define SQ_PAGE_H       (SQ_SCREEN_H - 2 * SQ_PAGE_INSET)          // 310
#define SQ_PAGE_RADIUS  25
#define SQ_BAR_H        36
#define SQ_HINT_H       26
#define SQ_BODY_TOP     SQ_BAR_H                                   // 36
#define SQ_BODY_BOTTOM  (SQ_PAGE_H - SQ_HINT_H)                    // 284
#define SQ_BODY_H       (SQ_BODY_BOTTOM - SQ_BODY_TOP)             // 248
#define SQ_BODY_X       10
#define SQ_BODY_W       (SQ_PAGE_W - 2 * SQ_BODY_X)                // 210

// ------------------------------------------------------- 每行字数预算 -----
// 汉字等宽（Noto Sans CJK 全角字形，前进宽度 = 字号），每行几个字 = 除法取整：
// 210px 的正文区，32/24/16px 三档就是 6/8/13 个字。
#define SQ_CHARS_HEADLINE 6   // 32px
#define SQ_CHARS_BODY     8   // 24px
#define SQ_CHARS_SMALL    13  // 16px

// ----------------------------------------------- 一行的行高 ---------------
// Noto Sans CJK 在 16/24px 字号下的实际行高。所有「只允许一行」的标签
// （行、顶栏、提示栏）必须把高度钉成这个值：LVGL 的省略号截断（DOTS）只对
// **固定尺寸**的标签生效 —— 高度给了「按内容自适应」，超宽的文字就会折成
// 第二行再被行高裁掉（家族真机反馈里「两行、看不见」的根因）。
#define SQ_LINE_H_SMALL   20  // 16px 字库的行高
#define SQ_LINE_H_BODY    29  // 24px 字库的行高

// ------------------------------------------------------------ 行情列表 ----
// 行情行高：一行 44px（名称/代码一行 + 价格/涨跌一行），正文区放 5 行 +
// 滚动提示。自选上限 8 只，靠翻页浏览。
#define SQ_ROW_H         44
#define SQ_ROW_GAP       3
#define SQ_LIST_ROWS     5
