#!/usr/bin/env python3
"""radio-browser 电台候选扫描：按分类搜索 → 逐台实测（协议/存活/icy 元数据）→ TSV 报告。

用法: python tools/station_scan.py
输出: tools/station_scan.tsv（name \t category \t votes \t protocol \t codec \t bitrate \t url）
只在本机（与设备同一网络出口）做一次性筛选用，设备端不参与。
"""
import json
import subprocess
import sys
import time

API = "https://de1.api.radio-browser.info/json/stations/search"

SEARCHES = [
    ("新闻", {"countrycode": "CN", "codec": "MP3", "order": "votes", "reverse": "true", "limit": 25, "hidebroken": "true"}),
    ("音乐", {"countrycode": "CN", "codec": "MP3", "tag": "music", "order": "votes", "reverse": "true", "limit": 25, "hidebroken": "true"}),
    ("曲艺", {"name": "相声", "codec": "MP3", "order": "votes", "reverse": "true", "limit": 15, "hidebroken": "true"}),
    ("曲艺2", {"name": "评书", "codec": "MP3", "order": "votes", "reverse": "true", "limit": 15, "hidebroken": "true"}),
    ("故事", {"name": "故事", "countrycode": "CN", "order": "votes", "reverse": "true", "limit": 15, "hidebroken": "true"}),
    ("外语", {"countrycode": "US", "codec": "MP3", "tag": "jazz", "order": "votes", "reverse": "true", "limit": 10, "hidebroken": "true"}),
    ("外语2", {"name": "SomaFM", "codec": "MP3", "order": "votes", "reverse": "true", "limit": 15, "hidebroken": "true"}),
]


def api(params):
    from urllib.parse import urlencode
    url = API + "?" + urlencode(params)
    for attempt in range(3):
        try:
            out = subprocess.run(
                ["curl", "-s", "--max-time", "25", "-H", "User-Agent: net-radio-scan/1.0", url],
                capture_output=True, text=True, timeout=30, encoding="utf-8", errors="replace")
            return json.loads(out.stdout)
        except Exception as e:
            print(f"  api retry {attempt}: {e}", file=sys.stderr)
            time.sleep(2)
    return []


def probe(url):
    """返回 (ok_protocol, content_type, icy_name, icy_br)。ok_protocol = 纯 http 可用。"""
    for attempt in range(2):
        try:
            out = subprocess.run(
                ["curl", "-s", "-o", "NUL" if sys.platform == "win32" else "/dev/null",
                 "--max-time", "10", "--max-filesize", "200000",
                 "-w", "%{http_code}\t%{content_type}\t%{url_effective}",
                 "-H", "User-Agent: VLC/3.0 LibVLC/3.0", "-H", "Icy-MetaData: 0",
                 "-D", "-", "-L", url],
                capture_output=True, text=True, timeout=14, encoding="utf-8", errors="replace")
            headers, status_line = out.stdout.split("\r\n\r\n" if "\r\n\r\n" in out.stdout else "\n\n", 1)
            code = ctype = effective = ""
            for line in out.stdout.splitlines():
                if line.startswith("HTTP/"):
                    code = line.split()[1]
                elif line.lower().startswith("content-type:"):
                    ctype = line.split(":", 1)[1].strip()
            # -w 结果在 stdout 末尾（body 重定向到 NUL 后 -w 仍打印）
            tail = [l for l in out.stdout.splitlines() if "\t" in l]
            if tail:
                parts = tail[-1].split("\t")
                code, ctype, effective = parts[0], parts[1], parts[2] if len(parts) > 2 else ""
            icy_name = icy_br = ""
            for line in out.stdout.splitlines():
                low = line.lower()
                if low.startswith("icy-name:"):
                    icy_name = line.split(":", 1)[1].strip()
                elif low.startswith("icy-br:"):
                    icy_br = line.split(":", 1)[1].strip()
            if code == "200" and ("mpeg" in ctype or "octet" in ctype or "aac" in ctype):
                return ("http", ctype, icy_name, icy_br)
            return (f"http-{code}", ctype, icy_name, icy_br)
        except Exception:
            continue
    return ("timeout", "", "", "")


def main():
    seen = {}
    rows = []
    for cat, params in SEARCHES:
        stations = api(params)
        print(f"[{cat}] {len(stations)} candidates", file=sys.stderr)
        for s in stations:
            url = s.get("url_resolved") or s.get("url") or ""
            if not url or url in seen:
                continue
            seen[url] = True
            rows.append({
                "name": (s.get("name") or "").strip()[:24],
                "cat": cat,
                "votes": s.get("votes", 0),
                "url": url,
            })
    out_path = "tools/station_scan.tsv"
    with open(out_path, "w", encoding="utf-8") as f:
        f.write("name\tcat\tvotes\tproto\tctype\ticy_br\turl\n")
        for r in rows:
            proto, ctype, _name, br = probe(r["url"])
            f.write(f"{r['name']}\t{r['cat']}\t{r['votes']}\t{proto}\t{ctype}\t{br}\t{r['url']}\n")
            f.flush()
            print(f"  {proto:9s} {r['name']}", file=sys.stderr)
    print(f"done -> {out_path}", file=sys.stderr)


if __name__ == "__main__":
    main()
