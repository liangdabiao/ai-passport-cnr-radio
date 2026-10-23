#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "cr_hls.h"

static void feed_all(cr_chunker_t *ck, const char *in, size_t n,
                     uint8_t *out, size_t cap, size_t *produced)
{
    const uint8_t *p = (const uint8_t *)in;
    size_t left = n;
    size_t room = cap;
    *produced = 0;
    // 模拟设备端调用方式：循环喂入直到输入耗尽或输出满。
    while (left > 0 && room > 0) {
        const size_t out_before = room;
        cr_chunker_feed(ck, &p, &left, out + (cap - room), &room);
        *produced += out_before - room;
        if (ck->state == CR_CK_BAD || ck->state == CR_CK_DONE) break;
    }
}

// ================= HLS 播放列表 + ADTS 扫描（radio.cn 实测形态） =================
#include <stdlib.h>

static char *read_file(const char *path, size_t *len_out)
{
    FILE *f = fopen(path, "rb");
    if (!f) {
        // 门禁在仓库根跑、调试可能在 tests/ 跑：两处都试。
        const char *base = strrchr(path, '/');
        if (base) f = fopen(base + 1, "rb");
    }
    if (!f) return NULL;
    static char buf[262144];
    const size_t n = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    *len_out = n;
    return buf;
}

static void test_playlist_and_adts(void)
{
    // 真实抓包：央广中国之声的 m3u8 与一个 10 秒 .ts 切片（tests/ 夹具）。
    size_t pl_len = 0;
    char *pl = read_file("tests/sample_playlist.m3u8", &pl_len);
    assert(pl && pl_len > 50);

    cr_hls_playlist_t p;
    assert(cr_hls_playlist_parse(pl, pl_len, &p));
    assert(p.count >= 1 && p.count <= CR_HLS_MAX_SEGS);
    assert(p.target_dur_s == 10);
    assert(p.media_seq > 5600);   // 抓包时刻的段序号（2026-10 实测 ≈5582）
    for (int i = 0; i < p.count; i++) {
        assert(p.names[i][0] == 's'); // segmentYYYYMMDD…-NNNNN.ts
        assert(strstr(p.names[i], ".ts"));
    }
    // 上限裁剪：给 20 段的列表只保留最新 CR_HLS_MAX_SEGS 段。
    {
        static char big[2048];
        int off = snprintf(big, sizeof(big), "#EXTM3U\n#EXT-X-MEDIA-SEQUENCE:100\n#EXT-X-TARGETDURATION:8\n");
        for (int i = 0; i < 20; i++) off += snprintf(big + off, sizeof(big) - off, "seg%03d.ts\n", i);
        assert(cr_hls_playlist_parse(big, strlen(big), &p));
        assert(p.count == CR_HLS_MAX_SEGS);
        assert(strcmp(p.names[0], "seg008.ts") == 0);
        assert(strcmp(p.names[CR_HLS_MAX_SEGS - 1], "seg019.ts") == 0);
        assert(p.media_seq == 100 + 8);
    }
    // 坏列表：没有 EXTM3U 头。
    assert(!cr_hls_playlist_parse("hello world", 11, &p));

    // ADTS 扫描：真实 TS 切片里抽取 AAC 帧。
    size_t ts_len = 0;
    uint8_t *ts = (uint8_t *)read_file("tests/seg.ts", &ts_len);
    assert(ts && ts_len > 30000);
    assert(ts[0] == 0x47); // MPEG-TS 同步

    size_t pos = 0;
    int frames = 0;
    uint16_t fl = 0;
    while (pos < ts_len && frames < 500) {
        const int off = cr_adts_find(ts + pos, ts_len - pos, &fl);
        if (off < 0) break;
        assert(fl >= 8 && fl <= 8192);
        pos += (size_t)off + fl;
        frames++;
    }
    // 原始 TS 汤里同步扫描只能找到一部分帧（PES 边界打断帧，实测 278/431）；
    // 完整 431 帧由 test_cr_ts（正经解复用）断言，这里只验证扫描器本身可用。
    assert(frames > 200);
    printf("playlist+adts OK (%d frames in %zu bytes)\n", frames, ts_len);
}

