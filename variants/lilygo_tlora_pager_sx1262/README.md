# LilyGo T-LoRa Pager (SX1262) - MeshCore variant

MeshCore firmware for the LilyGo T-LoRa Pager with the Semtech SX1262
sub-GHz radio. This folder covers all SX1262-specific code and the
on-board hardware: ST7796 IPS display, TCA8418 keyboard, rotary encoder,
BQ25896 charger/PMU, BQ27220 fuel gauge, MicroNMEA GPS, XL9555 IO
expander, ES8311 audio.

## Button layout

Three buttons sit along the bottom edge of the Pager (left to right):

| # | Label | GPIO | Behaviour |
|---|-------|------|-----------|
| 1 | RST | n/a (CHIP_PU) | Hard reset. Hardwired, not programmable. |
| 2 | BOOT | GPIO 0 | **Short press:** toggle sound alerts on/off (toast + top-bar icon). **Long press (>=600 ms):** turn screen off. Pressing while screen is off wakes the display. |
| 3 | PWR | n/a (BQ25896) | Powers the device on from off. Hardwired to PMU, not programmable. |

## Powering the device on and off

* **Power on:** press the PWR button (button 3, rightmost).
* **Power off:** hold the **encoder / scroll button for ≥ 3 seconds**.
  We pull the system rail down via the BQ25896 charger's BATFET_DIS
  bit (`tpager_power_off()` in `target.cpp`, mirrors
  `XPowersLib::PowersBQ25896::shutdown()`).
* The 600 ms encoder hold is reserved for the "back" gesture inside
  the LVGL UI. Only ≥ 3 s actually powers the device down.

## Build environment

| env name | purpose | based on |
|---|---|---|
| `T_LoRa_Pager_SX1262_companion_radio_usb` | standalone client with LVGL tile-carousel UI | `examples/companion_radio`, with this folder's `UITask.cpp` replacing the stock `ui-new/UITask.cpp` |

Pulls in `helpers/ui/LGFXDisplay.cpp` and the variant's own
`TPagerST7796Display.h`, `target.cpp`, `TPagerLVGL.cpp` and `UITask.cpp`.

## Navigation

- **Encoder rotation** scrolls / changes value. Direction is inverted
  while inside sub-screens (vertical lists) so "rotate up" moves the
  focus up.
- **Click** opens a tile, applies a setting, or toggles a boolean
  inline. If the focus didn't land on the row the first click "wakes"
  it — in that case a **double-click** is the natural follow-through.
- **Long-press (≥600 ms)** goes back. From a settings edit popup it
  cancels without applying; from a sub-screen it returns to the
  carousel.

## Tile reference

| # | Tile | Description |
|---|------|-------------|
| 0 | Radio | LoRa radio settings: frequency, SF, BW, CR, TX power, node name, scope, RX boost, path hash. |
| 1 | Channels | Hashtag and private channel list. Add/delete channels, open a chat view, long-press a row to mute/unmute. Clicking "+ Add channel" opens a type picker: **Hashtag** (public, key derived from name) or **Private** (user-supplied 32-hex key, interoperable with the MeshCore app). Private channels show a closed-eye icon. The key entry popup includes a **RAND** button to generate a random key on-device. |
| 2 | DM | Direct messages. Select a favourite contact to open a per-contact chat view. |
| 3 | Contacts | Favourite contacts. Click to open DM or unmark as favourite. |
| 4 | Discovered | All recently heard nodes (50-entry LRU). Add to contacts, mark as favourite, or send a direct/flooded advert. |
| 5 | Advert | Send a self-advert (direct or flood). |
| 6 | Map | Offline map rendered from SD-card XYZ tiles. See [MAPS.md](MAPS.md). |
| 7 | Settings | Global toggles: GPS enable, theme, brightness. |
| 8 | About | Firmware version and node info. |
| 9 | Sat Test | Live GPS satellite reception diagnostic. |
| 10 | Recent | Recently heard nodes. |
| 11 | Repeaters | Heard repeaters sorted by RSSI. Long-press to log in and view management info. |

