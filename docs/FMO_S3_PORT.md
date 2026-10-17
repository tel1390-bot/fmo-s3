<p align="right">
  <a href="FMO_S3_PORT.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# FMO-S3 Port: migrating ai-passport-fmo to ESP32-S3 (xiaozhi breadboard kit)

This document explains how to port the FMO live monitor firmware, originally
written for the **FoloToy AI Passport (ESP32-C3)**, to the
**ESP32-S3-DevKitC-1 (N16R8) with a 1.59" SPI-LCD** xiaozhi DIY breadboard kit.

The migrated tree lives in `fmo-s3/` (this repository root). All changes are
already applied and the project is ready to build.

---

## 1. Bottom line

**It works, and the changes are concentrated in 4 files**: the pin map, the
audio driver, the button driver, and the UI layout. All application logic
(Wi-Fi provisioning, FMO WebSockets, station switching, QSO history, sleep
policy) is chip-independent and untouched. The firmware only needs the
**playback** path — the MAX98357A amplifier is enough; the **INMP441 microphone
is not used by this firmware**.

## 2. Hardware differences vs. the Passport

| Module | Passport (original, C3) | Your kit (S3) | Port handling |
|---|---|---|---|
| SoC | ESP32-C3 (RISC-V, no PSRAM) | ESP32-S3 N16R8 (Xtensa, 8 MB PSRAM) | sdkconfig `esp32s3`, SPIRAM on, 16 MB flash |
| Display | ST7789P3 240x320 (MOSI=9/SCLK=8/CS=1/DC=20/BL=21) | 1.59" SPI-LCD (SCK=21/MOSI=47/RES=45/DC=40/CS=41/BL=42) | pin map rewritten; resolution via Kconfig (240/280); still driven as ST7789 (GC9107-compatible) |
| Audio | ES8311 I2C codec, full-duplex I2S (MCLK/BCLK/WS=3/DOUT=2/DIN=4) | MAX98357A I2S amp (DIN=7/BCLK=15/LRC=16) + INMP441 mic (unused) | `bsp_audio.c` rewritten as bare I2S output with software volume |
| Buttons | 3-key ADC divider (GPIO0) | Onboard BOOT button (GPIO0, single) | `bsp_button.c` rewritten as single GPIO key = OK |
| Battery | CW2017 fuel gauge (I2C) | None | I2C moved to free GPIO8/9; CW2017 absent -> automatic degradation, UI shows "--" |
| Recovery | Passport BLE installer (bootloader hook) | Not part of this ecosystem | bootloader hook removed, long-press jump disabled |
| USB console | USB-Serial-JTAG (GPIO18/19) | USB-Serial-JTAG (GPIO19/20, Type-C direct) | kept `CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG=y` |

## 3. Wiring (per the Feishu tutorial section 2.1, quick reference)

```
ESP32-S3          INMP441 (mic; not used by this firmware)
GPIO4  --->  WS
GPIO5  --->  SCK
GPIO6  --->  SD
3V3    --->  VDD
GND    --->  GND + L/R tied together

ESP32-S3          MAX98357A (amplifier)
GPIO7  --->  DIN
GPIO15 --->  BCLK
GPIO16 --->  LRC
3V3    --->  Vin (SD floating or tied to 3V3 to enable)
GND    --->  GND + GAIN tied together (9 dB)
Speaker +/- ---> amp OUT+/OUT-

ESP32-S3          SPI-LCD (1.59")
GPIO21 --->  SCL(SCK)
GPIO47 --->  SDA(MOSI)
GPIO45 --->  RES
GPIO40 --->  DC
GPIO41 --->  CS
GPIO42 --->  BLK
GND/3V3 ---> GND/VCC
```

> The Passport I2C bus used GPIO7/10; GPIO7 conflicts with this board's
> amplifier DIN, so the port moves I2C to GPIO8/9 (free pins).

## 4. Modified files

