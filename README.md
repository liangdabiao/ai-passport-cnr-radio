<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# AI Passport Net Radio — internet radio on a three-key handheld

A pocket internet radio for the FoloToy AI Passport: 26 curated stations
(news, music, crosstalk & storytelling, kids' stories, world music) streamed
over plain HTTP and decoded on the device itself. Tune with Up/Down, play or
pause with OK, hold Up/Down for volume. A retro dial with a sliding needle
shows where you are.

All station URLs come from **[radio-browser.info](https://www.radio-browser.info)** —
the largest community-maintained radio database in the world, with **40,000+
live stream URLs**, no account needed, free to search and preview. You can add
any station you like: see [Add your own stations](#add-your-own-stations).

| Title | Tuning screen | Volume overlay |
| --- | --- | --- |
| Nameplate, network state, decorative dial. | Dial with a needle per station, station name, live status and bitrate, dancing tuning bars while playing. | Hold Up/Down; auto-hides after 1.6 s. |

## Where the stations come from — radio-browser.info

[radio-browser.info](https://www.radio-browser.info) is a community-run, global
radio directory. It indexes more than 40,000 stations with their direct stream
URLs and metadata (country, language, tags, codec, bitrate, vote count).

- **No sign-up.** Open the site, type a station name, press play.
- **Direct stream URLs.** Every station exposes the raw stream address the
  device needs — usually ending in `.mp3`, `.aac` or `.pls`.
- **Free and open data**, plus a public API if you want to script your own
  station list.

The 26 stations baked into this firmware were picked from radio-browser's
Chinese and international listings and then **probed one by one** with
`tools/station_http.py` (8-second HTTP GET, at least 32 KB received) so a dead
stream does not ship inside the firmware.

## Add your own stations

Everything lives in one table — `main/nr_station.c`. Three steps:

**1. Find a stream URL on radio-browser.info**

Open <https://www.radio-browser.info>, search for the station name (or browse by
country / tag), and open the station page. Copy its **stream URL**.

Two rules matter for this device:

- **It must be plain `http://`, not `https://`.** The ESP32-C3 has no PSRAM; TLS
  is deliberately out of scope, so https-only stations cannot play.
- **It should be MP3.** The decoder is a fixed-point MP3 decoder (Helix). AAC/OGG
  streams are not supported yet.

Not sure whether a URL works? Probe it from your computer first:

```bash
curl -s -o /dev/null --max-time 8 -w "%{http_code} %{content_type}\n" \
  "http://your-station-url/stream.mp3"
```

A working stream answers `200 audio/mpeg` and keeps downloading. To probe a whole
candidate list at once, use the bundled script (it also filters and logs results):

```bash
python tools/station_scan.py     # search radio-browser by category and probe
python tools/station_http.py     # force plain http and measure real bytes received
```

**2. Add a row to the table**

```c
// main/nr_station.c
const nr_station_t NR_STATIONS[] = {
    // ... existing stations ...
    { "My Station", "http://example.com/live.mp3", NR_CAT_MUSIC },
};
```

Field rules:

| Field | Meaning | Rule |
| --- | --- | --- |
| `name` | Shown on screen | Keep it short: ≤7 CJK characters or ≤13 ASCII characters (24 px, one line). Characters must exist in the font subset (see below). |
| `url` | Stream address | Plain `http://`, MP3. |
| `cat` | Category | `NR_CAT_NEWS`, `NR_CAT_MUSIC`, `NR_CAT_QUYI`, `NR_CAT_STORY`, `NR_CAT_WORLD` |

**3. If you changed the number of stations, update two places**

- `main/nr_station.h`: `#define NR_STATION_COUNT` — compile-time asserted
  against the table, so a mismatch is a build error, not a runtime surprise.
- Regenerate the Chinese font subset so new characters render (missing glyphs
  would show as empty boxes):

```bash
python tools/net-radio/gen_font.py          # needs fontTools + lv_font_conv
```

Then build and flash as usual. Removing a station is just deleting its row
(the saved station index is validated on boot, so an out-of-range value falls
back to the first station).

## Using the radio

1. First run: title screen → hold **OK** → join the device hotspot
   **NetRadio** with your phone → open `192.168.4.1` → enter your home Wi-Fi
   (2.4 GHz). The device joins and remembers it.
2. **OK** enters the tuning screen; **OK** again plays/pauses the current
   station. Short **Up/Down** tunes to the previous/next station (switching
   streams live while playing). Hold **Up/Down** to change volume (0–100 in ten
   steps, with an on-screen overlay). Hold **OK** to return to the title screen
   and stop playback.
3. The last station and volume survive reboots. Nothing plays automatically on
   boot — press OK when you want sound.

## Build

An [ESP-IDF](https://github.com/espressif/esp-idf) 5.5.3 project targeting the
FoloToy AI Passport (ESP32-C3, 240×320 ST7789, three-key ADC input, ES8311
mono codec).

```bash
idf.py build
idf.py flash monitor
./tools/validate.sh          # static checks + host tests + firmware gate
```

Streaming pipeline: raw-socket HTTP GET (chunked and plain bodies, redirects,
`icy` metadata) → 12 KB ring buffer → Helix fixed-point MP3
(`components/libhelix_mp3`, RPSL/RCSL licences included) → downmix to mono →
ES8311 via I2S. Reconnects automatically (1 s / 2 s / 4 s backoff).

## Layout

```
main/nr_station.c     station table — the only file you need to edit
main/nr_player.c      HTTP streaming, ring buffer, MP3 decode, I2S output
main/nr_page_*.c      title / tuning / Wi-Fi screens
main/nr_app.c         navigation, volume, persisted state
components/libhelix_mp3   MP3 decoder (vendored, RPSL/RCSL)
tools/station_scan.py     search + probe radio-browser candidates
tools/station_http.py     force-http probe with real-byte threshold
tools/net-radio/gen_font.py   regenerate the CJK font subsets
```

## Documentation

- [`AGENTS.md`](AGENTS.md) — repository rules for AI-assisted work.
- [`assets/README.md`](assets/README.md) — font provenance and licence.
- [`docs/reference/liangdabiao/net-radio/README.md`](docs/reference/liangdabiao/net-radio/README.md) — application record.

## Licence and credits

Station data and search: [radio-browser.info](https://www.radio-browser.info)
(community-maintained, free and open). MP3 decoding: RealNetworks Helix,
RPSL/RCSL, licence files kept inside `components/libhelix_mp3`. Wi-Fi
provisioning comes from the family-verified `esp-wifi-connect` component; the
board support package and repository tools are the family-shared platform layer
(MIT, see [`LICENSE`](LICENSE)).

A FoloToy AI Passport family app — sibling of Sanzijing, Daodejing Daily,
Qiaopi Quiz, Weiqi Quest, Art Magazine, Gomoku Duel, Seven Bridges, Chinese
Checkers, A-Share Watchlist, IQ Challenge, MBTI Test and Nalan Poetry.
