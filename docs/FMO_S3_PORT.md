# FMO-S3 Port：把 ai-passport-fmo 移植到 ESP32-S3（小智面包板套件）

本文档说明如何把原为 **FoloToy AI Passport（ESP32-C3）** 编写的 FMO 实时通联状态屏固件，
移植到 **ESP32-S3-DevKitC-1（N16R8）+ 1.59" SPI-LCD** 的小智 AI 面包板 DIY 套件上。

工程目录：`fmo-s3/`（本仓库根目录）。所有改动均已落在该目录内，可直接编译。

---

## 1. 一句话结论

**可行，且改动集中在 4 个文件**（引脚表、音频驱动、按键驱动、UI 布局）。应用层
（Wi-Fi 配网、FMO WebSocket、台站切换、QSO 历史、熄屏策略）与芯片无关，一行未动。
音频层只需"播放"通道——MAX98357A 功放即可，**INMP441 麦克风在本固件中不使用**。

## 2. 与 Passport 硬件的差异对照

| 模块 | Passport（原版，C3） | 你的套件（S3） | 本移植处理 |
|---|---|---|---|
| SoC | ESP32-C3（RISC-V，无 PSRAM） | ESP32-S3 N16R8（Xtensa，8MB PSRAM） | sdkconfig 改 `esp32s3`，开 SPIRAM、16MB Flash |
| 屏幕 | ST7789P3 240x320（MOSI=9/SCLK=8/CS=1/DC=20/BL=21） | 1.59" SPI-LCD（SCK=21/MOSI=47/RES=45/DC=40/CS=41/BL=42） | 引脚表重写，分辨率走 Kconfig（240/280），驱动仍用 ST7789（GC9107 兼容） |
| 音频 | ES8311 I2C codec + I2S 全双工（MCLK/BCLK/WS=3/DOUT=2/DIN=4） | MAX98357A I2S 功放（DIN=7/BCLK=15/LRC=16）+ INMP441 麦克风（本固件不用） | `bsp_audio.c` 重写为裸 I2S 输出 + 软件音量 |
| 按键 | 三键 ADC 分压（GPIO0） | 板载 BOOT 键（GPIO0，单键） | `bsp_button.c` 重写为 GPIO 单键 = 确定键 |
| 电池 | CW2017 电量计（I2C） | 无 | I2C 挪到空脚 GPIO8/9，CW2017 不应答自动降级，UI 显示 "--" |
| Recovery | Passport BLE 安装器（bootloader hook） | 无此生态 | 删除 bootloader hook 组件，关闭长按跳转 |
| USB 控制台 | USB-Serial-JTAG（GPIO18/19） | USB-Serial-JTAG（GPIO19/20，Type-C 直连） | 保留 `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` |

## 3. 接线表（按飞书教程 2.1 节，供快速对照）

```
ESP32-S3          INMP441（麦克风，本固件不用）
GPIO4  ──>  WS
GPIO5  ──>  SCK
GPIO6  ──>  SD
3V3    ──>  VDD
GND    ──>  GND + L/R 短接

ESP32-S3          MAX98357A（功放）
GPIO7  ──>  DIN
GPIO15 ──>  BCLK
GPIO16 ──>  LRC
3V3    ──>  Vin（SD 悬空/接 3V3 使能）
GND    ──>  GND + GAIN 短接（9dB）
音频+/- ──> 喇叭正/负极

ESP32-S3          SPI-LCD（1.59"）
GPIO21 ──>  SCL(SCK)
GPIO47 ──>  SDA(MOSI)
GPIO45 ──>  RES
GPIO40 ──>  DC
GPIO41 ──>  CS
GPIO42 ──>  BLK
GND/3V3 ──> GND/VCC
```

> ⚠️ 原 Passport 的 I2C 用了 GPIO7/10，其中 GPIO7 与本板功放 DIN 冲突——本移植已把
> I2C 挪到 GPIO8/9（空脚），避免冲突。

## 4. 已修改文件清单

| 文件 | 改动 |
|---|---|
| `sdkconfig.defaults` | C3→S3、开 SPIRAM(Octal 8MB)、Flash 16MB、关 Recovery、保留 USB-JTAG |
| `CMakeLists.txt`（根） | 移除 bootloader_components 挂载（Passport Recovery 专用） |
| `bootloader_components/` | 整目录删除 |
| `components/bsp/include/bsp_pins.h` | 引脚表按小智接线重写；分辨率/反色/偏移/镜像改 Kconfig |
| `components/bsp/src/bsp_audio.c` | 重写：MAX98357A 裸 I2S 输出、mono→stereo 展开、软件音量；头文件接口不变 |
| `components/bsp/src/bsp_button.c` | 重写：单 GPIO 键（GPIO0 BOOT）= 确定键 |
| `components/bsp/src/bsp_display.c` | 镜像/偏移/反色改宏；RST=45；注释更新 |
| `components/bsp/src/bsp_display_lvgl.c` | S3 下 40 行双缓冲（原 C3 单缓冲 20 行） |
| `components/bsp/CMakeLists.txt` | REQUIRES 去掉 `esp_codec_dev` |
| `components/bsp/idf_component.yml` | 去掉 `espressif/esp_codec_dev` 依赖 |
| `main/Kconfig.projbuild` | 新增 `FMO_LCD_HEIGHT / INVERT / GAP_X / GAP_Y / MIRROR_X / MIRROR_Y` |
| `main/fmo_ui.c` | 布局按 `FMO_UI_H` 自适应：320（host 预览）/ 280（1.59" 默认）/ 240（紧凑）三套 |
| `tools/check_fmo_config.py` | 兼容 bool 型 `CONFIG_FMO_*` 配置（`=y/n`） |
| `tests/test_bsp_audio_runtime.py` | 重写 stub 以匹配 MAX98357A 驱动（I2S 输出/展开/音量） |
| `.github/workflows/build-firmware.yml` | CI 目标 `esp32c3` → `esp32s3` |

