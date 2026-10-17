<p align="right">
  <a href="README.md">English</a> · <strong>简体中文</strong>
</p>

<div align="center">

# FMO 实时通联状态屏（AI Passport）

**一块常亮的 FMO 中继状态小屏 —— 已移植到小智 ESP32-S3 面包板套件。**

| 构建 | 版本 | 框架 | 屏幕 |
|---|---|---|---|
| [![CI](https://img.shields.io/badge/CI-passing-2ea44f)](https://github.com/tel1390-bot/fmo-s3/actions) | [v1.0.0](https://github.com/tel1390-bot/fmo-s3/releases) | ESP-IDF 5.5.3 | 1.59" SPI-LCD 240×280 |

</div>

实时观看 FMO 频道状态：当前讲话呼号、空闲时上一呼号、最近三条通联记录、连接状态、PCM 音频电平条、时间与电量——全部呈现在一块彩色小屏上。固件只读取**本地 FMO 的 Web 接口**（`/events`、`/ws`、`/audio`），设备上不保存任何 FMO 凭据或证书。

---

## 功能特性

- 🗣 **实时呼号显示** — 讲话中橙色加粗、`crossServer` 红色、空闲显示白色上一呼号，网格保留至下一讲话者
- 🕘 **QSO 历史面板** — 最近三条通联记录 + 北京时间
- 📶 **音频电平条** — 通话 PCM 的 RMS，快升缓降
- 🔇 **软件音量** — 一键开关音频，预置 `CONFIG_FMO_VOLUME`（默认 50%）
- 🌐 **扫码配网** — 首次开机开热点 `FMO-Setup-XXXX` + 网页 `192.168.9.1`，最多保存 5 个 Wi-Fi 配置，FMO 地址默认 `fmo.local:80`（可改）
- 🔋 **电量胶囊** — 百分比显示，≤20% 变红，未知显示描边
- 😴 **息屏省电** — 30 秒变暗、90 秒关屏，按键/语音唤醒
- 🔧 **Kconfig 屏参调节** — 高度 / 反色 / 偏移 / 镜像，适配你的屏

## 支持的硬件

| 板子 | 状态 |
|---|---|
| **ESP32-S3-DevKitC-1 (N16R8) + 1.59" SPI-LCD + MAX98357A + INMP441** | ✅ 当前主线目标（本移植） |
| FoloToy AI Passport（ESP32-C3） | 原始目标（[上游仓库](https://github.com/LeopardDennis/ai-passport-fmo)） |

### 接线（小智面包板套件）

| 模块 | 引脚 |
|---|---|
| 屏（SPI） | SCK=`21` MOSI=`47` RES=`45` DC=`40` CS=`41` BL=`42` |
| MAX98357A 功放 | DIN=`7` BCLK=`15` LRC=`16` |
| INMP441 麦克风 | WS=`4` SCK=`5` SD=`6`（FMO 固件用不到） |
| 按键 | 板载 BOOT（GPIO0，单键） |

完整移植说明：[`docs/FMO_S3_PORT.zh_CN.md`](docs/FMO_S3_PORT.zh_CN.md)。

## 快速开始

1. **烧录**：从最新 [Releases](https://github.com/tel1390-bot/fmo-s3/releases) 下载固件（合并镜像 → 地址 `0x0`）：
   ```bash
   python -m pip install esptool
   python -m esptool --chip esp32s3 -p COM12 -b 921600 write_flash 0x0 FoloToy-AI-Passport-full.bin
   ```
2. **配网**：首次开机板子开热点 `FMO-Setup-XXXX`（随机密码显示在屏上）。手机连上热点，浏览器打开 `http://192.168.9.1`，填你家 2.4GHz Wi-Fi 的 SSID/密码，保存。热点自动关闭，设备连上 FMO（默认 `fmo.local:80`）。
3. **监听**：屏幕显示频道名和当前讲话呼号，通话音频默认 50% 音量播放。

### 单键操作（本板）

| 操作 | 功能 |
|---|---|
| 短按 BOOT | 音频开关 |
| **双击 BOOT** | 进入网络菜单 → 再次配网（短按确认执行） |
| 长按 BOOT（700ms） | 台站列表（主屏）/ 返回（子页） |

> 本板没有上下键：音量用 `CONFIG_FMO_VOLUME` 预置（默认 50%）。屏幕熄灭后第一次按键只唤醒屏幕，再按才生效。

## 编译

```bash
# 需要 ESP-IDF 5.5.3（含 esp32s3 工具链）
idf.py set-target esp32s3
idf.py menuconfig        # FMO Live Monitor：屏参/音量/网络设置
idf.py build
idf.py merge-bin -o FoloToy-AI-Passport-full.bin
idf.py -p COM12 flash monitor
```

GitHub Actions 每次推送都用 ESP-IDF 5.5.3 / esp32s3 构建，并上传验证固件产物（`build/validation/FoloToy-AI-Passport-full.bin`，不含任何内置凭据）。

## menuconfig 选项（FMO Live Monitor）

| 选项 | 默认 | 含义 |
|---|---|---|
| `FMO_LCD_HEIGHT` | 280 | 屏高（240 / 280 / 320 预览） |
| `FMO_LCD_INVERT` | y | 画面反色时打开 |
| `FMO_LCD_GAP_X / GAP_Y` | 0 | 像素偏移（部分屏需 1~2） |
| `FMO_LCD_MIRROR_X / MIRROR_Y` | n | 水平 / 垂直镜像 |
| `FMO_VOLUME` | 50 | 初始软件音量 % |
| `FMO_WIFI_SSID / PASSWORD` | 空 | 可选预置（勿发布） |
| `FMO_HOST / FMO_PORT` | fmo.local / 80 | FMO 目标（配网页也能改） |

## 显示说明

- 状态文字为中文；呼号、网络名、地址原样显示。
- 未知元数据不做猜测，首个通联前显示 `--`。
- 息屏时音频照常播放；播放期间关闭 Wi-Fi modem 省电以降低抖动。
- 时间为北京时间（UTC+8，SNTP 校时）；时钟未同步前显示 `--:--`。

## 兼容与可靠性

- 兼容社区 FMO Web 客户端使用的本地接口（字段变化改 `main/fmo_network.c` 的 `parse_fmo_message()`）。
- 三条 socket（events / control / audio）每 10 秒心跳、自动重连；元数据恢复不会静音正常音频。
- Wi-Fi：上次成功优先、按扫描信号重试、30 秒一轮；`fmo_wifi` NVS 最多 5 个配置；支持 WPA2/开放网络。
- 音频环形缓冲：24 KiB（约 1.5 秒），整条消息预留、背压代替丢包；静音/断线清空队列。
- 全新安装数据重置由**完整合并包**（bootloader + 分区表 + app）携带，仅刷 app 无法触发。

## 建议的硬件验证

- 开机与 FMO 侧切频后确认频道/界面一致。
- 按 PTT 松开：检查讲话指示、呼号切换、网格保留。
- 两端经历 Wi-Fi 中断后确认自动重连。
- 查看 USB 日志：堆最小值、最大分配、栈余量、按键响应、屏幕裁剪、电量显示。

## 致谢

- 上游移植： [LeopardDennis/ai-passport-fmo](https://github.com/LeopardDennis/ai-passport-fmo)
- 基础项目： [FoloToy/ai-passport](https://github.com/FoloToy/ai-passport)
- 接口参考： [FMO web client API](https://github.com/niufox/fmo-mobile-controller/blob/main/API_DOCUMENTATION_v2.md)

> 安全提示：切勿发布 Wi-Fi 密码、FMO 密钥、证书或未脱敏日志。CI 产物仅含测试用虚拟网络配置。
