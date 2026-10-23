// main/cr_player.c —— 见 cr_player.h。
//
// 央广 HLS 播放链路（radio.cn 2026-10 实测形态）：
//   m3u8 播放列表（纯 http，长效 key）→ 10 秒 .ts 切片 → TS 汤里的 ADTS
//   AAC-LC 帧 → helix AAC 定点解码（静态 arena 40KB，无 PSRAM 也装得下）
//   → 降混单声道 → ES8311。
//
// 单任务：段下载与解码交错（recv 一块就挤进环形缓冲，环到高水位切去解码到
// 低水位）——环形只有 10KB，装不下 67KB 的切片，必须边下边解。节拍器仍是
// I2S 阻塞写。切台用 serial 代号移交；会话失败 1s/2s/4s 退避重连。
//
// 格式契约：本模块在出声时独占 codec；停播后 codec 保留流格式，cr_sfx 出声
// 前必须先恢复 16kHz（见 cr_sfx.c）。
#include "cr_player.h"

#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lwip/sockets.h"

#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "bsp_audio.h"

#include "aacdec.h"
#include "cr_hls.h"
#include "cr_ts.h"
#include "cr_station.h"
#include "cr_volume.h"

static const char *TAG = "cr_player";

#define PLAYER_TASK_STACK 10240
#define PLAYER_TASK_PRIO  6  // 高于 LVGL(4)/输入(5)：解码不能被 UI 挤掉

#define RING_SIZE    (10 * 1024) // 压缩流环形缓冲（TS 汤 + ADTS 混装）
#define START_MARK   (5 * 1024)  // 攒到这个水位才开始解码
#define HIGH_WATER   (8 * 1024)  // 段下载中环到这个水位就去解码
#define LOW_WATER    (2 * 1024)  // 解码停点：回去继续收段
#define DECODE_PER_LOOP 4        // 每轮最多解几帧
#define GAIN_MASTER  3           // 100% 档位的数字增益倍数（codec 满量程之上）
#define STAGING_SIZE 4096        // 解码线性暂存（AAC 帧 ≤ 2KB，容 TS 噪声回绕）
#define HEAD_MAX     2048        // 响应头上限
#define HEAD_DEADLINE_MS 10000   // 等响应头的总时限
#define RETRY_MAX    3           // 会话失败重试（1s/2s/4s 退避）
#define STARVE_MS    12000       // 持续无新数据判死（切片 10s 一片，留足余量）
#define RECV_MS      20          // 单次 recv 超时：短到不拦住 DMA 喂数
#define AAC_ARENA_SIZE (24 * 1024) // AACInitDecoderPre 的池（惰性堆分配）
#define CR_PLBUF_SIZE 2048     // m3u8 播放列表缓冲
#define CR_ES_SIZE 4096       // 解复用 ES 暂存
#define CR_PENDING_SIZE 2048  // socket 原始块（SBR 关）

// 文案进 UI 状态行（16px 一行 ≤13 全角，短句）。
#define MSG_CONNECT   "连接中…"
#define MSG_PLAYING   "正在播放"
#define MSG_NO_URL    "电台地址无效"
#define MSG_UNREACH   "连不上电台"
#define MSG_BADSTREAM "电台数据异常"

typedef enum {
    SESS_REQUEST = 0, // 新请求/停止：移交控制权
    SESS_FAIL,        // 流失败（*msg_out 给出错误文案）
} session_end_t;

typedef struct {
    cr_play_status_t st;      // 快照区（lock 保护）
    SemaphoreHandle_t lock;

    TaskHandle_t task;
    volatile uint32_t serial; // 请求代号：play/stop 各 +1
    volatile uint8_t req_station;
    volatile bool req_valid;

    // 以下仅任务上下文触碰：
    uint8_t *ring;
    size_t ring_r, ring_w, ring_used;
    HAACDecoder dec;
    bool dec_open;
    bool format_set;          // 本次会话是否已按流设置过 codec
    uint32_t fmt_hz;
    uint8_t fmt_ch;
} player_t;

