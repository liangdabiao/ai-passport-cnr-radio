// pgmspace.h —— AVR/Arduino PROGMEM 兼容 shim（ESP-IDF 构建专用）。
//
// 本组件（Helix MP3，取自 ESP8266Audio/src/libhelix-mp3）的常量表用 PROGMEM
// 限定符 + pgm_read_word/byte 访问器。ESP32 的 flash 直接映射，按原宽度解引用
// 即为正确语义。ARDUINO 平台宏由组件 CMakeLists 私有定义（只影响本组件的
// mp3dec.h / assembly.h 平台门，不泄漏到工程其他部分）。
#pragma once

#include <stdint.h>

#define PROGMEM

#define pgm_read_byte(p) (*(const uint8_t *)(p))
#define pgm_read_word(p) (*(const uint16_t *)(p))
#define pgm_read_dword(p) (*(const uint32_t *)(p))