int main(void)
{
    // ---------------- 头部扫描 ----------------
    const char *full =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: audio/mpeg\r\n"
        "icy-br:128\r\n"
        "\r\nBODY";
    assert(cr_hls_head_scan(full, strlen(full), 2048) == strlen(full) - 4);

    // 半截头：未完成返回 0。
    assert(cr_hls_head_scan("HTTP/1.1 200 OK\r\nCon", 19, 2048) == 0);

    // 上限兜底：缓冲超限还没找到终点。
    char junk[600];
    memset(junk, 'A', sizeof(junk));
    assert(cr_hls_head_scan(junk, sizeof(junk), 256) == SIZE_MAX);

    // ---------------- 头部解析 ----------------
    cr_hls_head_t h;
    assert(cr_hls_head_parse(full, strlen(full) - 4, &h));
    assert(h.status == 200);
    assert(!h.chunked);
    assert(h.content_length == -1);
    assert(strcmp(h.content_type, "audio/mpeg") == 0);
    assert(h.icy_br == 128);
    assert(h.icy_name[0] == '\0');

    const char *icy =
        "HTTP/1.0 200 OK\r\n"
        "content-type: AUDIO/MPEG;codec=MP3\r\n"
        "ICY-Name:CNR-1 中国之声\r\n"
        "icy-br: 64 \r\n"
        "Content-Length: 123456\r\n"
        "\r\n";
    assert(cr_hls_head_parse(icy, strlen(icy) - 2, &h));
    assert(strcmp(h.content_type, "audio/mpeg") == 0); // 小写化 + 截分号
    assert(strcmp(h.icy_name, "CNR-1 中国之声") == 0);
    assert(h.icy_br == 64);
    assert(h.content_length == 123456);

    const char *redir =
        "HTTP/1.1 302 Found\r\n"
        "Location: http://a.example.com/live.mp3\r\n"
        "Content-Length: 0\r\n"
        "\r\n";
    assert(cr_hls_head_parse(redir, strlen(redir) - 2, &h));
    assert(h.status == 302);
    assert(strcmp(h.location, "http://a.example.com/live.mp3") == 0);

    // 100-Continue 后跟真头部：扫描应找到第一段；解析失败后调用方跳过。
    const char *cont =
        "HTTP/1.1 100 Continue\r\n\r\n"
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: audio/aac\r\n"
        "\r\n";
    const size_t first = cr_hls_head_scan(cont, strlen(cont), 2048);
    assert(first == 25);
    assert(cr_hls_head_parse(cont, first, &h));
    assert(h.status == 100);
    // 跳过 1xx 后再解析第二段。
    const char *second = cont + first;
    assert(cr_hls_head_parse(second, strlen(second) - 2, &h));
    assert(h.status == 200);
    assert(strcmp(h.content_type, "audio/aac") == 0);

    // 坏头部。
    assert(!cr_hls_head_parse("NOT HTTP", 8, &h));
    assert(!cr_hls_head_parse("HTTP/1.1 abc\r\n\r\n", 16, &h));
    assert(!cr_hls_head_parse(full, 8, &h));

    // ---------------- chunked：整段 ----------------
    cr_chunker_t ck;
    uint8_t out[256];
    size_t produced;
    const char *chunked = "4\r\nWiki\r\n5\r\npedia\r\n0\r\n\r\n";
    cr_chunker_init(&ck);
    feed_all(&ck, chunked, strlen(chunked), out, sizeof(out), &produced);
    assert(ck.state == CR_CK_DONE);
    assert(produced == 9);
    assert(memcmp(out, "Wikipedia", 9) == 0);

    // ---------------- chunked：逐字节喂（分片最恶劣形态）----------------
    cr_chunker_init(&ck);
    produced = 0;
    size_t room = sizeof(out);
    for (size_t i = 0; i < strlen(chunked); i++) {
        const uint8_t *p = (const uint8_t *)&chunked[i];
        size_t one = 1;
        cr_chunker_feed(&ck, &p, &one, out + produced, &room);
        produced = sizeof(out) - room;
        assert(ck.state != CR_CK_BAD);
    }
    assert(ck.state == CR_CK_DONE);
    assert(produced == 9);
    assert(memcmp(out, "Wikipedia", 9) == 0);

    // ---------------- chunked：输出容量不足（环满续传语义）----------------
    cr_chunker_init(&ck);
    produced = 0;
    memset(out, 0, sizeof(out));
    {
        const uint8_t *p = (const uint8_t *)chunked;
        size_t left = strlen(chunked);
        size_t room2 = 5; // 只给 5 字节空间
        while (left > 0 && room2 > 0) {
            cr_chunker_feed(&ck, &p, &left, out + produced, &room2);
        }
        assert(produced == 5 - room2 || room2 == 0);
        // 第一轮只产出 5 字节（"Wikipe"截断为5）。
        assert(room2 == 0);
        assert(memcmp(out, "Wikip", 5) == 0);
        assert(ck.state == CR_CK_DATA);
        // 继续给空间：剩余全部产出。
        room2 = sizeof(out) - 5;
        while (left > 0 && room2 > 0) {
            cr_chunker_feed(&ck, &p, &left, out + 5, &room2);
        }
        assert(left == 0);
        assert(memcmp(out, "Wikipedia", 9) == 0);
        assert(ck.state == CR_CK_DONE);
    }

    // ---------------- chunked：坏流 ----------------
    const char *bad = "Z\r\nWiki\r\n";
    cr_chunker_init(&ck);
    feed_all(&ck, bad, strlen(bad), out, sizeof(out), &produced);
    assert(ck.state == CR_CK_BAD);

    // chunk 扩展（v1 不支持）：';' 后非法。
    const char *ext = "4;name=v\r\nWiki\r\n0\r\n\r\n";
    cr_chunker_init(&ck);
    feed_all(&ck, ext, strlen(ext), out, sizeof(out), &produced);
    assert(ck.state == CR_CK_BAD);

    // 尾注：0 长度后带 trailer 头，滚动窗口扫到空行结束。
    const char *trailer = "5\r\nhello\r\n0\r\nX-T: 1\r\n\r\nTAIL";
    cr_chunker_init(&ck);
    feed_all(&ck, trailer, strlen(trailer), out, sizeof(out), &produced);
    assert(ck.state == CR_CK_DONE);
    assert(produced == 5);
    assert(memcmp(out, "hello", 5) == 0);

    test_playlist_and_adts();
    printf("test_cr_hls OK\n");
    return 0;
}
