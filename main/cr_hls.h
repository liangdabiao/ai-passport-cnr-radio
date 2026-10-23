// main/cr_hls.h —— 流媒体 HTTP 的纯逻辑解析：响应头扫描/解析 + chunked 剥框。
//
// 与 ESP-IDF/LVGL 解耦，宿主测试直接编译它。设备端 cr_player 用它处理
// raw socket 读到的字节流（hot-daily 真机教训：esp_http_client 在 C3+lwip
// 上不可用；家族经 raw socket 全程验证 —— 本项目沿同一路径）。
//
// 设计要点：
//   头部扫描容忍分片到达（每次 socket read 都可能是半截头）；1xx 中间响应
//   跳过继续找真正的响应头 —— 有的台源会先回 100 Continue。
//   chunked 解码是有状态流式状态机：输出只可能 ≤ 输入，天然适配环形缓冲。
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// 在流缓冲里找响应头终点（"\r\n\r\n"）。返回头部总长（含终点四字节）；
// 未找到返回 0；have 超过 cap 仍找不到返回 SIZE_MAX（流损坏，应放弃）。
size_t cr_hls_head_scan(const char *buf, size_t have, size_t cap);

typedef struct {
    int status;               // 状态码（1xx 已被调用方跳过）
    bool chunked;             // Transfer-Encoding: chunked
    long long content_length; // Content-Length，没有则 -1
    bool accept_ranges;       // Accept-Ranges 存在（信息性，不参与判定）
    char location[192];       // 3xx 的 Location（原样，无则空串）
    char content_type[48];    // 小写化后的 Content-Type（截到分号前）
    char icy_name[64];        // icy-name（台名，无则空串）
    int icy_br;               // icy-br 码率 kbps（无则 0）
} cr_hls_head_t;

// 解析一个完整头部（cr_hls_head_scan 返回的长度范围）。头部字段大小写不
// 敏感；任何畸形都不算致命——尽力解析，status 解析失败返回 false。
bool cr_hls_head_parse(const char *head, size_t len, cr_hls_head_t *out);

// ---------------------------------------------------------------- chunked ----
typedef enum {
    CR_CK_SIZE = 0,  // 正在读 chunk 长度行（十六进制 + CRLF）
    CR_CK_SIZE_LF,   // 长度行收完 '\r'，等 '\n'
    CR_CK_DATA,      // 正在读 chunk 数据
    CR_CK_DATA_CR,   // 数据读完，等 '\r'
    CR_CK_DATA_LF,   // 数据读完收了 '\r'，等 '\n'
    CR_CK_TRAILER,   // 0 长度 chunk 后的尾注：滚窗扫 "\r\n\r\n"
    CR_CK_DONE,      // 流结束，不再消费
    CR_CK_BAD,       // 流损坏
} cr_chunk_state_t;

typedef struct {
    cr_chunk_state_t state;
    size_t remain;     // 当前 chunk 剩余字节数
    uint64_t size_acc; // 长度行累计值
    int size_digits;   // 长度行已收到的位数（防溢出）
    uint64_t window;   // 尾注滚动窗口（低 32 位为最近 4 字节）
} cr_chunker_t;

void cr_chunker_init(cr_chunker_t *c);

// 流式剥 chunked 框架：消费输入（推进 *in / *n），产出净荷到 out（推进 *cap）。
// 一次调用可同时消费与产出多次；坏流置 state = CR_CK_BAD 并停止消费。
// 调用方循环喂入直到输入耗尽或输出满。干净流（非 chunked）不要走这里。
void cr_chunker_feed(cr_chunker_t *c, const uint8_t **in, size_t *n,
                     uint8_t *out, size_t *cap);

// ------------------------------------------------------------- m3u8 ----
// 央广 HLS（2026-10 实测）：m3u8 播放列表 → 10 秒 .ts 切片（内装 ADTS AAC-LC）。
#define CR_HLS_MAX_SEGS 12  // 单次播放列表最多记这么多段（直播边只需最近几段）

typedef struct {
    // 段文件名（相对播放列表目录，原样存放，不含目录）。
    char names[CR_HLS_MAX_SEGS][64];
    uint8_t count;        // 实际段数（超出上限只保留最新的）
    uint32_t media_seq;   // #EXT-X-MEDIA-SEQUENCE（首段序号）
    uint8_t target_dur_s; // #EXT-X-TARGETDURATION（秒）
} cr_hls_playlist_t;

// 解析播放列表文本（长度由 len 给出，行分隔兼容 CRLF）。
// 返回 false = 不是合法播放列表（没有 EXTM3U 头）。
bool cr_hls_playlist_parse(const char *text, size_t len, cr_hls_playlist_t *out);

// ------------------------------------------------------------- ADTS ----
// 在 [buf, buf+n) 里找第一个「头合法」的 ADTS 帧。返回帧起始偏移；
// *frame_len_out 给出帧总长（含 7 字节头）。没找到返回 -1。
// 校验：同步字 FF+MPEG-4/层0、采样率索引 <13、声道 1..7、帧长 8..8192
// 且不超过剩余缓冲。TS/PES 头只是帧间噪声，扫描天然跳过，无需 TS 解复用。
int cr_adts_find(const uint8_t *buf, size_t n, uint16_t *frame_len_out);

