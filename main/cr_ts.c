// main/cr_ts.c —— 见 cr_ts.h。
#include "cr_ts.h"

#include <string.h>

// sink 直写模式：先攒进 ts->out，满了或 feed 结束时冲给 sink —— 这样
// sink 的调用次数少而整，调用方（环形缓冲泵）好接。
static void emit(cr_ts_t *ts, const uint8_t *data, const size_t n,
                 cr_ts_sink_t sink, void *ctx)
{
    size_t pos = 0;
    while (pos < n) {
        const size_t room = sizeof(ts->out) - ts->out_len;
        const size_t take = n - pos < room ? n - pos : room;
        memcpy(ts->out + ts->out_len, data + pos, take);
        ts->out_len += take;
        pos += take;
        if (ts->out_len == sizeof(ts->out)) {
            sink(ctx, ts->out, ts->out_len);
            ts->out_len = 0;
        }
    }
}

// 从 TS 包 payload 里解析 PAT，拿第一个节目的 PMT PID。
static bool parse_pat(const uint8_t *p, const size_t n, uint16_t *pmt_out)
{
    if (n < 12) return false;
    const size_t ptr = p[0];
    if (1 + ptr + 8 > n) return false;
    const uint8_t *s = p + 1 + ptr; // section 起
    const size_t sec_len = ((s[1] & 0x0F) << 8) | s[2];
    const size_t body = 8 + sec_len - 4 - 4 - 4; // header+CRC+program_num/reserved 起
    size_t i = 12 + ptr + 0; // s 起第 12 字节起是节目循环（相对 p）
    // s[0..2]=header, s[3..7]=固定 5 字节, 节目循环从 s+8
    i = (size_t)(s - p) + 8;
    const size_t end = (size_t)(s - p) + 8 + sec_len - 4 - 5; // 去 CRC
    while (i + 4 <= end && i + 4 <= n) {
        const uint16_t num = (uint16_t)((p[i] << 8) | p[i + 1]);
        const uint16_t pid = (uint16_t)(((p[i + 2] & 0x1F) << 8) | p[i + 3]);
        if (num != 0) {
            *pmt_out = pid;
            return true;
        }
        i += 4;
    }
    (void)body;
    return false;
}

// 从 PMT payload 里找 stream_type 0x0F（AAC ADTS）的 PID。
static bool parse_pmt(const uint8_t *p, const size_t n, uint16_t *es_out)
{
    if (n < 16) return false;
    const size_t ptr = p[0];
    if (1 + ptr + 12 > n) return false;
    const uint8_t *s = p + 1 + ptr;
    const size_t sec_len = ((s[1] & 0x0F) << 8) | s[2];
    const size_t prog_info_len = ((s[10] & 0x0F) << 8) | s[11];
    size_t i = (size_t)(s - p) + 12 + prog_info_len;
    const size_t end = (size_t)(s - p) + 3 + sec_len - 4; // 去 CRC
    while (i + 5 <= end && i + 5 <= n) {
        const uint8_t stype = p[i];
        const uint16_t pid = (uint16_t)(((p[i + 1] & 0x1F) << 8) | p[i + 2]);
        const size_t es_len = (size_t)(((p[i + 3] & 0x0F) << 8) | p[i + 4]);
        if (stype == 0x0F) {
            *es_out = pid;
            return true;
        }
        i += 5 + es_len;
    }
    return false;
}

// 回退启发式：没等到 PAT/PMT 时，找第一个 payload 以 PES 头开头且净荷以
// ADTS 同步字开始的 PID，直接锁定它。
static bool looks_like_audio_pes(const uint8_t *p, const size_t n)
{
    if (n < 15) return false;
    if (p[0] != 0x00 || p[1] != 0x00 || p[2] != 0x01) return false;
    const size_t hdr = 9 + p[8];
    if (hdr + 2 >= n) return false;
    const uint8_t *es = p + hdr;
    return es[0] == 0xFF && (es[1] & 0xF6) == 0xF0;
}

static void process_packet(cr_ts_t *ts, const uint8_t *pkt,
                           cr_ts_sink_t sink, void *ctx)
{
    const bool pusi = (pkt[1] >> 6) & 1;
    const uint16_t pid = (uint16_t)(((pkt[1] & 0x1F) << 8) | pkt[2]);
    const uint8_t afc = (pkt[3] >> 4) & 3;
    size_t off = 4;
    if (afc & 2) off += 1 + pkt[4]; // adaptation field
    if (!(afc & 1)) return;          // 无 payload
    const uint8_t *payload = pkt + off;
    const size_t len = CR_TS_PACKET - off;

    if (ts->phase == CR_TS_WAIT_PAT) {
        if (pid == 0x0000) {
            uint16_t pmt = 0;
            if (parse_pat(payload, len, &pmt)) {
                ts->pmt_pid = pmt;
                ts->phase = CR_TS_WAIT_PMT;
            }
        } else if (pusi && pid != 0x1FFF && looks_like_audio_pes(payload, len)) {
            // 段里没有 PAT/PMT：直接按启发式锁定。
            ts->audio_pid = pid;
            ts->phase = CR_TS_RUNNING;
        }
        return;
    }
    if (ts->phase == CR_TS_WAIT_PMT) {
        if (pid == ts->pmt_pid) {
            uint16_t es = 0;
            if (parse_pmt(payload, len, &es)) {
                ts->audio_pid = es;
                ts->phase = CR_TS_RUNNING;
            }
        }
        return;
    }

    if (pid != ts->audio_pid) return;
    if (pusi) {
        if (len < 9 || payload[0] != 0x00 || payload[1] != 0x00 ||
            payload[2] != 0x01) {
            return; // 不是合法 PES 开头，丢弃
        }
        const size_t hdr = 9 + payload[8];
        if (hdr >= len) return;
        emit(ts, payload + hdr, len - hdr, sink, ctx);
    } else {
        emit(ts, payload, len, sink, ctx);
    }
}

void cr_ts_init(cr_ts_t *ts)
{
    memset(ts, 0, sizeof(*ts));
}

bool cr_ts_feed(cr_ts_t *ts, const uint8_t *data, size_t n,
                cr_ts_sink_t sink, void *ctx)
{
    size_t pos = 0;
    while (pos < n) {
        if (ts->pkt_fill == 0) {
            // 新包：先找同步字（正常流第一字节就是 0x47）。
            while (pos < n && data[pos] != 0x47) pos++;
            if (pos >= n) return true;
        }
        const size_t take = n - pos < CR_TS_PACKET - ts->pkt_fill
                                ? n - pos
                                : CR_TS_PACKET - ts->pkt_fill;
        memcpy(ts->pkt + ts->pkt_fill, data + pos, take);
        ts->pkt_fill += take;
        pos += take;
        if (ts->pkt_fill == CR_TS_PACKET) {
            if (ts->pkt[0] == 0x47) {
                process_packet(ts, ts->pkt, sink, ctx);
            }
            ts->pkt_fill = 0;
        }
    }
    return true;
}

void cr_ts_flush(cr_ts_t *ts, cr_ts_sink_t sink, void *ctx)
{
    if (ts->out_len > 0) {
        sink(ctx, ts->out, ts->out_len);
        ts->out_len = 0;
    }
    ts->pkt_fill = 0;
}
