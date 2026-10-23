# 网络电台（net-radio）—— 应用档案

<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

2026-10-23 从家族联网基线 `astock-quote` 派生（分支 `feature/net-radio`）。
家族第一个网络电台应用。

## 已确认的需求

- 26 个精选台，五个分类（新闻 / 音乐 / 曲艺 / 故事 / 环球），固化进固件；
  v1 不做在线搜台。
- 三键交互：上下换台、OK 播放暂停、长按上下调音量（10 档带浮层）、长按
  OK 返回并停播。
- 复古收音机皮肤（深胡桃 + 琥珀），界面全新设计。
- 断电记忆上次电台与音量；开机不自动出声。
- 台源纯 http，逐台以「GET 收流 8 秒、实收 ≥32KB」验证（`tools/station_http.py`，
  数据在 `tools/station_http.tsv`）。

## 实现要点

- 流媒体：raw-socket HTTP → 24KB 环形缓冲 → Helix 定点 MP3
  （`components/libhelix_mp3`，取自 ESP8266Audio 的 libhelix-mp3）→ ES8311。
- TLS 刻意不做（C3 无 PSRAM）；https-only 的台（如蜻蜓部分入口）筛选时剔除。
- 筛选教训：部分 CDN（蜻蜓）对 HEAD 回 400 但 GET 正常——探活必须用 GET，
  与设备端行为一致。

## 构建标识

见 `feature/net-radio` 分支 git log；固件产物由 `./tools/validate.sh --firmware`
产出。

## 真机结果

待首次真机测试（配网 + 出声 + 换台）。
