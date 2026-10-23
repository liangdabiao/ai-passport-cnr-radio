# Helix MP3 定点解码器（RealNetworks Helix DNA，RPSL/RCSL 许可见本目录 LICENSE.txt）。
# 取自 earlephilhower/ESP8266Audio src/libhelix-mp3（ESP32-audioI2S 同源解码核心），
# 纯 C 定点实现，无汇编路径、无入口残留。
idf_component_register(
    SRCS "bitstream.c"
         "buffers.c"
         "dct32.c"
         "dequant.c"
         "dqchan.c"
         "huffman.c"
         "hufftabs.c"
         "imdct.c"
         "mp3tabs.c"
         "mp3dec.c"
         "polyphase.c"
         "scalfact.c"
         "stproc.c"
         "subband.c"
         "trigtabs.c"
    INCLUDE_DIRS "."
)

# 定点整数解码，禁用浮点单元依赖；-O2 保证解码吞吐（160MHz 单核实时解码 64-128kbps）。
target_compile_options(${COMPONENT_LIB} PRIVATE -O2)

# 上游按 Arduino 构建；本组件已给 mp3dec.h / assembly.h 的平台门打了
# __riscv 补丁（走通用 C 分支），PROGMEM / pgm_read_* 由本目录 pgmspace.h
# shim 提供。无需额外平台宏。
