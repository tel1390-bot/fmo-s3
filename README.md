<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

<div align="center">

# FMO Live Monitor for AI Passport

**An always-on status screen for your FMO repeater — now ported to the ESP32-S3 xiaozhi breadboard kit.**

| Build | Release | Framework | Display |
|---|---|---|---|
| [![CI](https://img.shields.io/badge/CI-passing-2ea44f)](https://github.com/tel1390-bot/fmo-s3/actions) | [v1.0.0](https://github.com/tel1390-bot/fmo-s3/releases) | ESP-IDF 5.5.3 | 1.59" SPI-LCD 240×280 |

</div>

Watch the FMO channel live: the callsign currently speaking, the last-heard
callsign, the three most recent QSO records, connection state, a PCM audio level
bar, time and battery — all on a small color LCD. The firmware observes the
**local FMO web interface** (`/events`, `/ws`, `/audio`); no FMO credentials or
certificates are stored on the device.

---

## Features

- 🗣 **Live callsign display** — bold orange while speaking, red on `crossServer`,
  white for the last-heard station; grid retained after release
- 🕘 **QSO history panel** — newest three contacts with Beijing time
- 📶 **Audio level bar** — RMS of the conversation PCM with a quick-rise/smooth-fall meter
- 🔇 **Software volume** — toggle audio with one key; preset `CONFIG_FMO_VOLUME` (50%)
- 🌐 **QR provisioning** — first boot hosts `FMO-Setup-XXXX` + `192.168.9.1` page,
  saves up to 5 Wi-Fi profiles, `fmo.local:80` FMO target (editable)
- 🔋 **Battery capsule** — percentage, red ≤20%, outline when unknown
- 😴 **Display power saving** — dims at 30 s, sleeps at 90 s, key/speech wake
- 🔧 **Kconfig display tuning** — height / invert / gap / mirror for your panel

## Supported hardware

| Board | Status |
|---|---|
| **ESP32-S3-DevKitC-1 (N16R8) + 1.59" SPI-LCD + MAX98357A + INMP441** | ✅ primary target (this port) |
| FoloToy AI Passport (ESP32-C3) | original target ([upstream](https://github.com/LeopardDennis/ai-passport-fmo)) |

### Wiring (xiaozhi breadboard kit)

| Module | Pins |
|---|---|
| LCD (SPI) | SCK=`21` MOSI=`47` RES=`45` DC=`40` CS=`41` BL=`42` |
| MAX98357A amp | DIN=`7` BCLK=`15` LRC=`16` |
| INMP441 mic | WS=`4` SCK=`5` SD=`6` (not used by FMO) |
| Button | onboard BOOT (GPIO0, single) |

Full port notes: [`docs/FMO_S3_PORT.md`](docs/FMO_S3_PORT.md).

## Quick start

1. **Flash** the firmware from the latest
   [Release](https://github.com/tel1390-bot/fmo-s3/releases) (merge image →
   address `0x0`):
   ```bash
   python -m pip install esptool
   python -m esptool --chip esp32s3 -p COM12 -b 921600 write_flash 0x0 FoloToy-AI-Passport-full.bin
   ```
2. **Provision**: on first boot the board opens hotspot `FMO-Setup-XXXX` (random
   password shown on the LCD). Join it from your phone, open `http://192.168.9.1`,
   enter your 2.4 GHz Wi-Fi SSID/password, save. The hotspot closes and the
   monitor connects to `fmo.local:80`.
3. **Listen**: the screen shows the channel name and the current speaker. Conversation
   audio plays at 50% volume by default.

### Single-button controls (this board)

| Gesture | Action |
|---|---|
| Short press BOOT | toggle audio |
| **Double press BOOT** | open the network menu → provision again (short press confirms) |
| Long press BOOT (700 ms) | station list (monitor) / back (sub-pages) |

> No UP/DOWN keys on this board: volume is preset via `CONFIG_FMO_VOLUME`
> (default 50%). Screen-off first press only wakes the display; press again to control.

## Build

```bash
# ESP-IDF 5.5.3 (includes esp32s3 toolchain)
idf.py set-target esp32s3
idf.py menuconfig        # FMO Live Monitor: display/volume/network settings
idf.py build
idf.py merge-bin -o FoloToy-AI-Passport-full.bin
idf.py -p COM12 flash monitor
```

GitHub Actions builds every push with ESP-IDF 5.5.3 / esp32s3 and uploads the
validation firmware artifact (`build/validation/FoloToy-AI-Passport-full.bin`,
no embedded credentials).

## menuconfig options (FMO Live Monitor)

| Option | Default | Meaning |
|---|---|---|
| `FMO_LCD_HEIGHT` | 280 | panel height (240 / 280 / 320 preview) |
| `FMO_LCD_INVERT` | y | toggle if the picture is negative |
| `FMO_LCD_GAP_X / GAP_Y` | 0 | pixel offset (some panels need 1–2) |
| `FMO_LCD_MIRROR_X / MIRROR_Y` | n | horizontal / vertical mirror |
| `FMO_VOLUME` | 50 | initial software volume % |
| `FMO_WIFI_SSID / PASSWORD` | empty | optional preset (do not publish) |
| `FMO_HOST / FMO_PORT` | fmo.local / 80 | FMO target (also editable on setup page) |

## Display notes

- Status text is Chinese; callsigns, network names and addresses stay as-is.
- Unknown metadata is never simulated; `--` until the first contact.
- Audio plays even while the LCD sleeps; Wi-Fi modem sleep is disabled during
  playback for lower jitter.
- Time is Beijing time (UTC+8) via SNTP; `--:--` until the clock is valid.

## Compatibility & reliability

- Works with the local FMO interface used by community web clients
  (`parse_fmo_message()` in `main/fmo_network.c` if fields change).
- All three sockets (events / control / audio) ping every 10 s and reconnect
  automatically; metadata recovery never mutes healthy audio.
- Wi-Fi: last-successful first, scan-ordered retry, 30 s between rounds; up to 5
  saved profiles in `fmo_wifi` NVS; WPA2/open supported.
- Audio ring: 24 KiB queue (~1.5 s), whole-message reservation, backpressure
  instead of burst drops; mute/offline clears queued audio.
- Fresh-install data reset is carried by the **complete merged package**
  (bootloader + partition + app), never by flashing only the app.

## Hardware validation still recommended

- Confirm channel/UI after boot and after an FMO-side channel change.
- Key and release a radio: check speaking indicator, callsign transitions, grid retention.
- Leave both devices running through a Wi-Fi interruption; confirm auto-reconnect.
- Check USB logs: heap minimum, largest allocation, stack headroom, button response, display clipping.

## Credits

- Upstream: [LeopardDennis/ai-passport-fmo](https://github.com/LeopardDennis/ai-passport-fmo)
- Base project: [FoloToy/ai-passport](https://github.com/FoloToy/ai-passport)
- Interface reference: [FMO web client API](https://github.com/niufox/fmo-mobile-controller/blob/main/API_DOCUMENTATION_v2.md)

> Security: never publish Wi-Fi passwords, FMO secrets, certificates or
> unsanitized logs. CI artifacts contain dummy network settings only.
