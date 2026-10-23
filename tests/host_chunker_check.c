// 宿主验证：用真实抓包的 chunked 流跑 nr_chunker_feed，输出净荷供逐字节比对。
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nr_http.h"

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s raw.bin out.bin\n", argv[0]);
        return 2;
    }
    FILE *f = fopen(argv[1], "rb");
    if (!f) return 1;
    static unsigned char raw[400000];
    const size_t n = fread(raw, 1, sizeof(raw), f);
    fclose(f);

    // 跳过头部
    const size_t head = nr_http_head_scan((const char *)raw, n, sizeof(raw));
    if (head == 0 || head == SIZE_MAX) {
        printf("bad header scan\n");
        return 1;
    }
    nr_http_head_t h;
    if (!nr_http_head_parse((const char *)raw, head, &h)) {
        printf("bad header parse\n");
        return 1;
    }
    printf("status=%d chunked=%d\n", h.status, h.chunked);

    nr_chunker_t ck;
    nr_chunker_init(&ck);
    const uint8_t *in = raw + head;
    size_t left = n - head;
    static unsigned char out[400000];
    size_t total = 0;
    // 模拟设备：每次给 2048 字节输入、4096 字节输出空间，反复调用
    while (left > 0) {
        size_t feed = left > 2048 ? 2048 : left;
        const uint8_t *p = in;
        size_t room = sizeof(out) - total;
        size_t cap = room > 4096 ? 4096 : room;
        const size_t cap_before = cap;
        nr_chunker_feed(&ck, &p, &feed, out + total, &cap);
        total += cap_before - cap;
        const size_t consumed = (size_t)(p - in);
        in += consumed;
        left -= consumed;
        if (ck.state == NR_CK_BAD) {
            printf("BAD at remaining=%zu (offset %zu)\n", left, (size_t)(in - raw));
            break;
        }
        if (ck.state == NR_CK_DONE) {
            printf("DONE\n");
            break;
        }
        if (consumed == 0 && (cap_before - cap) == 0) {
            printf("stall at offset %zu\n", (size_t)(in - raw));
            break;
        }
    }
    printf("decoded %zu bytes, state=%d, left=%zu\n", total, (int)ck.state, left);
    FILE *o = fopen(argv[2], "wb");
    fwrite(out, 1, total, o);
    fclose(o);
    return 0;
}
