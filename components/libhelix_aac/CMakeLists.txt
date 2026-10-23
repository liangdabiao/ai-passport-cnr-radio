# Helix AAC 定点解码器（RealNetworks Helix DNA，许可见本目录 LICENSE/RCSL_RPSL）。
# 取自 earlephilhower/ESP8266Audio src/libhelix-aac，处理 radio.cn 的 HLS AAC-LC 流。
# 平台适配：aacdec.h/assembly.h 平台门加了 __riscv 通用分支；pgmspace.h 为 shim。
idf_component_register(
    SRCS aacdec.c aactabs.c bitstream.c buffers.c dct4.c decelmnt.c dequant.c fft.c filefmt.c huffmanaac.c hufftabs.c imdct.c noiseless.c pns.c sbr.c sbrfft.c sbrfreq.c sbrhfadj.c sbrhfgen.c sbrhuff.c sbrimdct.c sbrmath.c sbrqmf.c sbrside.c sbrtabs.c stproc.c tns.c trigtabs.c
    INCLUDE_DIRS "."
)

target_compile_options(${COMPONENT_LIB} PRIVATE -O2)

# radio.cn 全平台 AAC-LC（ffprobe 实测），SBR 关闭：省 ~50KB PSInfoSBR，
# 24KB 静态池即可（宿主/设备同宏，见 aaccommon.h）。
target_compile_definitions(${COMPONENT_LIB} PRIVATE AAC_DISABLE_SBR)