static player_t P;
// 播放器全部缓冲都是惰性堆分配（见 cr_player_start）：配网门户期 AP+httpd+
// DHCP 需要堆（真机实测静态占着时 GET / 只剩 9.2KB，页面与 DHCP 随机饿死），
// 播放器第一次出声时才占内存。
static int16_t *s_pcm;            // 一帧 PCM 上限（1024 样本 × 2 声道）
static uint8_t *s_staging;
static unsigned char *s_aac_arena_bytes;
static uint8_t s_level = CR_VOLUME_DEFAULT; // 用户音量档（播放器侧副本）
static uint8_t s_applied_codec_vol;         // 已下发的 codec 音量（0 = 需重下发）

// AAC 解码器池：helix 的 AACInitDecoderPre 从惰性分配的内存切结构体。
static uint8_t *s_pending; // socket 原始块（cr_ts 的输入侧）
static uint8_t *s_es;      // 解复用出的 ADTS 净荷暂存
static size_t s_es_len;
static cr_ts_t s_ts;       // TS 解复用状态机（内嵌 2KB out，静态可承受）
static size_t s_pending_len;
static char *s_plbuf;      // m3u8 播放列表整体缓冲

static uint8_t cr_level_get(void) { return s_level; }

// cr_ts 的 sink：ES 先落暂存（feed 返回后由主循环泵进环形缓冲）。
static void es_sink(void *ctx, const uint8_t *data, const size_t n)
{
    (void)ctx;
    if (!s_es || s_es_len + n > 4096) return; // 主循环每轮冲刷，实际到不了
    memcpy(s_es + s_es_len, data, n);
    s_es_len += n;
}

static void publish(const cr_play_state_t state, const char *msg)
{
    xSemaphoreTake(P.lock, portMAX_DELAY);
    P.st.state = state;
    if (msg) snprintf(P.st.msg, sizeof(P.st.msg), "%s", msg);
    xSemaphoreGive(P.lock);
}

static void publish_retry(const unsigned attempt, const unsigned total)
{
    xSemaphoreTake(P.lock, portMAX_DELAY);
    P.st.state = CR_PLAY_CONNECT;
    snprintf(P.st.msg, sizeof(P.st.msg), "重连 %u/%u…", attempt, total);
    xSemaphoreGive(P.lock);
}

static int64_t now_ms(void)
{
    return esp_timer_get_time() / 1000;
}

// ---------------------------------------------------------------- 环形 ----
static void ring_reset(void)
{
    P.ring_r = P.ring_w = P.ring_used = 0;
}

static size_t ring_space(void)
{
    return RING_SIZE - P.ring_used;
}

// 拷出至多 want 字节到线性区（不消费，回绕自动拼头尾）。返回拷到的字节数。
static size_t ring_peek_linear(uint8_t *dst, const size_t want)
{
    size_t take = want < P.ring_used ? want : P.ring_used;
    const size_t tail = RING_SIZE - P.ring_r;
    const size_t first = take < tail ? take : tail;
    memcpy(dst, P.ring + P.ring_r, first);
    if (first < take) {
        memcpy(dst + first, P.ring, take - first);
    }
    return take;
}

static void ring_drop(size_t n)
{
    if (n > P.ring_used) n = P.ring_used;
    P.ring_r = (P.ring_r + n) % RING_SIZE;
    P.ring_used -= n;
}

// 净荷尽量塞进环形缓冲，返回**实际消费**的输入字节数（绝不丢字节：环满时
// 剩下的留在调用方缓冲里，等解码腾出空间再来）。
static size_t ring_push_some(const uint8_t *data, const size_t len)
{
    size_t used = 0;
    while (used < len) {
        const size_t space = ring_space();
        if (space == 0) break;
        uint8_t *out = P.ring + P.ring_w;
        size_t cap = RING_SIZE - P.ring_w; // 到缓冲尾的直线长度
        if (cap > space) cap = space;
        const size_t move = (len - used) < cap ? (len - used) : cap;
        memcpy(out, data + used, move);
        P.ring_w = (P.ring_w + move) % RING_SIZE;
        P.ring_used += move;
        used += move;
    }
    return used;
}

