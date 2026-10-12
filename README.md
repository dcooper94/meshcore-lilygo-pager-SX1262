<h1 align="center">MeshCore for the LilyGo T-LoRa Pager (SX1262)</h1>

<p align="center">
  A standalone, off-grid <strong>LoRa mesh messenger</strong> firmware for the
  <strong>LilyGo T-LoRa Pager</strong> with the <strong>Semtech SX1262</strong> radio —
  a <a href="https://meshtastic.org">Meshtastic</a> alternative built on
  <a href="https://github.com/meshcore-dev/MeshCore">MeshCore</a>. No phone required.
</p>

<p align="center">
  <a href="https://github.com/dcooper94/meshcore-lilygo-pager-SX1262/releases/latest"><img src="https://img.shields.io/github/v/release/dcooper94/meshcore-lilygo-pager-SX1262?label=release" alt="Latest release"></a>
  <img src="https://img.shields.io/badge/platform-ESP32--S3-3178c6" alt="Platform ESP32-S3">
  <img src="https://img.shields.io/badge/radio-SX1262-FAA61A" alt="Radio SX1262">
  <a href="license.txt"><img src="https://img.shields.io/github/license/dcooper94/meshcore-lilygo-pager-SX1262" alt="License"></a>
</p>

> **This is an SX1262 port** of [CJvanSoest/meshcore-lilygo-pager-LR1121](https://github.com/CJvanSoest/meshcore-lilygo-pager-LR1121).
> If your pager has an **LR1121** radio, use the upstream repo instead.

---

The **LilyGo T-LoRa Pager** is a pocket LoRa handheld with a colour IPS screen, a QWERTY keyboard and a scroll-wheel. This firmware turns it into a fully **standalone MeshCore node** — read and write channel messages and direct messages, watch nodes appear on an offline map, and manage the radio, all on the device itself. No phone, no WiFi, no internet required.

## ✨ Features

- **Tile carousel UI** — Radio · Channels · DM · Contacts · Discovered · Advert · Map · Settings · About · Sat Test · Recent, driven by the scroll-wheel and keyboard; no companion app needed.
- **Channels & direct messages** — hashtag channels and per-contact DMs with an on-screen keyboard, unread badges, and chat history.
- **Per-channel mute** — long-press any channel row (including Public) to mute/unmute notifications.
- **GPS + offline maps** — live position fix, satellite test screen, and a pannable/zoomable offline map rendered from SD-card tiles, with your position and heard nodes drawn on it.
- **RSSI + noise floor + GPS status** displayed in the top status bar.
- **GPS off by default** — enable in Settings when needed; duty-cycles automatically (30 s on / 270 s off) to save battery.
- **Accurate battery %** — reads the BQ27220 coulomb counter; calibration reference persists across reboots via NVS.
- **Side button** — short press = screen on/off; long press (≥ 600 ms) = navigate back.
- **Sounds only for messages** — DMs and channel messages play a tone; adverts, ACKs, and new-node discoveries are silent.
- **On-device radio settings** — frequency, SF, BW, CR, TX power, node name, all editable from the UI.
- **Dark / light themes + brightness control** — persist across reboots.

## 📟 Hardware

| | |
|---|---|
| **Board** | LilyGo T-LoRa Pager (**SX1262** variant) |
| **MCU** | Espressif ESP32-S3 (with PSRAM) |
| **Radio** | Semtech SX1262 — sub-GHz |
| **Display** | Sitronix ST7796 IPS LCD (480 × 222) |
| **Input** | TCA8418 QWERTY keyboard + rotary encoder |
| **Other** | GPS, BQ25896 charger, BQ27220 fuel gauge, XL9555 IO expander, microSD |

## ⚡ Flashing pre-built firmware

1. Download `tpager-sx1262-firmware.zip` from the [**Releases**](https://github.com/dcooper94/meshcore-lilygo-pager-SX1262/releases/latest) page and extract it.
2. Hold **BOOT**, tap **RST**, release **BOOT** to enter download mode.
3. Flash with [esptool](https://github.com/espressif/esptool):

```bash
python -m esptool --chip esp32s3 --port /dev/ttyACM0 --baud 115200 \
  --no-stub --before default_reset --after hard_reset \
  write_flash --flash_mode dio --flash_freq 80m --flash_size 16MB \
  0x0000 bootloader.bin 0x8000 partitions.bin 0x10000 firmware.bin
```

Replace `/dev/ttyACM0` with your port (`COM3`, `/dev/tty.usbmodem*`, etc.).

See the README inside the zip for app-only update instructions (faster, preserves your settings).

## 🛠️ Build from source

Requires [PlatformIO](https://platformio.org/):

```bash
pio run -e T_LoRa_Pager_SX1262_companion_radio_usb
```

See [`variants/lilygo_tlora_pager_sx1262/README.md`](variants/lilygo_tlora_pager_sx1262/README.md) for the full build notes, pin map, and UI reference.

## 🔌 Buttons

The pager has three buttons along the bottom edge:

| Button | Short press | Long press (≥ 600 ms) | Very long press (≥ 3 s) |
|--------|-------------|----------------------|------------------------|
| Left | Reboot | — | — |
| **Middle** (scroll-wheel click) | Confirm / select | Navigate back | **Power off** (from home screen) |
| Right (BOOT) | Screen on / off | Navigate back | — |

> Power-off via the middle button only triggers from the home carousel — holding it inside a chat or subscreen just navigates back, so you can't power off by accident.

## 📚 Documentation

- [Variant README](variants/lilygo_tlora_pager_sx1262/README.md) — build environments, pin map, UI navigation, flashing.
- [Offline map tiles](variants/lilygo_tlora_pager_sx1262/MAPS.md) — how to prepare SD-card map tiles.

## 🌐 Credits & upstream

This is a port of **[CJvanSoest/meshcore-lilygo-pager-LR1121](https://github.com/CJvanSoest/meshcore-lilygo-pager-LR1121)** (LR1121 variant by CJ van Soest), which is itself built on **[MeshCore](https://github.com/meshcore-dev/MeshCore)** by Scott Powell / [rippleradios.com](https://rippleradios.com). All credit for the underlying mesh stack and pager UI foundation goes to those projects. This fork adds SX1262 radio support and the feature additions listed above.

## 📄 License

Released under the **MIT License** — see [`license.txt`](license.txt).