| File | Change |
|---|---|
| `sdkconfig.defaults` | C3 -> S3, SPIRAM (octal 8 MB), flash 16 MB, Recovery off, USB-JTAG kept |
| `CMakeLists.txt` (root) | bootloader_components mount removed (Passport Recovery only) |
| `bootloader_components/` | directory deleted |
| `components/bsp/include/bsp_pins.h` | pin map rewritten for the xiaozhi kit; resolution/invert/gap/mirror via Kconfig |
| `components/bsp/src/bsp_audio.c` | rewritten: MAX98357A bare I2S output, mono->stereo expand, software volume; public API unchanged |
| `components/bsp/src/bsp_button.c` | rewritten: single GPIO key (GPIO0 BOOT) = OK |
| `components/bsp/src/bsp_display.c` | mirror/gap/invert via macros; RST=45; comments updated |
| `components/bsp/src/bsp_display_lvgl.c` | 40-row double buffer on S3 (was 20-row single on C3) |
| `components/bsp/CMakeLists.txt` | REQUIRES drops `esp_codec_dev` |
| `components/bsp/idf_component.yml` | `espressif/esp_codec_dev` dependency removed |
| `main/Kconfig.projbuild` | new `FMO_LCD_HEIGHT / INVERT / GAP_X / GAP_Y / MIRROR_X / MIRROR_Y` |
| `main/fmo_ui.c` | layout adapts to `FMO_UI_H`: 320 (host preview) / 280 (1.59" default) / 240 (compact) |
| `tools/check_fmo_config.py` | tolerant of boolean `CONFIG_FMO_*` (`=y/n`) |
| `tests/test_bsp_audio_runtime.py` | stubs rewritten for the MAX98357A driver (I2S out/expand/volume) |
| `.github/workflows/build-firmware.yml`, `.github/workflows/ci.yml` | CI target `esp32c3` -> `esp32s3` |

Untouched: `main/main.c`, `fmo_audio.c`, `fmo_network.c`, `fmo_controls.c` and
all other application code.

## 5. Build and flash

### 5.1 Local build (needs ESP-IDF 5.5.3)

Install ESP-IDF 5.5.3 with the official installer (includes the esp32s3
toolchain), then:

```bash
cd fmo-s3
idf.py set-target esp32s3        # or just delete build/ and build fresh
idf.py menuconfig                # optional tweaks, see 5.2
idf.py build

# Merge bootloader + partition table + app into a single image
idf.py merge-bin -o FoloToy-AI-Passport-full.bin

# Flash via USB-Serial-JTAG (Type-C data cable)
idf.py -p COM<x> flash monitor
```

> `sdkconfig.defaults` already selects esp32s3; `set-target` only (re)generates
> the local `sdkconfig`. First build pulls LVGL, esp_lvgl_port, button etc.
> (network required).

### 5.2 menuconfig display tweaks (the key knobs)

Under `FMO Live Monitor`:

| Option | Default | Meaning |
|---|---|---|
| `FMO_LCD_HEIGHT` | 280 | 1.59" panels: 240x280 (GC9107/ST7789-compatible) -> 280; if your panel is 240x240 choose 240 |
| `FMO_LCD_INVERT` | y | toggle if the picture is negative |
| `FMO_LCD_GAP_X / GAP_Y` | 0 | picture offset (GC9107 often needs 1-2 px) |
| `FMO_LCD_MIRROR_X / MIRROR_Y` | n | horizontal/vertical mirror |
| `FMO_WIFI_SSID/PASSWORD` | empty | optional preset; otherwise use first-boot AP QR provisioning |
| `FMO_HOST / FMO_PORT` | fmo.local / 80 | FMO device address, editable on the setup page |

### 5.3 GitHub Actions build (recommended without local IDF)

The repo ships CI: push `fmo-s3/` to your own GitHub repository (tag push or a
manual `workflow_dispatch`). GitHub builds with ESP-IDF 5.5.3 / esp32s3 and
uploads `build/validation/FoloToy-AI-Passport-full.bin` as an artifact; download
it from the Actions page and flash. The CI image has no preset Wi-Fi — use the
first-boot provisioning AP.

## 6. Single-button constraints

- This board only has the onboard **BOOT button (GPIO0)**, mapped to **OK**:
  - short press: toggle audio
  - long press: station list (monitor view) / back (subpages)
  - setup page short press: switch QR / credentials
- **Up/Down keys (volume +/-10%, long-Up network menu) have no hardware**.
  Volume is preset with `FMO_VOLUME` in menuconfig (default 50%). To adjust
  volume at runtime, add an external button and map it to `BSP_BTN_UP` in
  `bsp_button.c`, or wire the original 3-key ADC divider back in.
- Audio volume is **software** (PCM attenuation); the MAX98357A hardware gain is
  fixed by the GAIN pin (tied to GND = 9 dB).

## 7. Known limitations

1. **No microphone capture** — FMO is a listening display (plays the /audio
   stream); INMP441 may stay unwired or be connected per the tutorial for later.
2. **No battery gauge** (no CW2017 on the kit); the battery icon shows an empty
   "--" outline.
3. The panel is driven by the **ST7789-compatible init sequence** (covers
   GC9107 and similar 1.59" panels). Panels needing a vendor-specific init can
   replace `ST7789P3_CMDS` in `bsp_display.c`.
4. The partition table keeps the original layout (recovery partition remains
   but empty and never jumped); about 1 MB of flash is unused and can be
   reclaimed later.
5. `dependencies.lock` still records `target: esp32c3`; the first build
   regenerates it for esp32s3.

## 8. On-device verification checklist

- [ ] Screen lights up without artifacts/negative/offset (otherwise use 5.2)
- [ ] USB log shows `Display ready 240x280`, `MAX98357A ready 8000Hz/16bit/mono`, `Button ready: GPIO0`
- [ ] First boot enters the provisioning AP (no preset Wi-Fi), phone scans the QR and connects
- [ ] Channel name appears when connected; speaking shows orange callsign + audio bar
- [ ] Conversation audio plays, volume reasonable (software attenuation, no clipping)
- [ ] Short BOOT toggles audio, long BOOT switches stations
- [ ] Dim after 30 s, off after 90 s; key press / speech wakes
- [ ] Audio keeps playing while the display sleeps (FMO stays listening)

## 9. Rollback

`fmo-s3/` is independent of the original download (`src/ai-passport-fmo-main/`).
To restore the C3/Passport build, use the original directory; the two trees do
not interfere.