static void pending_reset(void) { s_pending_len = 0; }

// ---------------------------------------------------------------- URL ----
// 解析 "http://host[:port]/path"。out_dir（可 NULL）收播放列表目录（含尾 '/'）。
static bool split_url_dir(const char *url, char *host, const size_t host_cap,
                          char *path, const size_t path_cap, int *port,
                          char *out_dir, const size_t dir_cap)
{
    if (strncmp(url, "http://", 7) != 0) return false;
    url += 7;
    const char *slash = strchr(url, '/');
    const char *colon = strchr(url, ':');
    if (!slash) return false;

    size_t host_len = (size_t)(slash - url);
    if (colon && colon < slash) {
        host_len = (size_t)(colon - url);
        *port = atoi(colon + 1);
        if (*port <= 0 || *port > 65535) return false;
    } else {
        *port = 80;
    }
    if (host_len == 0 || host_len >= host_cap) return false;
    memcpy(host, url, host_len);
    host[host_len] = '\0';

    const size_t path_len = strlen(slash);
    if (path_len >= path_cap) return false;
    memcpy(path, slash, path_len + 1);

    if (out_dir && dir_cap > 0) {
        const char *last = strrchr(slash, '/');
        size_t dir_len = (size_t)(last - slash) + 1;
        if (dir_len >= dir_cap) return false;
        memcpy(out_dir, slash, dir_len);
        out_dir[dir_len] = '\0';
    }
    return true;
}

static int tcp_open(const char *host, const int port)
{
    const struct hostent *he = gethostbyname(host);
    if (!he || !he->h_addr_list[0]) {
        ESP_LOGW(TAG, "DNS 失败: %s", host);
        return -1;
    }
    const int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    struct timeval tv = { .tv_sec = 8, .tv_usec = 0 };
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in dest = {
        .sin_family = AF_INET,
        .sin_port = htons((uint16_t)port),
        .sin_addr = *(struct in_addr *)he->h_addr_list[0],
    };
    if (connect(sock, (struct sockaddr *)&dest, sizeof(dest)) != 0) {
        ESP_LOGW(TAG, "TCP 失败 %s:%d", host, port);
        close(sock);
        return -1;
    }
    tv.tv_sec = 0;
    tv.tv_usec = RECV_MS * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    return sock;
}

// 发 GET、读完整响应头。成功返回 0：sock 保持打开（净荷续用同一连接），
// *out_head 是 malloc 的头部（调用方 free）。头之后顺带读到的净荷放进
// s_pending（调用方从那里接着处理）。1xx 跳过；非 200 失败；3xx 不跟。
static int fetch_head(const char *host, const int port, const char *path,
                      const int sock, char **out_head, size_t *out_head_len)
{
    *out_head = NULL;
    *out_head_len = 0;

    char request[640];
    const int rl = snprintf(request, sizeof(request),
                            "GET %s HTTP/1.1\r\n"
                            "Host: %s:%d\r\n"
                            "User-Agent: VLC/3.0 LibVLC/3.0\r\n"
                            "Accept: */*\r\n"
                            "Connection: close\r\n"
                            "\r\n",
                            path, host, port);
    if (rl <= 0 || (size_t)rl >= sizeof(request)) return -1;
    if (write(sock, request, (size_t)rl) != rl) return -1;

    char *buf = malloc(HEAD_MAX);
    if (!buf) return -1;
    size_t have = 0;
    const int64_t deadline = now_ms() + HEAD_DEADLINE_MS;
    for (;;) {
        if (have >= HEAD_MAX || now_ms() > deadline) { free(buf); return -1; }
        const int n = read(sock, buf + have, HEAD_MAX - have);
        if (n < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            free(buf);
            return -1;
        }
        if (n == 0) { free(buf); return -1; }
        have += (size_t)n;

        const size_t head_len = cr_hls_head_scan(buf, have, HEAD_MAX);
        if (head_len == 0) continue;
        if (head_len == SIZE_MAX) { free(buf); return -1; }

        cr_hls_head_t head;
        if (!cr_hls_head_parse(buf, head_len, &head)) { free(buf); return -1; }
        if (head.status < 200) { // 1xx：丢弃，找真正的响应头
            memmove(buf, buf + head_len, have - head_len);
            have -= head_len;
            continue;
        }
        if (head.status != 200) { free(buf); return -1; }

        *out_head = buf;
        *out_head_len = head_len;
        const size_t body_have = have - head_len;
        if (body_have > 0 && body_have <= 2048) {
            memcpy(s_pending, buf + head_len, body_have);
            s_pending_len = body_have;
        }
        return 0;
    }
}

