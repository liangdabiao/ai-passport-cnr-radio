// 宿主验证：cr_ts 解复用真实央广切片，与 Python 参考实现逐字节比对。
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cr_ts.h"

static uint8_t out[262144];
static size_t out_len;

static void sink(void *ctx, const uint8_t *data, size_t n)
{
    (void)ctx;
    memcpy(out + out_len, data, n);
    out_len += n;
}

static size_t read_file(const char *path, uint8_t *buf, size_t cap)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    const size_t n = fread(buf, 1, cap, f);
    fclose(f);
    return n;
}

int main(int argc, char **argv)
{
    static uint8_t seg[262144];
    const char *seg_path = argc > 1 ? argv[1] : "seg.ts";
    const size_t seg_len = read_file(seg_path, seg, sizeof(seg));
    if (!seg_len) {
        printf("cannot read %s\n", seg_path);
        return 2;
    }

    // 碎喂：7 字节一块（最恶劣的分片形态）
    cr_ts_t ts;
    cr_ts_init(&ts);
    for (size_t i = 0; i < seg_len; i += 7) {
        const size_t take = seg_len - i < 7 ? seg_len - i : 7;
        if (!cr_ts_feed(&ts, seg + i, take, sink, NULL)) {
            printf("feed failed\n");
            return 1;
        }
    }
    cr_ts_flush(&ts, sink, NULL);
    printf("demux ES=%zu bytes, audio_pid=%d\n", out_len, ts.audio_pid);

    // 与参考（ts_demux_check.py 生成的 seg_adts.bin）比对
    static uint8_t ref[262144];
    const size_t ref_len = read_file(argc > 2 ? argv[2] : "seg_adts.bin", ref, sizeof(ref));
    if (ref_len == 0) {
        printf("reference missing, frames counted only\n");
    } else if (ref_len == out_len && memcmp(ref, out, ref_len) == 0) {
        printf("IDENTICAL to python reference (%zu bytes)\n", ref_len);
    } else {
        printf("MISMATCH: c=%zu py=%zu\n", out_len, ref_len);
        return 1;
    }

    // 数帧
    size_t pos = 0;
    int frames = 0;
    while (pos + 7 <= out_len) {
        if (out[pos] == 0xFF && (out[pos + 1] & 0xF6) == 0xF0) {
            const unsigned fl = ((out[pos + 3] & 3) << 11) | (out[pos + 4] << 3) |
                                (out[pos + 5] >> 5);
            if (fl >= 8 && fl <= 8192 && pos + fl <= out_len) {
                pos += fl;
                frames++;
                continue;
            }
        }
        pos++;
    }
    printf("frames=%d (expect 431)\n", frames);
    return frames == 431 ? 0 : 1;
}
