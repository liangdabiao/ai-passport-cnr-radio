<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# CNR Radio — China's official stations on a three-key handheld

An internet radio for the FoloToy AI Passport, sourced entirely from **[radio.cn](https://www.radio.cn/pc-portal/erji/radioStation.html)** — the official online radio platform of CNR (China National Radio): every national CNR channel (Voices of China, Business, Music…) plus provincial stations across the country. Fifty curated stations in eight regional groups (CNR / North / Northeast / East / Central / South / Southwest / Northwest), each one verified alive (playlist fetch + a real 10-second segment download) before being baked into the firmware.

Tune with Up/Down, play or pause with OK, hold Up/Down for volume. A retro dial with a sliding needle — in a China-red theme, distinct from its amber sibling app "Net Radio".

| Title | Tuning screen | Volume overlay |
| --- | --- | --- |
| Nameplate, network state, decorative dial. | Dial with a needle per station, station name, live status and bitrate, dancing tuning bars while playing. | Hold Up/Down; auto-hides after 1.6 s. |

## What makes it different from an MP3 radio

radio.cn streams are **HLS**: an m3u8 live playlist → 10-second MPEG-TS segments →
AAC-LC audio frames inside. The whole playback chain is built from scratch:

- **HLS client**: playlist fetch, live-edge tracking, sequential segment download,
  resume-by-segment-name across playlist refreshes.
- **TS demuxer** (`main/cr_ts.c`): PAT → PMT → audio PID → PES unwrap, emitting a
  clean ADTS byte stream. Verified against a real CNR segment in host tests —
  byte-identical to a reference demuxer, all 431 frames out of a 10-second
  segment.
- **AAC decoder**: Helix fixed-point AAC (`components/libhelix_aac`, RPSL/RCSL
  licences kept in-component), running from a 24 KB static arena so a
  PSRAM-less chip can afford it.
- Mono downmix + software volume (codec at full scale, Q8 gain, 100% = 3×).

## Getting started

1. First run: title screen → hold **OK** → join the device hotspot **CnrRadio**
   → open `192.168.4.1` → enter your home Wi-Fi (2.4 GHz). The device joins and
   remembers it.
2. **OK** opens the tuning screen; **OK** again plays/pauses. Tap Up/Down to tune
   across the country (CNR → North → Northeast → …). Hold Up/Down for volume
   (0–100, ten steps).
3. Hold **OK** to stop and return. The last station and volume survive reboots.

## Build

An [ESP-IDF](https://github.com/espressif/esp-idf) 5.5.3 project for the FoloToy
AI Passport (ESP32-C3, 240×320 ST7789, three-key ADC input, ES8311 codec).

```bash
idf.py build
idf.py flash monitor
./tools/validate.sh
```

## Layout

```
main/cr_station.c     station table (50 radio.cn stations, script-generated)
main/cr_player.c      HLS client, sessions, reconnect, volume
main/cr_ts.c          MPEG-TS → PES → ADTS demux (pure logic, host-tested)
main/cr_hls.c         m3u8 parsing + ADTS validation + HTTP header/chunked
main/cr_page_*.c      title / tuning / Wi-Fi screens
components/libhelix_aac   AAC decoder (vendored + platform patches, RPSL/RCSL)
tools/cnr-radio/gen_font.py   regenerate the CJK font subsets
```

## Licence and credits

A FoloToy AI Passport family app — sibling of Sanzijing, Daodejing Daily, Qiaopi
Quiz, Weiqi Quest, Art Magazine, Gomoku Duel, Seven Bridges, Chinese Checkers,
A-Share Watchlist, IQ Challenge, MBTI Test, Nalan Poetry and Net Radio. The HLS
client, TS demux, AAC playback pipeline and UI are written from scratch for this
app; AAC decoding is RealNetworks Helix (RPSL/RCSL, licence files inside
`components/libhelix_aac`); Wi-Fi provisioning comes from the family-verified
esp-wifi-connect component; the board support package and repository tools are
the family-shared platform layer (MIT, see [`LICENSE`](LICENSE)). Radio content
and data belong to CNR / radio.cn; this project is for personal study.