// 读小响应体（播放列表）：s_pending 开头 + 继续读到连接关闭或缓冲满。
// 返回体长度（>0 且以 NUL 结尾存进 s_plbuf）。
static size_t read_small_body(const int sock)
{
    size_t len = 0;
    if (s_pending_len > 0) {
        len = s_pending_len < CR_PLBUF_SIZE - 1 ? s_pending_len : CR_PLBUF_SIZE - 1;
        memcpy(s_plbuf, s_pending, len);
        s_plbuf[len] = '\0';
        pending_reset();
    }
    while (len + 1 < CR_PLBUF_SIZE) {
        const int n = read(sock, (uint8_t *)s_plbuf + len, CR_PLBUF_SIZE - 1 - len);
        if (n <= 0) break;
        len += (size_t)n;
        s_plbuf[len] = '\0';
    }
    return len;
}

// 取一份播放列表（打开、取头、读体、解析）。失败返回 false。
static bool fetch_playlist(const char *host, const int port, const char *path,
                           cr_hls_playlist_t *out)
{
    const int sock = tcp_open(host, port);
    if (sock < 0) return false;
    char *head = NULL;
    size_t head_len = 0;
    pending_reset();
    const int fr = fetch_head(host, port, path, sock, &head, &head_len);
    free(head);
    if (fr != 0) {
        close(sock);
        return false;
    }
    const size_t len = read_small_body(sock);
    close(sock);
    return len > 0 && cr_hls_playlist_parse(s_plbuf, len, out);
}

