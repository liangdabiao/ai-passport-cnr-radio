<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# AI Passport 网络电台 —— 三键小机器上的互联网收音机

装在 FoloToy AI Passport 上的口袋收音机：26 个精选电台（新闻 / 音乐 /
曲艺 / 故事 / 环球），纯 HTTP 直连、机器自己解码。上下键调台、OK 播放暂停、
长按上下调音量，复古刻度盘上有一根滑动的指针。

**所有电台地址都来自 [radio-browser.info](https://www.radio-browser.info)** ——
全球最大的社区维护电台数据库，收录 **4 万+ 个电台的直播流地址**，不用注册，
直接搜索就能试听。想听什么台自己加：见下面的[自己加电台](#自己加电台)。

| 标题页 | 收音机主页 | 音量浮层 |
| --- | --- | --- |
| 铭牌、联网状态、装饰刻度带。 | 刻度盘 + 指针、台名、实时状态与码率、播放中跳动的调谐条。 | 长按上下出现，1.6 秒自动消隐。 |

## 电台从哪来 —— radio-browser.info

[radio-browser.info](https://www.radio-browser.info) 是一个由社区维护的全球
电台目录，收录 **4 万多个电台**，带国家、语言、标签、编码格式、码率、评分等
信息。

- **不用注册**：打开网站 → 搜台名 → 点开就能试听。
- **直接给出直播流地址**：每个台都挂着机器要用的那条 URL，通常以
  `.mp3`、`.aac` 或 `.pls` 结尾。
- **数据免费开放**，还提供公开 API，方便批量整理自己的台单。

固件里这 26 个台就是从 radio-browser 的中文与国际列表里挑出来的，并且
**逐个实测过**：`tools/station_http.py` 对每个地址做 8 秒 HTTP GET，实收
≥32KB 才算数，避免把失效的台烧进固件。

## 自己加电台

全部电台都在一张表里：`main/nr_station.c`。三步搞定。

### 第 1 步：在 radio-browser.info 找到直播流地址

打开 <https://www.radio-browser.info>，搜索台名（或按国家、标签浏览），
进入电台页面，复制它的 **stream URL**。

这台机器上有两条硬规矩：

- **必须是 `http://`，不能是 `https://`。** ESP32-C3 没有 PSRAM，TLS 是为
  省内存刻意不做的，只有 https 的台播不了。
- **必须是 MP3。** 解码器是定点 MP3 解码器（Helix），AAC/OGG 暂不支持。

不确定地址能不能用？先在电脑上验一下：

```bash
curl -s -o /dev/null --max-time 8 -w "%{http_code} %{content_type}\n" \
  "http://你的电台地址/stream.mp3"
```

能用的台会返回 `200 audio/mpeg` 并且持续下载。想一次批量筛一遍，直接用仓库
里的脚本（自带分类搜索与实测记录）：

```bash
python tools/station_scan.py     # 按分类搜 radio-browser 并逐个探活
python tools/station_http.py     # 强制纯 http、按实收字节数判定
```

### 第 2 步：在表里加一行

```c
// main/nr_station.c
const nr_station_t NR_STATIONS[] = {
    // ……已有的台……
    { "我的电台", "http://example.com/live.mp3", NR_CAT_MUSIC },
};
```

字段规矩：

| 字段 | 含义 | 要求 |
| --- | --- | --- |
| `name` | 屏幕上显示的台名 | 要短：全角 ≤7 个字或半角 ≤13 个字符（24px 只有一行）；用到的字必须在字库里（见下一步） |
| `url` | 直播流地址 | 纯 `http://`，MP3 |
| `cat` | 分类 | `NR_CAT_NEWS` / `NR_CAT_MUSIC` / `NR_CAT_QUYI` / `NR_CAT_STORY` / `NR_CAT_WORLD` |

### 第 3 步：改了台数，同步两个地方

- `main/nr_station.h` 里的 `#define NR_STATION_COUNT` —— 编译期会与表格长度
  对齐检查，改漏了是编译报错，不会变成运行时的怪现象。
- **重新生成中文字库**，让新台名的字能显示（缺字会变成空方框）：

```bash
python tools/net-radio/gen_font.py          # 需要 fontTools 与 lv_font_conv
```

然后照常编译烧录。删台就是删掉那一行（开机时会校验存档里的台号，越界自动
回到第一个台）。

## 怎么用

1. **首次使用**：标题页 → 长按 **OK** → 手机连设备热点 **NetRadio** →
   打开 `192.168.4.1` → 输入家里 Wi-Fi（2.4G）。设备会自动联网并记住。
2. **OK** 进收音机页，再按 **OK** 播放/暂停当前台；**短按上/下**换台
   （播放中换台会立刻切流）；**长按上/下**调音量（0–100 共 10 档，屏幕上有
   浮层）；**长按 OK** 回标题页并停播。
3. 上次听的台和音量会记住，断电也不丢。开机不会自动出声，按 OK 才播。

## 构建

[ESP-IDF](https://github.com/espressif/esp-idf) 5.5.3 工程，目标 FoloToy AI
Passport（ESP32-C3，240×320 ST7789，三键 ADC 输入，ES8311 单声道 codec）。

```bash
idf.py build
idf.py flash monitor
./tools/validate.sh          # 静态检查 + 宿主测试 + 固件门禁
```

播放链路：raw socket HTTP GET（支持 chunked、重定向、`icy` 元数据）→ 12KB
环形缓冲 → Helix 定点 MP3 解码（`components/libhelix_mp3`，RPSL/RCSL 许可随
组件保留）→ 降混单声道 → I2S 送给 ES8311。断流自动重连（1s / 2s / 4s 退避）。

## 目录

```
main/nr_station.c     电台表 —— 你唯一需要改的文件
main/nr_player.c      HTTP 收流、环形缓冲、MP3 解码、I2S 输出
main/nr_page_*.c      标题 / 收音机 / 配网 三个页面
main/nr_app.c         导航、音量、断电记忆
components/libhelix_mp3   MP3 解码器（原样收编，RPSL/RCSL）
tools/station_scan.py     搜索并探活 radio-browser 候选台
tools/station_http.py     强制纯 http 的实收字节探活
tools/net-radio/gen_font.py   重新生成中文字库
```

## 文档

- [`AGENTS.zh_CN.md`](AGENTS.zh_CN.md) —— 仓库协作规则。
- [`assets/README.zh_CN.md`](assets/README.zh_CN.md) —— 字体来源与许可。
- [`docs/reference/liangdabiao/net-radio/README.zh_CN.md`](docs/reference/liangdabiao/net-radio/README.zh_CN.md) —— 应用档案。

## 许可与致谢

电台数据与搜索：[radio-browser.info](https://www.radio-browser.info)（社区
维护、免费开放）。MP3 解码：RealNetworks Helix，RPSL/RCSL，许可文件保留在
`components/libhelix_mp3` 内。Wi-Fi 配网来自同族验证过的 `esp-wifi-connect`
组件；板级支持包与仓库工具是家族共享的平台层（MIT，见
[`LICENSE`](LICENSE)）。

FoloToy AI Passport 家族应用：三字经、道德经日课、侨批问答、围棋闯关、
名画画刊、五子棋、七桥谜题、中国跳棋、A股大行情、IQ 挑战、超MBTI、纳兰
诗词是它的兄弟姊妹。
