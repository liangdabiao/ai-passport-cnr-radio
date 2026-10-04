<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# A-Share Watchlist — Application Record

Text-only application record for the three-key A-share watchlist tool
(`astock-quote`): the data path, implemented rules, network structure and test
coverage. Cover images and store metadata live outside this repository.

## Data path

```
Eastmoney push2.eastmoney.com  (HTTP, UTF-8 JSON)
  └─ sq_net: raw-socket GET (hot-daily device lesson: esp_http_client fails
     internally on C3+lwip; a direct socket is verified to work)
      └─ sq_quote: cJSON parse → snapshot (price/change ×100 fixed point;
         suspended "-" marks the item invalid)
          └─ NVS cache (offline replay) + callback (delivered under the
             LVGL lock to the list page)
```

One request fetches the whole watchlist (comma-separated secids, SH `1.` /
SZ `0.` prefixes). During trading hours (Beijing 09:15–15:05, weekends off)
the list auto-refreshes at the configured interval; the server f124 timestamp
travels with the snapshot into the cache, and the offline list is labelled
"showing cache".

## Rules

- Watchlist cap 8; the same code may coexist in the index and stock domains
  (000001 is both SSE Index 1.000001 and Ping An Bank 0.000001).
- Market inference by first digit: 6/9/5 SH, 0/2/3 SZ; Beijing Stock Exchange
  (4/8) is explicitly rejected in v1.
- Suspended symbols return `"-"` fields, parsed as invalid — the UI shows
  "suspended/no data", never 0.00 pretending to be a quote.
- No holiday calendar: the session check only knows weekends and daily
  windows; on holidays a refresh returns the previous session's data, honestly
  labelled by the server timestamp.

## Provisioning

esp-wifi-connect component (same as hot-daily / english-podcast, verified on
this hardware): Title → "Wi-Fi" → the device opens hotspot **AStock** → the
phone joins it and opens 192.168.4.1 → home Wi-Fi (2.4 GHz) credentials are
stored in NVS with auto-reconnect. The Wi-Fi page can clear credentials and
re-provision. The hot-daily lesson is preserved verbatim: after connecting,
DNS is set explicitly (DHCP DNS stalls the first HTTP request).

## Test coverage

- `test_sq_symbol`: code validation and market inference (BSE rejected).
- `test_sq_quote`: real response shape, suspended placeholders, bad JSON,
  snapshot serialize round-trip (cJSON compiled from tests/cjson).
- `test_sq_market`: session boundaries (opening auction, lunch break,
  after-close, weekends; timegm is not portable — the test uses Hinnant's
  civil-days algorithm instead).
- `test_sq_watch`: presets, add/remove dedup (same code, two domains), cap 8,
  serialize round-trip.
- `test_sq_volume` / `test_sq_wrap`: family-standard.

## Delivery notes

- Fonts: three sizes (16/24/32 px, Noto Sans CJK SC, SIL OFL 1.1), UI subset
  (327 codepoints). Symbols display as "code.market + preset short name"
  (MOUTAI, SSE…), deliberately without dynamic Chinese glyphs — saving about
  1.9 MB of flash.
- No TLS: quotes travel over plain HTTP (public data, no credentials),
  saving ~45 KB of heap and the whole certificate burden.
- Tasks and stacks: input 24 KB, network 16 KB; BLE is never initialized
  (provisioning uses the hotspot).
- Known limitations: no holiday calendar; suspended symbols show "no data";
  the Eastmoney endpoint is unencrypted.