// ---------------------------------------------------------------- 解码 ----
// 从环形缓冲解一帧 ADTS AAC 并写出。返回 1=出一帧；0=数据不足/找同步中；
// -1=写设备失败（连续 20 次才判死，调用方收尾）。
static int decode_write(void)
{
    const size_t have = ring_peek_linear(s_staging, STAGING_SIZE);
    if (have < 64) return 0;

    uint16_t frame_len = 0;
    const int off = cr_adts_find(s_staging, have, &frame_len);
    if (off < 0) {
        // 整段没有合法 ADTS 头：丢掉（留 6 字节给跨块同步）。
        ring_drop(have > 6 ? have - 6 : have);
        return 0;
    }
    if (off > 0) ring_drop((size_t)off);

    uint8_t *in = s_staging + off;
    int bytes_left = (int)(have - off);
    const int err = AACDecode(P.dec, &in, &bytes_left, s_pcm);
    const int consumed = (int)(have - off) - bytes_left;

    if (err == ERR_AAC_INDATA_UNDERFLOW) return 0; // 帧未到齐：等数据
    if (err != 0) {
        // 坏帧：跳两个字节重新找同步（限制丢量，防整环被拖死）。
        size_t drop = (size_t)(consumed > 0 ? consumed : 0) + 2;
        ring_drop(drop < have ? drop : have);
        return 0;
    }
    ring_drop((size_t)(consumed > 0 ? consumed : (int)frame_len));

    AACFrameInfo fi;
    AACGetLastFrameInfo(P.dec, &fi);
    if (fi.outputSamps <= 0 || fi.sampRateOut == 0 || fi.nChans == 0) return 0;
    if (fi.nChans > 2) return 0; // 多声道流不做映射，跳帧

    // ES8311 是单声道 codec：立体声降混 (L+R)/2（真机实测立体声喂法慢于
    // 实时，声音发闷拖沓）。
    int out_samples = fi.outputSamps;
    if (fi.nChans == 2) {
        out_samples /= 2;
        for (int i = 0; i < out_samples; i++) {
            s_pcm[i] = (int16_t)(((int)s_pcm[i * 2] + s_pcm[i * 2 + 1]) / 2);
        }
    }

    if (!P.format_set || P.fmt_hz != (uint32_t)fi.sampRateOut || P.fmt_ch != 1) {
        if (bsp_audio_set_format((uint32_t)fi.sampRateOut, 16, 1) != ESP_OK) {
            ESP_LOGE(TAG, "设置流格式失败 %uHz", fi.sampRateOut);
            return -1;
        }
        P.format_set = true;
        P.fmt_hz = (uint32_t)fi.sampRateOut;
        P.fmt_ch = 1;
        s_applied_codec_vol = 0; // 重新打开的 codec 需要重新下发音量
        ESP_LOGI(TAG, "流格式: %uHz %uch %ukbps",
                 fi.sampRateOut, fi.nChans, fi.bitRate / 1000);
    }

    xSemaphoreTake(P.lock, portMAX_DELAY);
    P.st.kbps = (uint16_t)(fi.bitRate / 1000);
    P.st.sample_hz = (uint32_t)fi.sampRateOut;
    P.st.channels = (uint8_t)fi.nChans;
    xSemaphoreGive(P.lock);

    if (s_applied_codec_vol != 100) {
        bsp_audio_set_volume(100);
        s_applied_codec_vol = 100;
    }
    // Q8 定点增益：codec 音量曲线 100 档才 0dB，满量程 + 软件增益（100%=3 倍）。
    const int32_t gain_q8 = (int32_t)cr_level_get() * GAIN_MASTER * 256 / 100;
    if (gain_q8 != 256) {
        for (int i = 0; i < out_samples; i++) {
            int32_t v = ((int32_t)s_pcm[i] * gain_q8) >> 8;
            if (v > 32767) v = 32767;
            if (v < -32768) v = -32768;
            s_pcm[i] = (int16_t)v;
        }
    }

    static unsigned consecutive;
    if (bsp_audio_write(s_pcm, (size_t)out_samples * sizeof(int16_t)) != ESP_OK) {
        if (++consecutive >= 20) {
            consecutive = 0;
            return -1;
        }
        return 0;
    }
    consecutive = 0;
    return 1;
}

// 把 ES 暂存泵进环形缓冲（解码腾出空间后反复调用直到清空）。
static void es_pump(void)
{
    const size_t used = ring_push_some(s_es, s_es_len);
    if (used > 0) {
        memmove(s_es, s_es + used, s_es_len - used);
        s_es_len -= used;
    }
}

typedef enum { SEG_OK = 0, SEG_ABORT, SEG_FAIL } seg_result_t;

