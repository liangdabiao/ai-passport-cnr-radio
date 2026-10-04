<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# A-Share Watchlist — offline-friendly quotes on a three-key handheld

An A-share watchlist tool for the device: after one-time Wi-Fi provisioning,
your watchlist quotes auto-refresh during trading hours, red for up and green
for down. Up to 8 symbols, the three major indices built in, one batched
Eastmoney request per refresh. Works offline too — cached quotes with their
timestamps when the network is away.

| Title | Watchlist | Detail |
| --- | --- | --- |
| Headline, network status, four menu items. | One row per symbol: name, code, price and change%, red up / green down, gold-framed selection. | Big price with change, plus open / prev-close / high / low / quote time. |

## Quick start

1. First run: Title → "Wi-Fi" → connect your phone to the device hotspot
   **AStock** → open 192.168.4.1 → enter your home Wi-Fi (2.4 GHz). The device
   joins and remembers.
2. Title → "Watchlist" for quotes; auto-refresh every 30 s during trading
   hours (09:15–15:05 on trading days).
3. "Add" enters a 6-digit code (SH/SZ auto-detected, up to 8 symbols).
4. Up/Down selects a row, OK opens the detail; long-press OK refreshes now;
   long-press Down removes the symbol.
5. "Settings" switches 15/30/60 s; long-press OK twice returns to the title.

## Build

An [ESP-IDF](https://github.com/espressif/esp-idf) 5.5.3 project targeting the
FoloToy AI Passport (ESP32-C3, 240x320 ST7789, three-key ADC input).

```bash
idf.py build
idf.py flash monitor
./tools/validate.sh
```

## Documentation

- [`docs/reference/liangdabiao/astock-quote/README.md`](docs/reference/liangdabiao/astock-quote/README.md) — application record.
- [`AGENTS.md`](AGENTS.md) — repository rules.
- [`assets/README.md`](assets/README.md) — font provenance and licence.

## Origin and licence

A FoloToy AI Passport family app: Sanzijing, Daodejing Daily, Qiaopi Quiz,
Weiqi Quest, Art Magazine, Gomoku Duel, Seven Bridges and Chinese Checkers are
its siblings. The quote parser, watchlist and interface here are written from
scratch; Wi-Fi provisioning comes from the family-verified esp-wifi-connect
component; the board support package and repository tools are the
family-shared platform layer (MIT, see [`LICENSE`](LICENSE)).
