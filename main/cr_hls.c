// main/cr_hls.c —— 见 cr_hls.h。
#include "cr_hls.h"

#include <stdlib.h>
#include <strings.h>

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

size_t cr_hls_head_scan(const char *buf, const size_t have, const size_t cap)
{
    if (!buf) return 0;
    // 头部终点是 "\r\n\r\n"。真头部远小于上限；超过还找不到 = 流损坏。
    for (size_t i = 0; i + 4 <= have; i++) {
        if (buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] == '\r' &&
            buf[i + 3] == '\n') {
            return i + 4;
        }
        if (i > cap) return SIZE_MAX;
    }
    return have > cap ? SIZE_MAX : 0;
}

// 头部行取值：在 [head, head+len) 里找 "Name: value"（跳过状态行）。
// 返回 value 的 [指针, 长度)，去尾部空白；没找到返回 false。
static bool header_find(const char *head, size_t len, const char *name,
                        const char **value, size_t *value_len)
{
    const size_t name_len = strlen(name);
    const char *nl = memchr(head, '\n', len);
    if (!nl) return false;
    size_t pos = (size_t)(nl - head) + 1; // 跳过状态行

    while (pos < len) {
        const char *line = head + pos;
        const char *end = memchr(line, '\n', len - pos);
        const size_t line_len = end ? (size_t)(end - line) : len - pos;
        if (line_len == 0) break;
        if (line_len > name_len && line[name_len] == ':' &&
            strncasecmp(line, name, name_len) == 0) {
            const char *p = line + name_len + 1;
            const char *line_end = line + line_len;
            while (p < line_end && (*p == ' ' || *p == '\t')) p++;
            const char *v_end = line_end;
            while (v_end > p && (v_end[-1] == '\r' || v_end[-1] == ' ')) v_end--;
            *value = p;
            *value_len = (size_t)(v_end - p);
            return true;
        }
        pos += line_len + (end ? 1 : 0);
    }
    return false;
}

static bool copy_header(const char *head, size_t len, const char *name,
                        char *out, size_t cap)
{
    out[0] = '\0';
    const char *v;
    size_t vlen;
    if (!header_find(head, len, name, &v, &vlen)) return false;
    if (vlen >= cap) vlen = cap - 1;
    memcpy(out, v, vlen);
    out[vlen] = '\0';
    return true;
}

bool cr_hls_head_parse(const char *head, const size_t len, cr_hls_head_t *out)
{
    if (!head || len < 12 || !out) return false;
    memset(out, 0, sizeof(*out));
    out->content_length = -1;

    // 状态行："HTTP/1.x 200 OK\r\n"。
    if (strncmp(head, "HTTP/", 5) != 0) return false;
    const char *sp = memchr(head, ' ', len);
    if (!sp) return false;
    out->status = atoi(sp + 1);
    if (out->status < 100 || out->status >= 600) return false;

    char buf[64];
    if (copy_header(head, len, "Transfer-Encoding", buf, sizeof(buf))) {
        for (char *p = buf; *p; p++) *p = (char)tolower((unsigned char)*p);
        out->chunked = strstr(buf, "chunked") != NULL;
    }
    if (copy_header(head, len, "Content-Type", out->content_type,
                    sizeof(out->content_type))) {
        for (char *p = out->content_type; *p; p++) {
            if (*p == ';') { *p = '\0'; break; }
            *p = (char)tolower((unsigned char)*p);
        }
    }
    (void)copy_header(head, len, "Location", out->location, sizeof(out->location));
    (void)copy_header(head, len, "icy-name", out->icy_name, sizeof(out->icy_name));
    if (copy_header(head, len, "Content-Length", buf, sizeof(buf))) {
        out->content_length = atoll(buf);
    }
    if (copy_header(head, len, "icy-br", buf, sizeof(buf))) {
        out->icy_br = atoi(buf);
        if (out->icy_br < 0) out->icy_br = 0;
    }
    return true;
}

// -------------------------------------------------------------- chunked ----
// 状态图：
//   SIZE --hex--> SIZE        ';'/其他非法 --> BAD（v1 不支持 chunk 扩展）
//   SIZE --'\r'--> SIZE_LF    非法 --> BAD
//   SIZE_LF --'\n'--> size>0 ? DATA(remain=size) : TRAILER
//   DATA --拷贝 remain 字节--> remain==0 后等 DATA_CR
//   DATA_CR --'\r'--> DATA_LF --'\n'--> SIZE
//   TRAILER --滚动窗口扫到 "\r\n\r\n"--> DONE（不再消费）
//   任何状态收到非法字节 --> BAD

void cr_chunker_init(cr_chunker_t *c)
{
    c->state = CR_CK_SIZE;
    c->remain = 0;
    c->size_acc = 0;
    c->size_digits = 0;
    c->window = 0x0d0a; // 尾注检测窗口预置 0 长度行结尾的 "\r\n"
}

static int hexval(const uint8_t ch)
{
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}