// 下载一个切片：边收边解（socket 块 → cr_ts 解复用 → ES 泵进环 → 高水位
// 切去解码到低水位）。SEG_FAIL 只是**这一段**失败——直播里丢一段无感，
// 调用方刷新播放列表跳到直播边即可，不杀会话（真机教训：网络偶发抽风
// 每 ~2 分钟一段失败，旧逻辑整会话重连，听感断续）。
static seg_result_t stream_segment(const char *host, const int port,
                                   const char *seg_url,
                                   const uint32_t my_serial, bool *aborted,
                                   int64_t *last_data_ms)
{
    *aborted = false;
    const int sock = tcp_open(host, port);
    if (sock < 0) return SEG_FAIL;

    char *head = NULL;
    size_t head_len = 0;
    pending_reset();
    if (fetch_head(host, port, seg_url, sock, &head, &head_len) != 0) {
        free(head);
        close(sock);
        return SEG_FAIL;
    }
    free(head);
    // 头之后顺带读到的切片开头也走解复用。
    if (s_pending_len > 0) {
        (void)cr_ts_feed(&s_ts, s_pending, s_pending_len, es_sink, NULL);
        s_pending_len = 0;
    }

    bool playing = P.st.state == CR_PLAY_PLAYING;
    for (;;) {
        if (P.serial != my_serial) {
            close(sock);
            *aborted = true;
            return SEG_ABORT;
        }

        const int n = read(sock, s_pending, CR_PENDING_SIZE);
        if (n > 0) {
            *last_data_ms = now_ms();
            (void)cr_ts_feed(&s_ts, s_pending, (size_t)n, es_sink, NULL);
        } else if (n == 0) {
            cr_ts_flush(&s_ts, es_sink, NULL);
            es_pump();
            close(sock);
            return true; // 切片完成（服务端关流）
        } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
            close(sock);
            return SEG_FAIL; // 中途断：当本段失败，丢段续播
        }

        es_pump();
        if (!playing && P.ring_used >= START_MARK) {
            playing = true;
            publish(CR_PLAY_PLAYING, MSG_PLAYING);
        }
        if (P.ring_used >= HIGH_WATER || (n <= 0 && P.ring_used > 0)) {
            int burst = 0;
            for (;;) {
                if (P.serial != my_serial) break;
                const int rc = decode_write();
                if (rc < 0) {
                    close(sock);
                    return SEG_FAIL;
                }
                if (rc == 0) break;
                if (++burst >= DECODE_PER_LOOP * 4) break;
                if (P.ring_used <= LOW_WATER) break;
            }
            if (P.serial != my_serial) {
                close(sock);
                *aborted = true;
                return SEG_ABORT;
            }
        }
    }
}

// 与一个电台周旋到结束（重试在外层）。返回值见 session_end_t。
static session_end_t play_one(const char *url, const uint32_t my_serial,
                              const char **msg_out)
{
    *msg_out = MSG_UNREACH;
    char host[96];
    char path[160];
    char dir[128];
    int port;
    if (!split_url_dir(url, host, sizeof(host), path, sizeof(path), &port,
                       dir, sizeof(dir))) {
        *msg_out = MSG_NO_URL;
        return SESS_FAIL;
    }

    ring_reset();
    pending_reset();
    s_es_len = 0;
    cr_ts_init(&s_ts);
    P.format_set = false;
    int64_t last_data = now_ms();
    int64_t last_pl_ms = 0;
    unsigned pl_fail = 0;
    publish(CR_PLAY_CONNECT, MSG_CONNECT);

    cr_hls_playlist_t pl;
    if (!fetch_playlist(host, port, path, &pl)) {
        *msg_out = MSG_UNREACH;
        return SESS_FAIL;
    }
    ESP_LOGI(TAG, "播放列表: %u 段 media_seq=%u 目标%us",
             pl.count, pl.media_seq, pl.target_dur_s);
    last_pl_ms = now_ms();
    last_data = now_ms();

    // 从倒数第 2 段开始（贴直播边又留一拍余量）。
    int seg_i = pl.count >= 2 ? pl.count - 2 : 0;
    char last_name[64] = "";

    for (;;) {
        if (P.serial != my_serial) return SESS_REQUEST;
        if (now_ms() - last_data > STARVE_MS) {
            *msg_out = MSG_UNREACH;
            return SESS_FAIL;
        }

        if (seg_i < pl.count) {
            char seg_url[288];
            snprintf(seg_url, sizeof(seg_url), "%s%s", dir, pl.names[seg_i]);
            snprintf(last_name, sizeof(last_name), "%s", pl.names[seg_i]);
            ESP_LOGD(TAG, "取段 %s", pl.names[seg_i]);
            bool aborted = false;
            const seg_result_t sr = stream_segment(host, port, seg_url,
                                                   my_serial, &aborted,
                                                   &last_data);
            if (sr == SEG_ABORT) return SESS_REQUEST;
            if (sr == SEG_FAIL) {
                static unsigned seg_fail_streak;
                if (++seg_fail_streak >= 4) {
                    seg_fail_streak = 0;
                    *msg_out = MSG_BADSTREAM;
                    return SESS_FAIL;
                }
                ESP_LOGW(TAG, "段失败，跳段续播（连续 %u）", seg_fail_streak);
                // 跳到直播边：重新拉列表从最新段开始。
                cr_hls_playlist_t plx;
                if (fetch_playlist(host, port, path, &plx)) {
                    pl = plx;
                }
                seg_i = pl.count >= 2 ? pl.count - 2 : 0;
                last_data = now_ms();
                continue;
            }
            seg_i++;
            pl_fail = 0;
            continue;
        }

        // 已到列表尾：解码掉环里的余量，等半拍再刷新列表。
        int burst = 0;
        for (;;) {
            const int rc = decode_write();
            if (rc != 1) break;
            if (++burst >= DECODE_PER_LOOP * 6) break;
        }
        const int64_t refresh_ms =
            (int64_t)(pl.target_dur_s ? pl.target_dur_s : 10) * 500;
        if (now_ms() - last_pl_ms < refresh_ms) {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }

        cr_hls_playlist_t pl2;
        if (!fetch_playlist(host, port, path, &pl2)) {
            if (++pl_fail > 6) { *msg_out = MSG_UNREACH; return SESS_FAIL; }
            vTaskDelay(pdMS_TO_TICKS(400));
            continue;
        }
        last_pl_ms = now_ms();
        // 从「上次段名 + 1」接着播；找不到（列表滚过头）就跳到直播边。
        int next_i = -1;
        for (int i = 0; i < pl2.count; i++) {
            if (strcmp(pl2.names[i], last_name) == 0) { next_i = i + 1; break; }
        }
        if (next_i < 0 || next_i >= pl2.count) {
            next_i = pl2.count >= 2 ? pl2.count - 2 : 0;
        }
        pl = pl2;
        seg_i = next_i;
        last_data = now_ms();
    }
}

