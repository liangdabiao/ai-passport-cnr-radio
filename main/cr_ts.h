// main/cr_ts.h —— MPEG-TS → PES → ADTS AAC 解复用（纯逻辑，宿主可测）。
//
// radio.cn 的 HLS 切片是单节目 TS：PAT(0x0000) → PMT → 一路 AAC ADTS
// (stream_type 0x0F)。音频帧会被 PES/TS 头从中间打断（真机教训：同步字
// 扫描法只能解出 154/431 帧），必须正经解复用。实测一个 10 秒切片 = 357
// 个 TS 包 = 23 个 PES = 431 帧 AAC。
//
// 用法（流式）：cr_ts_feed(&ts, chunk, n, sink, ctx) —— 任意大小的输入块，
// 解出的净荷（ADTS 字节流）经 sink 回调交出。内部状态机自动找包同步、
// PAT/PMT、音频 PID；PUSI 起 PES 头、非 PUSI 续包。
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CR_TS_PACKET 188

typedef enum {
    CR_TS_WAIT_PAT = 0, // 等 PAT（或回退启发式锁定音频 PID）
    CR_TS_WAIT_PMT,
    CR_TS_RUNNING,      // 音频 PID 已锁定
} cr_ts_phase_t;

typedef struct {
    cr_ts_phase_t phase;
    uint16_t pmt_pid;
    uint16_t audio_pid;

    uint8_t pkt[CR_TS_PACKET]; // 包组装缓冲（socket 分片到达）
    size_t pkt_fill;

    // 输出暂存：sink 一次吃不下时分批（调用方循环冲刷）。
    uint8_t out[2048];
    size_t out_len;
} cr_ts_t;

typedef void (*cr_ts_sink_t)(void *ctx, const uint8_t *data, size_t n);

void cr_ts_init(cr_ts_t *ts);

// 喂入任意长度字节；解出的 ADTS 净荷经 sink 交出。返回 false = 流损坏
// （长期丢同步）。
bool cr_ts_feed(cr_ts_t *ts, const uint8_t *data, size_t n,
                cr_ts_sink_t sink, void *ctx);

// 冲刷剩余输出（段尾调用）。
void cr_ts_flush(cr_ts_t *ts, cr_ts_sink_t sink, void *ctx);