void cr_chunker_feed(cr_chunker_t *c, const uint8_t **in, size_t *n,
                     uint8_t *out, size_t *cap)
{
    while (*n > 0 && *cap > 0) {
        const uint8_t ch = **in;
        switch (c->state) {
            case CR_CK_SIZE: {
                const int hv = hexval(ch);
                if (hv >= 0) {
                    if (++c->size_digits > 8) { c->state = CR_CK_BAD; return; }
                    c->size_acc = c->size_acc * 16u + (uint64_t)hv;
                    (*in)++; (*n)--;
                } else if (ch == '\r') {
                    if (c->size_digits == 0) { c->state = CR_CK_BAD; return; }
                    c->state = CR_CK_SIZE_LF;
                    (*in)++; (*n)--;
                } else {
                    c->state = CR_CK_BAD; // 含 ';'：chunk 扩展不支持
                    return;
                }
                break;
            }
            case CR_CK_SIZE_LF:
                if (ch != '\n') { c->state = CR_CK_BAD; return; }
                (*in)++; (*n)--;
                if (c->size_acc == 0) {
                    c->state = CR_CK_TRAILER;
                } else {
                    c->remain = (size_t)c->size_acc;
                    c->size_acc = 0;
                    c->size_digits = 0;
                    c->state = CR_CK_DATA;
                }
                break;
            case CR_CK_DATA: {
                const size_t move = c->remain < *n ? c->remain : *n;
                const size_t room = *cap < move ? *cap : move;
                if (room > 0) {
                    memcpy(out, *in, room);
                    out += room;
                    *cap -= room;
                    (*in) += room;
                    (*n) -= room;
                    c->remain -= room;
                }
                if (c->remain == 0 && room == move) {
                    // 数据段消费完（输出满时 room<move，留到下轮）。
                    if (*n > 0) {
                        if (**in != '\r') { c->state = CR_CK_BAD; return; }
                        (*in)++; (*n)--;
                        c->state = CR_CK_DATA_LF;
                    } else {
                        c->state = CR_CK_DATA_CR; // 输入耗尽，等下一批的 '\r'
                    }
                }
                break;
            }
            case CR_CK_DATA_CR:
                // 数据段刚消费完，等 '\r'。
                if (ch != '\r') { c->state = CR_CK_BAD; return; }
                (*in)++; (*n)--;
                c->state = CR_CK_DATA_LF;
                break;
            case CR_CK_DATA_LF:
                // DATA_CR 的镜像：'\r' 已收，等 '\n'。
                if (ch != '\n') { c->state = CR_CK_BAD; return; }
                (*in)++; (*n)--;
                c->state = CR_CK_SIZE;
                break;
            case CR_CK_TRAILER: {
                // 滚动 4 字节窗口，扫到 "\r\n\r\n" 即尾注结束。
                c->window = (c->window << 8) | ch;
                (*in)++; (*n)--;
                if ((c->window & 0xffffffffULL) == 0x0d0a0d0aULL) {
                    c->state = CR_CK_DONE;
                    return;
                }
                break;
            }
            case CR_CK_DONE:
                return; // 不再消费：调用方随后关闭连接
            case CR_CK_BAD:
                return;
        }
    }
}

// -------------------------------------------------------------- m3u8 ----
static void pl_keep_latest(cr_hls_playlist_t *pl, const char *name, size_t len)
{
    if (pl->count == CR_HLS_MAX_SEGS) {
        memmove(pl->names[0], pl->names[1],
                sizeof(pl->names) - sizeof(pl->names[0]));
        pl->count--;
        pl->media_seq++;
    }
    if (len >= sizeof(pl->names[0])) len = sizeof(pl->names[0]) - 1;
    memcpy(pl->names[pl->count], name, len);
    pl->names[pl->count][len] = '\0';
    pl->count++;
}

bool cr_hls_playlist_parse(const char *text, size_t len, cr_hls_playlist_t *out)
{
    memset(out, 0, sizeof(*out));
    if (len < 7 || memcmp(text, "#EXTM3U", 7) != 0) return false;

    size_t pos = 0;
    while (pos < len) {
        size_t eol = pos;
        while (eol < len && text[eol] != '\n') eol++;
        size_t line_len = eol - pos;
        if (line_len > 0 && text[pos + line_len - 1] == '\r') line_len--;

        if (line_len > 21 && memcmp(text + pos, "#EXT-X-MEDIA-SEQUENCE:", 22) == 0) {
            out->media_seq = (uint32_t)strtoul(text + pos + 22, NULL, 10);
        } else if (line_len > 21 && memcmp(text + pos, "#EXT-X-TARGETDURATION:", 22) == 0) {
            out->target_dur_s = (uint8_t)strtoul(text + pos + 22, NULL, 10);
        } else if (line_len > 0 && text[pos] != '#') {
            pl_keep_latest(out, text + pos, line_len);
        }
        pos = eol + 1;
    }
    return out->count > 0;
}

// ------------------------------------------------------------- ADTS ----
int cr_adts_find(const uint8_t *buf, size_t n, uint16_t *frame_len_out)
{
    for (size_t i = 0; i + 7 <= n; i++) {
        if (buf[i] != 0xFF || (buf[i + 1] & 0xF6) != 0xF0) continue;
        const uint8_t sr_idx = (buf[i + 2] >> 2) & 0x0F;
        const uint8_t ch = ((buf[i + 2] & 0x01) << 2) | (buf[i + 3] >> 6);
        if (sr_idx >= 13 || ch < 1 || ch > 7) continue;
        const uint32_t fl = ((uint32_t)(buf[i + 3] & 0x03) << 11) |
                            ((uint32_t)buf[i + 4] << 3) | (buf[i + 5] >> 5);
        if (fl < 8 || fl > 8192 || i + fl > n) continue;
        *frame_len_out = (uint16_t)fl;
        return (int)i;
    }
    return -1;
}
