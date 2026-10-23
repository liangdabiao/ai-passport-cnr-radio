<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 央广播音机 —— 三键设备上的官方网络电台

装在 FoloToy AI Passport 上的网络收音机，台源全部来自 **[radio.cn（央广网）](https://www.radio.cn/pc-portal/erji/radioStation.html)** —— 央广网官方互联网电台平台：央广全套（中国之声、经济之声、音乐之声……）加全国各省级电台，一台机器听遍全国。50 个精选电台分八个区组（央广/华北/东北/华东/华中/华南/西南/西北），每个地址都经过「取播放列表 → 下载 10 秒切片」实测验证。

上下键调台、OK 播放暂停、长按上下调音量，复古刻度盘配滑动指针（中国红主题，与兄弟应用「网络电台」的琥珀木纹区分）。

| 标题页 | 收音机主页 | 音量浮层 |
| --- | --- | --- |
| 铭牌、联网状态、装饰刻度带。 | 刻度盘 + 指针、台名、实时状态与码率、播放中跳动的调谐条。 | 长按上下出现，1.6 秒自动消隐。 |

## 技术要点（本应用与一般 MP3 电台的区别）

radio.cn 的直播流是 **HLS 切片流**：m3u8 播放列表 → 10 秒一片的 MPEG-TS →
里面装着 AAC-LC 音频帧。设备端播放链路完全自研：

- **HLS 客户端**：取播放列表、跟踪直播边、逐段下载、按段名续播；
- **TS 解复用器**（`main/cr_ts.c`）：PAT→PMT→音频 PID→PES 剥壳，吐出干净的
  ADTS 字节流 —— 用真实央广切片做宿主测试，与参考实现逐字节一致（431 帧/
  10 秒，一帧不丢）；
- **AAC 解码器**：Helix 定点 AAC（`components/libhelix_aac`，RPSL/RCSL 许可
  随组件保留），静态内存池 24KB，无 PSRAM 的芯片也装得下；
- 单声道降混 + 软件音量（codec 满量程 + Q8 增益，100% 档 3 倍）。

## 快速上手

1. 首次使用：标题页 → 长按确定 → 手机连设备热点 **CnrRadio** → 访问
   192.168.4.1 输入家里 Wi-Fi（2.4G）→ 设备自动联网并记住。
2. **OK** 进收音机页，再按 **OK** 播放；短按上/下换台（跨省听：央广→华北
   →东北→…循环）；长按上/下调音量（0–100 共 10 档）。
3. 长按确定回标题页并停播。上次电台与音量断电记忆。

## 构建

[ESP-IDF](https://github.com/espressif/esp-idf) 5.5.3 工程，目标 FoloToy AI
Passport（ESP32-C3，240x320 ST7789，三键 ADC 输入，ES8311 音频）。

```bash
idf.py build
idf.py flash monitor
./tools/validate.sh
```

## 目录

```
main/cr_station.c     电台表（radio.cn 50 台，tools 下脚本生成）
main/cr_player.c      HLS 客户端 + 会话/重连/音量
main/cr_ts.c          MPEG-TS → PES → ADTS 解复用（纯逻辑，宿主可测）
main/cr_hls.c         m3u8 解析 + ADTS 帧校验 + HTTP 头/chunked（纯逻辑）
main/cr_page_*.c      标题 / 收音机 / 配网 三个页面
components/libhelix_aac   AAC 解码器（原样收编 + 平台适配，RPSL/RCSL）
tools/cnr-radio/gen_font.py   重新生成中文字库
```

## 文档

- [`AGENTS.zh_CN.md`](AGENTS.zh_CN.md) —— 仓库协作规则。
- [`assets/README.zh_CN.md`](assets/README.zh_CN.md) —— 字体来源与许可。

## 出身与许可

FoloToy AI Passport 家族应用：三字经、道德经日课、侨批问答、围棋闯关、名画
画刊、五子棋、七桥谜题、中国跳棋、A股大行情、IQ 挑战、超MBTI、纳兰诗词、
网络电台是它的兄弟姊妹。本应用的 HLS 客户端、TS 解复用、AAC 播放链路与界面
为它从零编写；AAC 解码来自 RealNetworks Helix（RPSL/RCSL，许可文件在
`components/libhelix_aac`）；Wi-Fi 配网来自同族验证过的 esp-wifi-connect
组件；板级支持包与仓库工具是家族共享的平台层（MIT，见
[`LICENSE`](LICENSE)）。电台内容与数据归央广网所有，本项目仅供个人学习。
