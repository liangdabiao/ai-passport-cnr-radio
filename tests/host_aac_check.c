// 宿主验证：真实央广 TS 切片 → helix-aac 解码（检验采样率/帧长/声道/时长）。
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aacdec.h"

// 与 cr_hls.c 相同的 ADTS 校验逻辑（独立复刻，避免依赖设备侧文件）
static int adts_find(const unsigned char *b, size_t n, unsigned *fl_out)
{
    for (size_t i = 0; i + 7 <= n; i++) {
        if (b[i] != 0xFF || (b[i + 1] & 0xF6) != 0xF0) continue;
        unsigned sri = (b[i + 2] >> 2) & 0xF;
        unsigned ch = ((b[i + 2] & 1) << 2) | (b[i + 3] >> 6);
        unsigned fl = ((b[i + 3] & 3) << 11) | (b[i + 4] << 3) | (b[i + 5] >> 5);
        if (sri < 13 && ch >= 1 && ch <= 7 && fl >= 8 && fl <= 8192 && i + fl <= n) {
            *fl_out = fl;
            return (int)i;
        }
    }
    return -1;
}

static unsigned char arena[40 * 1024] __attribute__((aligned(8)));

int main(int argc, char **argv)
{
    FILE *f = fopen(argv[1], "rb");
    if (!f) return 2;
    static unsigned char ts[262144];
    size_t n = fread(ts, 1, sizeof(ts), f);
    fclose(f);
    printf("segment %zu bytes\n", n);

    HAACDecoder dec = AACInitDecoderPre(arena, sizeof(arena));
    if (!dec) {
        printf("AACInitDecoderPre FAILED (arena too small?)\n");
        return 1;
    }
    printf("decoder inited from static arena\n");

    static short pcm[2048 * 2];
    size_t pos = 0;
    int frames = 0, errors = 0;
    long long samples = 0;
    int last_hz = 0, last_ch = 0;
    while (pos < n) {
        unsigned fl = 0;
        int off = adts_find(ts + pos, n - pos, &fl);
        if (off < 0) break;
        unsigned char *in = ts + pos + off;
        int left = (int)(n - pos - off);
        int err = AACDecode(dec, &in, &left, pcm);
        if (err != 0) {
            errors++;
            pos += (size_t)off + 2;
            continue;
        }
        pos += (size_t)off + fl;
        AACFrameInfo fi;
        AACGetLastFrameInfo(dec, &fi);
        samples += fi.outputSamps;
        last_hz = fi.sampRateOut;
        last_ch = fi.nChans;
        if (frames < 3) {
            printf("frame%d: out=%d samps %dHz %dch sbr=%d tns=%d pns=%d\n",
                   frames, fi.outputSamps, fi.sampRateOut, fi.nChans,
                   0, fi.tnsUsed, fi.pnsUsed);
        }
        frames++;
    }
    printf("frames=%d errors=%d last=%dHz %dch total_samples=%lld ≈ %.1fs\n",
           frames, errors, last_hz, last_ch, samples,
           last_hz ? (double)samples / last_hz : 0.0);
    return frames > 0 && errors < frames / 10 ? 0 : 1;
}
