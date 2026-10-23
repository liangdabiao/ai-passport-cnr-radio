// main/nr_layout.h —— 版式几何与复古收音机配色。
//
// 刻意不依赖 LVGL：这些数字是「一屏放得下」的算术两端，界面与宿主测试必须
// 引用**同一份**定义。屏与四角圆角遮罩是家族同一块物理屏（240x320）；
// 配色是本应用自己的复古收音机主题（深胡桃木 + 琥珀 + 米金刻字）。
#pragma once

// ---------------------------------------------------------------- 配色 -----
// 全部由十六进制转 RGB565（R5 G6 B5），转换式
// (r>>3)<<11 | (g>>2)<<5 | (b>>3) 记在这里。
#define NR_C_SCREEN    0x1081 // 逻辑屏底：#17100a 深胡桃黑
#define NR_C_PANEL     0x08C2 // 面板：#241811 深木色
#define NR_C_PANEL_ALT 0x0AE2 // 次面板：#2d1f14 亮一档的木色
#define NR_C_INK       0xF739 // 正文：#f5e6c8 米金刻字
#define NR_C_MUTED     0x9C4E // 次级文字：#9a8a72 旧铜灰
#define NR_C_AMBER     0xF589 // 主强调：#f2b04a 琥珀（台名/指针/选中）
#define NR_C_AMBER_D   0x8B06 // 暗琥珀：#8a6230（边框/刻度）
#define NR_C_POINTER   0xFBC7 // 调谐指针：#ff7a3c 亮橙
#define NR_C_LINE      0x49A4 // 分隔线：#4a3624
#define NR_C_ERR       0xEAE7 // 错误：#e85d3a
#define NR_C_OK        0x7DED // 正常/联网：#7fbf6a

// --------------------------------------------------------------- 版式 ------
// 与家族其它应用同一块屏（240x320，四角 30px 圆角遮罩），物理事实相同。
#define NR_SCREEN_W   240
#define NR_SCREEN_H   320
#define NR_PAGE_INSET 5
#define NR_PAGE_W     (NR_SCREEN_W - 2 * NR_PAGE_INSET) // 230
#define NR_PAGE_H     (NR_SCREEN_H - 2 * NR_PAGE_INSET) // 310
#define NR_PAGE_RADIUS 25
#define NR_BAR_H      32                                 // 顶栏
#define NR_HINT_H     26                                 // 底部提示栏
#define NR_BODY_W     (NR_PAGE_W - 40)                   // 190：内容主体宽

// 一行的行高（Noto Sans CJK 16/24px 实际行高；只允许一行的标签必须钉住，
// 否则 LVGL 的 DOTS 截断会失效 —— 家族真机教训）。
#define NR_LINE_H_SMALL 20 // 16px
#define NR_LINE_H_BODY  29 // 24px

// 16px 一行的全角字数预算（190px 内容宽 → 11 字；提示栏 218px → 13 字）。
#define NR_CHARS_BODY  11
#define NR_CHARS_HINT  13

// ------------------------------------------------------------ 刻度盘 -------
// 频率刻度盘面板（卡片内坐标）。刻度 88~108 MHz 一排，指针按台号均分。
#define NR_DIAL_X     20
#define NR_DIAL_Y     44
#define NR_DIAL_W     190
#define NR_DIAL_H     62
#define NR_DIAL_FREQ_LO 88
#define NR_DIAL_FREQ_HI 108
#define NR_DIAL_PAD   10 // 面板内左右留白（首末刻度到边的距离）

// --------------------------------------------------------- 收音机主页 ------
// 卡片内坐标：顶栏(0..32) → 刻度盘(44..106) → 分类/台名/状态 → 提示栏。
#define NR_RADIO_CAT_Y     116 // 分类（16px）
#define NR_RADIO_NAME_Y    138 // 台名（24px，高 29）
#define NR_RADIO_STATUS_Y  174 // 状态行（16px）
#define NR_RADIO_INFO_Y    196 // 流信息行（16px，128k · 44.1kHz）
#define NR_RADIO_VU_Y      232 // 播放指示条区顶
#define NR_RADIO_VU_H      40  // 指示条最大高度

// 音量浮层（屏幕中央偏下，常驻创建、按需显示）。
#define NR_VOL_W    150
#define NR_VOL_H    56
#define NR_VOL_X    ((NR_PAGE_W - NR_VOL_W) / 2)
#define NR_VOL_Y    128
#define NR_VOL_HIDE_MS 1600
