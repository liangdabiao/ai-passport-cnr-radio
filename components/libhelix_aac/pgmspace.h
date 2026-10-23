// pgmspace.h —— AVR/Arduino PROGMEM 兼容 shim（ESP-IDF 构建专用）。
// 同 libhelix_mp3 的做法：ESP32 flash 直接映射，空限定符 + 原宽解引用即正确。
#pragma once

#include <stdint.h>

#define PROGMEM

#define pgm_read_byte(p) (*(const uint8_t *)(p))
#define pgm_read_word(p) (*(const uint16_t *)(p))
#define pgm_read_dword(p) (*(const uint32_t *)(p))