// ------------------------------------------------------------- 播放外壳 ----
static void play_station(const uint8_t station)
{
    char url[192];
    snprintf(url, sizeof(url), "%s", CR_STATIONS[station].url);
    const uint32_t my_serial = P.serial;
    unsigned retry = 0;

    for (;;) {
        const char *msg = MSG_UNREACH;
        const session_end_t end = play_one(url, my_serial, &msg);

        if (end == SESS_REQUEST) return;
        ESP_LOGW(TAG, "会话结束: %s（第 %u 次失败）", msg, retry + 1);
        if (++retry > RETRY_MAX) {
            publish(CR_PLAY_ERROR, msg);
            return;
        }
        publish_retry(retry, RETRY_MAX);
        vTaskDelay(pdMS_TO_TICKS(1000 << (retry - 1)));
        if (P.serial != my_serial) return;
    }
}

static void player_task(void *arg)
{
    (void)arg;
    for (;;) {
        if (!P.req_valid) {
            if (P.st.state != CR_PLAY_IDLE) publish(CR_PLAY_IDLE, NULL);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        const uint8_t station = P.req_station;
        P.req_valid = false;

        xSemaphoreTake(P.lock, portMAX_DELAY);
        P.st.station = station;
        P.st.msg[0] = '\0';
        P.st.kbps = 0;
        P.st.sample_hz = 0;
        P.st.channels = 0;
        xSemaphoreGive(P.lock);

        play_station(station);
        ring_reset();
    }
}

// ---------------------------------------------------------------- 对外 ----
void cr_player_start(void)
{
    if (P.task) return;
    P.lock = xSemaphoreCreateMutex();
    if (!P.lock) {
        ESP_LOGE(TAG, "互斥量创建失败");
        return;
    }
    P.ring = malloc(RING_SIZE);
    s_pcm = (int16_t *)malloc(1024 * 2 * sizeof(int16_t));
    s_staging = malloc(STAGING_SIZE);
    s_aac_arena_bytes = malloc(AAC_ARENA_SIZE);
    s_pending = malloc(CR_PENDING_SIZE);
    s_es = malloc(CR_ES_SIZE);
    s_plbuf = malloc(CR_PLBUF_SIZE);
    if (!P.ring || !s_pcm || !s_staging || !s_aac_arena_bytes || !s_pending ||
        !s_es || !s_plbuf) {
        ESP_LOGE(TAG, "播放缓冲分配失败: free=%u",
                 (unsigned)esp_get_free_heap_size());
        free(P.ring); free(s_pcm); free(s_staging); free(s_aac_arena_bytes);
        free(s_pending); free(s_es); free(s_plbuf);
        P.ring = NULL; s_pcm = NULL; s_staging = NULL; s_aac_arena_bytes = NULL;
        s_pending = NULL; s_es = NULL; s_plbuf = NULL;
        vSemaphoreDelete(P.lock);
        P.lock = NULL;
        return;
    }
    P.dec = AACInitDecoderPre(s_aac_arena_bytes, AAC_ARENA_SIZE);
    if (!P.dec) {
        ESP_LOGE(TAG, "AAC 解码器初始化失败: arena=%u free=%u",
                 (unsigned)AAC_ARENA_SIZE,
                 (unsigned)esp_get_free_heap_size());
        free(P.ring); free(s_pcm); free(s_staging); free(s_aac_arena_bytes);
        free(s_pending); free(s_es); free(s_plbuf);
        s_pcm = NULL; s_staging = s_aac_arena_bytes = NULL;
        s_pending = s_es = NULL; s_plbuf = NULL;
        P.ring = NULL;
        vSemaphoreDelete(P.lock);
        P.lock = NULL;
        return;
    }
    P.dec_open = true;
    ESP_LOGI(TAG, "解码器就绪: free=%u largest=%u",
             (unsigned)esp_get_free_heap_size(),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));

    if (xTaskCreate(player_task, "cr_play", PLAYER_TASK_STACK, NULL,
                    PLAYER_TASK_PRIO, &P.task) != pdPASS) {
        ESP_LOGE(TAG, "播放任务创建失败");
        P.dec_open = false;
        P.dec = NULL;
        free(P.ring); free(s_pcm); free(s_staging); free(s_aac_arena_bytes);
        free(s_pending); free(s_es); free(s_plbuf);
        P.ring = NULL; s_pcm = NULL; s_staging = s_aac_arena_bytes = NULL;
        s_pending = s_es = NULL; s_plbuf = NULL;
        vSemaphoreDelete(P.lock);
        P.lock = NULL;
    }
}

