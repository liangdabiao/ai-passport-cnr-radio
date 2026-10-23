# Net Radio (net-radio) — application record

<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

Derived from the family's networked baseline `astock-quote` on 2026-10-23
(branch `feature/net-radio`). First internet-radio app in the family.

## Accepted requirements

- 26 curated stations in five categories (news / music / crosstalk & story
  programmes / stories / world), baked into the firmware; no online search in v1.
- Three-key interaction: Up/Down tunes, OK plays/pauses, long-press Up/Down
  changes volume (10 steps, overlay), long-press OK returns and stops.
- Retro radio skin (dark walnut + amber), fully redesigned screens.
- Reboot restores the last station and volume; no auto-play on boot.
- Stations: plain-HTTP only, each verified by an 8-second GET stream with
  ≥32 KB received (`tools/station_http.py`, data in `tools/station_http.tsv`).

## Implementation notes

- Streaming: raw-socket HTTP → 24 KB ring → Helix fixed-point MP3
  (`components/libhelix_mp3`, from ESP8266Audio's `libhelix-mp3`) → ES8311.
- TLS deliberately out of scope (no PSRAM on C3); https-only stations
  (e.g. some Qingting entries) were rejected during curation.
- Probing lesson: HEAD is rejected with 400 by some CDNs (Qingting) while GET
  works — curation must probe with GET, which is what the device does.

## Build identity

See git log on `feature/net-radio`; firmware binaries are produced by
`./tools/validate.sh --firmware`.

## Device results

Pending first on-device test (Wi-Fi provisioning + audible stream + tuning).