## Pin map / hardware

Mirrors `Xinyuan-LilyGO/LilyGoLib-PlatformIO/variants/lilygo_tlora_pager/pins_arduino.h`.

- LoRa (SX1262): `CS=36 IRQ=14 RST=47 BUSY=48` on the shared SPI bus
  (`SCLK=35 MOSI=34 MISO=33`).
- Display (ST7796): `CS=38 DC=37 BL=42`. Panel mounted 180° (offset
  rotation 2); LGFXDisplay applies +90° on top, so the effective layout
  is landscape 480×222.
- Encoder: `ROTARY_A=40 ROTARY_B=41 ROTARY_C=7` (push).
- Keyboard (TCA8418 on I²C 0x34): 4×10 matrix.
- I²C bus: `SDA=3 SCL=2`.
- XL9555 IO expander on 0x20 with `EXPANDS_LORA_EN=3`,
  `EXPANDS_KB_EN=8`, `EXPANDS_KB_RST=2` — the LR1121 power rail and
  the keyboard reset are routed through the expander, not direct GPIO.

## Keyboard layout

| layer    | how to engage                       | what it produces                                    |
|----------|-------------------------------------|-----------------------------------------------------|
| QWERTY   | tap a letter key                    | the silk-screened letter (a-z, Enter, Space, Backspace) |
| Symbol   | hold the **orange FN key** next to Z | the silk-screened symbol on the same key            |

Symbol-layer map (matches the physical hardware labels):

```
FN+Q..P → 1 2 3 4 5 6 7 8 9 0
FN+A..L → * / + - = : ' " @
FN+Z..M → _ $ ; ? ! , .
FN+Space → #         (needed for MeshCore channel names)
```

The unmarked key right of M (raw 29) is a sticky caps-lock toggle.

## Repeaters tile

The Repeaters tile (index 11) lists all heard nodes with `ADV_TYPE_REPEATER`,
sorted strongest RSSI first.

- **Long-press** a row to open the login popup for that repeater.
- Passwords are masked with asterisks and persisted to NVS per-repeater
  (keyed by the first 4 bytes of the pub_key). On subsequent visits the
  password auto-fills.
- After a successful login the **management screen** appears with two sections:
  - **Info** — firmware version, node name, owner info.
  - **Stats** — battery voltage, uptime, RX/TX packet counts.
- Encoder scrolls through all rows on the management screen.

## Known LVGL caveats

These cost some debug time and are documented here so the next pair of
hands doesn't have to rediscover them:

- **LVGL needs a tick source.** Call
  `lv_tick_set_cb((uint32_t(*)())millis)` once before `lv_timer_handler`
  — without it timers (including the indev read-cb) never fire.
- **Encoder indev does not raise `LV_EVENT_LONG_PRESSED`** on focused
  widgets. The long-press is reserved for the group's edit-mode
  transition. Detect long-press yourself in the input poll.
- **Encoder `enc_diff` accumulator** needs a cap (`±4` here) and
  `continue_reading=false`, otherwise a fast spin drains the queue in
  one cycle and feels like the focus jumped to the end.
- **`LV_USE_STDLIB_MALLOC=LV_STDLIB_CUSTOM`** in the canonical LilyGo
  `lv_conf.h` expects you to provide `lv_malloc_core`/`lv_free_core`.
  We switched to `LV_STDLIB_BUILTIN` since we don't ship those symbols.
- **Encoder + spinbox: don't call `lv_spinbox_step_prev`** at creation
  time. It bumps the step to 10, which collapses to the range bounds
  for small ranges (SF 5..12 only shows 5 or 12).

## Upcoming

- SD-persisted DM history (needs SD bringup in `target.cpp`)
- Map tile renderer from SD card (see [MAPS.md](MAPS.md); needs SD bringup)
- Settings tile: global toggles (GPS enable, theme, brightness)
- Favourite-first contact eviction when the 350-contact cap is hit