void cr_player_play(const uint8_t station_index)
{
    if (!cr_station_valid(station_index)) return;
    // 惰性启动：配网门户期堆紧张（见 main.c 注释），播放器等第一次真正要
    // 出声时才占内存；此后常驻。
    if (!P.task) cr_player_start();
    if (!P.task) return;
    P.req_station = station_index;
    P.req_valid = true;
    P.serial++;
}

void cr_player_stop(void)
{
    if (!P.task) return;
    P.req_valid = false;
    P.serial++;
}

bool cr_player_is_streaming(void)
{
    if (!P.lock) return false;
    xSemaphoreTake(P.lock, portMAX_DELAY);
    const cr_play_state_t state = P.st.state;
    xSemaphoreGive(P.lock);
    return state == CR_PLAY_CONNECT || state == CR_PLAY_BUFFERING ||
           state == CR_PLAY_PLAYING;
}

void cr_player_status(cr_play_status_t *out)
{
    if (!out) return;
    if (!P.lock) {
        memset(out, 0, sizeof(*out));
        return;
    }
    xSemaphoreTake(P.lock, portMAX_DELAY);
    *out = P.st;
    xSemaphoreGive(P.lock);
}

void cr_player_set_volume(const uint8_t level)
{
    s_level = level;
    // 播放中由 decode_write 按帧应用；待机时直接落到 codec（音效路会再设
    // 自己的基准，见 cr_sfx.c）。
    if (!cr_player_is_streaming()) {
        bsp_audio_set_volume((uint8_t)(((uint32_t)level * 58u) / 100u));
    }
}