未修改：`main/main.c`、`fmo_audio.c`、`fmo_network.c`、`fmo_controls.c` 等全部应用层代码。

## 5. 编译与烧录

### 5.1 本地构建（需 ESP-IDF 5.5.3）

Windows 用户推荐用乐鑫官方安装器安装 ESP-IDF 5.5.3（含 esp32s3 工具链），然后：

```bash
# 打开 "ESP-IDF 5.5 CMD" 命令行
cd fmo-s3
idf.py set-target esp32s3        # 或直接删除 build/ 目录全新构建
idf.py menuconfig                # 按需调整（见 5.2）
idf.py build

# 合并固件（含分区表/bootloader，可直接烧整片）
idf.py merge-bin -o FoloToy-AI-Passport-full.bin

# 烧录（USB-Serial-JTAG，Type-C 数据线）
idf.py -p COM<x> flash monitor
```

> 说明：`sdkconfig.defaults` 已含 esp32s3 目标，`set-target` 只用于生成/校验本地
> `sdkconfig`。首次构建会自动拉取 LVGL、esp_lvgl_port、button 等依赖（需要网络）。

### 5.2 menuconfig 屏幕参数（关键调试点）

`FMO Live Monitor` 菜单下：

| 配置 | 默认 | 说明 |
|---|---|---|
| `FMO_LCD_HEIGHT` | 280 | 1.59" 屏：240x280（GC9107/ST7789 兼容）选 280；若你的屏是 240x240 选 240 |
| `FMO_LCD_INVERT` | y | 画面呈负片时切换 |
| `FMO_LCD_GAP_X / GAP_Y` | 0 | 画面整体偏移（GC9107 常见 ±1~2px） |
| `FMO_LCD_MIRROR_X / MIRROR_Y` | n | 画面左右/上下镜像 |
| `FMO_WIFI_SSID/PASSWORD` | 空 | 可预置 Wi-Fi；也可用首次开机的配网热点扫码配网 |
| `FMO_HOST / FMO_PORT` | fmo.local / 80 | FMO 设备地址，配网页可改 |

### 5.3 GitHub Actions 构建（无本地 IDF 时推荐）

仓库已带 CI：把 `fmo-s3/` 推到你自己的 GitHub 仓库（tag 推送或手动 `workflow_dispatch`），
GitHub 会用 ESP-IDF 5.5.3 / esp32s3 构建出 `build/validation/FoloToy-AI-Passport-full.bin`，
在 Actions 页面下载即可烧录。CI 产物为"无预置 Wi-Fi"的通用镜像，开机用配网热点配网。

## 6. 按键与音量说明（单键约束）

- 本板只有板载 **BOOT 键（GPIO0）**，映射为"确定键"：
  - 短按：开关音频
  - 长按：切换台站（守听页）/ 返回（子页面）
  - 配网页短按：切换二维码/热点信息
- **上/下键（音量 ±10%、长按进网络菜单）无硬件**。音量用 menuconfig 的
  `FMO_VOLUME` 预置（默认 50%）；如需运行时调音量，可外接一个按键改
  `bsp_button.c`（把第二键映射为 `BSP_BTN_UP`），或外接三键 ADC 分压电路换回原版
  `bsp_button.c`。
- 音频为**软件音量**（PCM 衰减），MAX98357A 硬件增益固定由 GAIN 引脚决定（接 GND=9dB）。

## 7. 已知限制

1. **本固件不采集麦克风**（FMO 是守听屏，只播 /audio 通联流），INMP441 可不接或照教程接上备用。
2. **无电池检测**（套件无 CW2017），顶部电池显示 "--" 空轮廓。
3. **屏幕驱动按 ST7789 兼容点亮**（覆盖 GC9107 等 1.59" 屏）。个别屏需要厂商专属
   初始化序列时，以 `bsp_display.c` 中的 `ST7789P3_CMDS` 为模板替换。
4. 分区表沿用原项目（recovery 分区保留但无固件、不会被跳转），浪费 1MB Flash，可后续精简。
5. `dependencies.lock` 中的 `target: esp32c3` 会在首次构建时被 IDF 自动更新为 esp32s3。

## 8. 真机验证清单

- [ ] 烧录后屏幕点亮，无花屏/负片/偏移（不对就用 5.2 调整）
- [ ] USB 日志出现 `Display ready 240x280`、`MAX98357A 就绪 8000Hz/16bit/mono`、`按键就绪:GPIO0`
- [ ] 首次开机进入配网热点（无预置 Wi-Fi 时），手机扫码配网成功
- [ ] 连上 FMO 后显示频道名，有人发言显示橙色呼号 + 音频强度条
- [ ] 播放通联语音正常、音量可调（软件衰减无爆音）
- [ ] 短按 BOOT 键开关音频，长按切换台站
- [ ] 30 秒调暗、90 秒熄屏，按键/发言可唤醒
- [ ] 熄屏期间音频继续播放（FMO 守听不受影响）

## 9. 回退

`fmo-s3/` 与原始下载（`src/ai-passport-fmo-main/`）相互独立。如需恢复 C3/Passport
版本，直接用原始目录构建即可；两版互不影响。
