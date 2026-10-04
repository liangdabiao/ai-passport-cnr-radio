// main/sq_volume.h —— 音量档位与缩放。
//
// 不依赖 ESP-IDF 与 LVGL，所以宿主测试直接编译它。档位而不是「加/减」：这台
// 机器只有上/下/确定三个键，上下键在菜单里已经用来选行。档位由确定键循环。
// 0 档就是关，于是不需要单独的静音开关 —— 与家族各应用同一套决定。
#pragma once

#include <stddef.h>
#include <stdint.h>

// 档位表。0 是关，最大值是满量程。
#define SQ_VOLUME_STEP_COUNT 6
#define SQ_VOLUME_OFF 0
#define SQ_VOLUME_MAX 100

// 出厂默认档位。必须是档位表里的一个值 —— 见 .c 里的 _Static_assert。
#define SQ_VOLUME_DEFAULT 80

// 音效在**用户音量 100% 时**的编解码器音量基准。本应用只有一路音频（RTTTL
// 音效合成），基准值必须与 sq_sfx.c 的 SQ_SFX_BASE_VOLUME 一致（58）——
// 这一点由 tests/test_sq_volume.c 把等式钉住。
#define SQ_VOLUME_BASE_SFX 58

// 档位表本体：关、20%、40%、60%、80%、100%。
extern const uint8_t sq_volume_steps[SQ_VOLUME_STEP_COUNT];

// 把用户档位乘到某一路的基准音量上。整数截断（不是四舍五入）：base * level / 100。
uint8_t sq_volume_scale(uint8_t base, uint8_t level);

// 下一个档位，到顶回绕到关。当前值不在档位表里时回到关（而不是返回一个
// 不存在的档位，让用户再也调不动）。
uint8_t sq_volume_next(uint8_t level);

// 档位文本："关" 或 "80%"。返回写入的字节数；容量不足时返回 0 并把 out 置为
// 空串（不留半截字符串）。
uint16_t sq_volume_text(uint8_t level, char *out, size_t capacity);
