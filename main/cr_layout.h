// main/cr_layout.h —— 版式几何与复古收音机配色。
//
// 刻意不依赖 LVGL：这些数字是「一屏放得下」的算术两端，界面与宿主测试必须
// 引用**同一份**定义。屏与四角圆角遮罩是家族同一块物理屏（240x320）；
// 配色是本应用自己的「中国红」主题（深朱漆 + 宫墙红 + 米白刻字）—— 与
// 兄弟应用「网络电台」的琥珀木纹刻意区分（同一台设备上两个电台共存）。
#pragma once

// ---------------------------------------------------------------- 配色 -----
// 全部由十六进制转 RGB565（R5 G6 B5），转换式
// (r>>3)<<11 | (g>>2)<<5 | (b>>3) 记在这里。
#define CR_C_SCREEN    0x1081 // 逻辑屏底：#180c0a 深朱黑
#define CR_C_PANEL     0x08C2 // 面板：#26100e 深漆红
#define CR_C_PANEL_ALT 0x0AE2 // 次面板：#2f1712 亮一档的朱漆
#define CR_C_INK       0xF739 // 正文：#f5e6c8 米白刻字
#define CR_C_MUTED     0x9C4E // 次级文字：#9a8a72 旧纸灰
#define CR_C_AMBER     0x5A89 // 主强调：#e8543a 宫墙红（台名/指针/选中）
#define CR_C_AMBER_D   0x8B06 // 暗红：#8a3a2e（边框/刻度）
#define CR_C_POINTER   0xFBC7 // 调谐指针：#ff7a3c 亮橙（家族指针色）
#define CR_C_LINE      0x49A4 // 分隔线：#4a2a22
#define CR_C_ERR       0xEAE7 // 错误：#e85d3a
#define CR_C_OK        0x7DED // 正常/联网：#7fbf6a

// --------------------------------------------------------------- 版式 ------
// 与家族其它应用同一块屏（240x320，四角 30px 圆角遮罩），物理事实相同。
#define CR_SCREEN_W   240
#define CR_SCREEN_H   320
#define CR_PAGE_INSET 5
#define CR_PAGE_W     (CR_SCREEN_W - 2 * CR_PAGE_INSET) // 230
#define CR_PAGE_H     (CR_SCREEN_H - 2 * CR_PAGE_INSET) // 310
#define CR_PAGE_RADIUS 25
#define CR_BAR_H      32                                 // 顶栏
#define CR_HINT_H     26                                 // 底部提示栏
#define CR_BODY_W     (CR_PAGE_W - 40)                   // 190：内容主体宽

// 一行的行高（Noto Sans CJK 16/24px 实际行高；只允许一行的标签必须钉住，
// 否则 LVGL 的 DOTS 截断会失效 —— 家族真机教训）。
#define CR_LINE_H_SMALL 20 // 16px
#define CR_LINE_H_BODY  29 // 24px

// 16px 一行的全角字数预算（190px 内容宽 → 11 字；提示栏 218px → 13 字）。
#define CR_CHARS_BODY  11
#define CR_CHARS_HINT  13

// ------------------------------------------------------------ 刻度盘 -------
// 频率刻度盘面板（卡片内坐标）。刻度 88~108 MHz 一排，指针按台号均分。
#define CR_DIAL_X     20
#define CR_DIAL_Y     44
#define CR_DIAL_W     190
#define CR_DIAL_H     62
#define CR_DIAL_FREQ_LO 88
#define CR_DIAL_FREQ_HI 108
#define CR_DIAL_PAD   10 // 面板内左右留白（首末刻度到边的距离）

// --------------------------------------------------------- 收音机主页 ------
// 卡片内坐标：顶栏(0..32) → 刻度盘(44..106) → 分类/台名/状态 → 提示栏。
#define CR_RADIO_CAT_Y     116 // 分类（16px）
#define CR_RADIO_NAME_Y    138 // 台名（24px，高 29）
#define CR_RADIO_STATUS_Y  174 // 状态行（16px）
#define CR_RADIO_INFO_Y    196 // 流信息行（16px，128k · 44.1kHz）
#define CR_RADIO_VU_Y      232 // 播放指示条区顶
#define CR_RADIO_VU_H      40  // 指示条最大高度

// 音量浮层（屏幕中央偏下，常驻创建、按需显示）。
#define CR_VOL_W    150
#define CR_VOL_H    56
#define CR_VOL_X    ((CR_PAGE_W - CR_VOL_W) / 2)
#define CR_VOL_Y    128
#define CR_VOL_HIDE_MS 1600
