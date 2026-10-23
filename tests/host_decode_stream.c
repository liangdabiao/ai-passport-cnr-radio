// 宿主测试：用真实电台流验证 helix 解码与帧处理（不含设备端代码）。
// 编译：gcc -O2 -std=c11 -Icomponents/libhelix_mp3 -Icomponents/libhelix_mp3 \
//          tests_host_decode.c components/libhelix_mp3/*.c -o /tmp/dec
// 用法：/tmp/dec stream.mp3
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mp3dec.h"

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "usage: %s stream.mp3\n", argv[0]);
        return 2;
    }
    FILE *f = fopen(argv[1], "rb");
    if (!f) {
        perror("open");
        return 1;
    }
    static unsigned char in[65536];
    const size_t n = fread(in, 1, sizeof(in), f);
    fclose(f);
    printf("read %zu bytes\n", n);

    HMP3Decoder dec = MP3InitDecoder();
    if (!dec) {
        printf("MP3InitDecoder FAILED\n");
        return 1;
    }

    unsigned char *p = in;
    int left = (int)n;
    static short pcm[1152 * 2];
    int frames = 0, errors = 0;
    long rms_sum = 0;
    int first_sync = MP3FindSyncWord(p, left);
    printf("first sync at %d\n", first_sync);
    if (first_sync >= 0) {
        p += first_sync;
        left -= first_sync;
    }

    int max_amp = 0;
    for (;;) {
        const int pre = left;
        const int err = MP3Decode(dec, &p, &left, pcm, 0);
        if (err != 0) {
            if (err == ERR_MP3_INDATA_UNDERFLOW || err == ERR_MP3_MAINDATA_UNDERFLOW) break;
            errors++;
            const int s = MP3FindSyncWord(p + 2, left - 2);
            if (s < 0) break;
            p += 2 + s;
            left -= 2 + s;
            continue;
        }
        MP3FrameInfo fi;
        MP3GetLastFrameInfo(dec, &fi);
        long sum = 0;
        int amp = 0;
        for (int i = 0; i < fi.outputSamps; i++) {
            const int v = pcm[i];
            sum += (long)v * v;
            if (abs(v) > amp) amp = abs(v);
        }
        if (amp > max_amp) max_amp = amp;
        rms_sum += sum;
        if (frames < 3) {
            printf("frame %d: %dHz %dch %dbps out=%d maxamp=%d consumed=%d\n",
                   frames, fi.samprate, fi.nChans, fi.bitrate, fi.outputSamps, amp,
                   pre - left);
        }
        frames++;
        if (frames >= 200) break;
    }
    printf("frames=%d errors=%d max_amp=%d\n", frames, errors, max_amp);
    MP3FreeDecoder(dec);
    return frames > 0 && max_amp > 0 ? 0 : 1;
}
