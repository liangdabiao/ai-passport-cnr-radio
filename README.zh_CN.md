<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# A股大行情 —— 三键设备上的离线自选行情

与设备人机交互的 A 股自选行情小工具：Wi-Fi 配网后盘中自动刷新自选报价，
红涨绿跌一目了然。两档玩法——3 颗……不，**自选 8 只**、三大指数内置，
东财行情一次批量拉取。全程可用、断网可看缓存。

| 标题页 | 行情列表 | 详情 |
| --- | --- | --- |
| 大标题、网络状态、四项菜单。 | 自选逐行「名称 代码 / 价格 涨跌%」，红涨绿跌；金框选中行。 | 现价大字 + 涨跌 + 昨收/今开/最高/最低/行情时间。 |

## 快速上手

1. 首次使用：标题页 →「Wi-Fi 配网」→ 手机连设备热点 **AStock** → 访问
   192.168.4.1 输入家里 Wi-Fi（2.4G）→ 设备自动联网并记住。
2. 标题页 →「行情列表」看行情；盘中（交易日 9:15–15:05）每 30 秒自动刷新。
3. 「添加股票」输入 6 位代码加入自选（沪深自动识别，上限 8 只）。
4. 上/下选行，确定进详情；长按确定手动刷新；长按下键删除自选。
5. 「刷新设置」切 15/30/60 秒；长按确定两次回标题。

## 三档智能

- **东财行情 JSON 直连**：UTF-8 股名无需转码，一次请求拉全部自选。
- **交易时段感知**：集合竞价到收盘自动刷新，收盘后静默；周末不空转。
- **断网兜底**：行情缓存进 NVS，断网显示最后报价并标注时间，不误导。

## 构建

[ESP-IDF](https://github.com/espressif/esp-idf) 5.5.3 工程，目标板 FoloToy
AI Passport（ESP32-C3，240x320 ST7789，三键 ADC 输入）。

```bash
idf.py build
idf.py flash monitor
./tools/validate.sh
```

## 文档

- [`docs/reference/liangdabiao/astock-quote/README.zh_CN.md`](docs/reference/liangdabiao/astock-quote/README.zh_CN.md) —— 应用档案。
- [`AGENTS.zh_CN.md`](AGENTS.zh_CN.md) —— 仓库规则。
- [`assets/README.zh_CN.md`](assets/README.zh_CN.md) —— 字体来源与许可。

## 来源与许可

FoloToy AI Passport 家族应用：三字经、道德经日课、侨批问答、围棋闯关、
名画画刊、五子棋、七桥谜题与中国跳棋是它的兄弟。本应用的行情解析、自选
管理与界面为它从零编写；Wi-Fi 配网来自同族验证过的 esp-wifi-connect 组件；
板级支持包与仓库工具是家族共享的平台层（MIT，见 [`LICENSE`](LICENSE)）。
