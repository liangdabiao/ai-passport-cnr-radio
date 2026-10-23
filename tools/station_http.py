#!/usr/bin/env python3
"""第二轮精选：对第一轮候选做「纯 http GET 收流」实测。

要点：第一轮的 proto 列不可靠（curl -L 的 -D 头里有多段响应，脚本取了最后一段
的状态码，TLS 也会被标成 200）。这一轮把 URL 强制改写成 http:// 后真 GET
收流 8 秒，用实收字节数判定可用 —— 与设备端 raw-socket 行为一致。
"""
import concurrent.futures
import subprocess
import sys

IN_PATH = "tools/station_scan.tsv"
OUT_PATH = "tools/station_http.tsv"
HEADERS = ["-H", "User-Agent: VLC/3.0 LibVLC/3.0", "-H", "Icy-MetaData: 0"]


def probe(row):
    name, cat, votes, _proto, _ctype, _br, url = row
    url_http = "http://" + url.split("://", 1)[1]
    try:
        out = subprocess.run(
            ["curl", "-s", "-o", "NUL", "--max-time", "8", "--max-filesize", "400000",
             "-w", "%{http_code}\t%{content_type}\t%{size_download}", "-D", "-",
             "-L", "--max-redirs", "3", *HEADERS, url_http],
            capture_output=True, text=True, timeout=14,
            encoding="utf-8", errors="replace")
        code, ctype, size = "", "", "0"
        for line in out.stdout.splitlines():
            if "\t" in line:
                parts = line.split("\t")
                if len(parts) == 3:
                    code, ctype, size = parts
            low = line.lower()
        icy_name = icy_br = ""
        for line in out.stdout.splitlines():
            low = line.lower()
            if low.startswith("icy-name:"):
                icy_name = line.split(":", 1)[1].strip()[:40]
            elif low.startswith("icy-br:"):
                icy_br = line.split(":", 1)[1].strip()
        try:
            n = int(size)
        except ValueError:
            n = 0
        ok = code == "200" and n >= 32768
        return (name, cat, code, ctype, n, icy_br, icy_name, url_http, "OK" if ok else "-")
    except Exception as e:
        return (name, cat, "ERR", str(e)[:30], 0, "", "", url_http, "-")


def main():
    rows = []
    with open(IN_PATH, encoding="utf-8") as f:
        next(f)
        for line in f:
            parts = line.rstrip("\n").split("\t")
            if len(parts) == 7:
                rows.append(tuple(parts))
    with concurrent.futures.ThreadPoolExecutor(max_workers=10) as pool:
        results = list(pool.map(probe, rows))
    results.sort(key=lambda r: (r[1], -r[4]))
    with open(OUT_PATH, "w", encoding="utf-8") as f:
        f.write("name\tcat\tcode\tctype\tbytes\tbr\ticy_name\turl_http\tverdict\n")
        for r in results:
            f.write("\t".join(str(x) for x in r) + "\n")
    ok_count = sum(1 for r in results if r[8] == "OK")
    print(f"{ok_count}/{len(results)} 个台纯 http 直连可用 -> {OUT_PATH}", file=sys.stderr)


if __name__ == "__main__":
    main()
