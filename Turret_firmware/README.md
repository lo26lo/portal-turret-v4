# Turret2 firmware

Firmware of the Portal Sentry Turret on the **Turret2** board (ESP32-S3-MINI-1-N8). It started as a copy of the upstream firmware (`../Fork/`, unchanged) and was adapted to the new board: safe power-up, power fault handling, amplifier control, new IMU, staggered servos, settings, web page, serial console, home WiFi and NTP.

> **Status**: it compiles (`turret2` and `turret2_bringup`), but it has **not run on a board yet**. Expect calibration and fixes at bring-up. The web page has been tried against the simulator (`tools/mock_server.py`), not against real hardware.

Design and reasons: [../docs/firmware-plan.md](../docs/firmware-plan.md). Hardware: [../README.md](../README.md), [../docs/design-plan.md](../docs/design-plan.md).

---

## Contents

1. [What the turret does](#1-what-the-turret-does)
2. [Project layout](#2-project-layout)
3. [Build, flash, monitor](#3-build-flash-monitor)
4. [Boot sequence](#4-boot-sequence)
5. [State machine](#5-state-machine)
6. [Modules](#6-modules)
7. [Buttons, switch and status LEDs](#7-buttons-switch-and-status-leds)
8. [Settings](#8-settings)
9. [WiFi, first setup, time](#9-wifi-first-setup-time)
10. [Web page and API](#10-web-page-and-api)
11. [Serial console](#11-serial-console)
12. [Firmware update over WiFi](#12-firmware-update-over-wifi)
13. [Hardware safety rules](#13-hardware-safety-rules)
14. [Calibration and bring-up](#14-calibration-and-bring-up)
15. [Testing without the board](#15-testing-without-the-board)
16. [Known limits](#16-known-limits)

---

## 1. What the turret does

At rest the wings are closed and the ring glows red. When the **HLK-LD2450 radar** sees someone, the turret opens its wings, extends its guns, plays the gunshot sound for 3 s, then retracts the guns and closes the wings. Button A starts the same cycle by hand.

Around that behaviour, which comes from the upstream firmware, Turret2 adds:

- a **safe power-up**: every output that floats after reset is set within the first milliseconds, the servos start one at a time;
- **power protection**: a fault reported by the eFuse sheds the load at once, a reboot loop starts a reduced mode;
- **settings** stored in flash (NVS), changeable from the web page or the console;
- a **web page** (status, settings, calibration, tests, log, firmware update) and a **serial console** with the same commands;
- **home WiFi** next to the turret's own access point, **NTP time**, name `portal-turret.local`.

## 2. Project layout

```
platformio.ini              environments, pinned platform and libraries
boards/turret2.json         board: 8 MB QIO flash, no PSRAM, partitions default_8MB.csv
variants/turret2/           Arduino variant: I2C on IO35/IO36, LED_BUILTIN = IO33, no RGB_BUILTIN
scripts/embed_page.py       pre-build: gzips src/web/page/index.html into src/web/page_gz.h
tools/mock_server.py        web page test bench (simulated turret, no board needed)
data/fire.mp3               LittleFS image (pio run -t uploadfs)
src/
  main.cpp                  setup() = boot steps 1-10, loop()
  pins.h                    pinout imposed by the board (never reassign)
  Turret.h                  references shared by the states
  board/Board.*             safe state, reset reason, reboot-loop detection, status LEDs, buttons, SW1, PWR_FLT
  board/I2cBus.*            I2C start on IO35/IO36, bus recovery, scan
  board/Log.*               console output: Serial + 4 kB RAM buffer (GET /api/log)
  audio/Amp.*               MAX98357A: shutdown, gain, mute
  audio/Audio.*             I2S, gunshot sample with loop, test tone, volume
  audio/AudioLoop.*         plays a sample with a loop section
  audio/GunShotAudio.h      gunshot sample (16-bit mono, 44.1 kHz, 16 420 samples)
  light/Light.*             NeoPixels: ring (9) and guns (2 + 2), power cap, test colours
  motion/ServoChannel.*     one servo output with deferred, staggered attachment
  motion/Gun.*              gun servo (positional)
  motion/Wing.*             wing servo (continuous rotation) + Hall sensor
  motion/Gantry.*           both wings, rotations X/Z, attach schedule, hold policy
  sensors/Motion.*          LSM6DSOX IMU, orientation
  sensors/Radar.*           HLK-LD2450 frame parser (UART1, 256000 baud)
  settings/Settings.*       setting table, NVS storage
  states/*                  state machine: Booting, Idle, Activate, Firing, Disengage, Manual, Fault
  control/Actions.*         command interpreter shared by console, buttons and web page; status JSON
  web/AccessPoint.*         turret access point (WPA2) + captive portal DNS
  web/Station.*             home WiFi, NTP, mDNS, network scan
  web/TurretWebServer.*     web page and JSON API, HTTP Basic authentication
  web/Ota.*                 firmware upload (POST /update)
  web/page/index.html       the web page (embedded in the firmware at build time)
```

## 3. Build, flash, monitor

PlatformIO project. In VS Code, open **this folder** (`Turret_firmware/`), not the repository root: the PlatformIO extension only detects a `platformio.ini` at the root of the opened folder. From the repository root on the command line, add `-d Turret_firmware`.

| Environment | Use |
|---|---|
| `turret2` (default) | normal firmware |
| `turret2_bringup` | debug build: core logs at level 3, button and PWR_FLT events printed, waits up to 3 s for the USB host |
| `turret2_ota` | `turret2` uploaded over WiFi, from the turret's network (section 12) |
| `turret2_ota_home` | same, from the home network (`portal-turret.local`) |

```
pio run                       # build
pio run -t upload             # flash over the native USB (IO19/IO20)
pio run -t uploadfs           # LittleFS image: data/fire.mp3
pio device monitor            # console, 115200 baud
```

If the native USB does not show up: hold **BOOT** (SW2), press **RESET** (SW3), release BOOT, flash again. Last resort: UART0 on J16 with an ESP-Prog.

First flash of a new board: `esptool.py flash_id` must report **8 MB** (module N8). The boot banner also warns when the flash is not 8 MB or when PSRAM is found.

Pinned versions (`platformio.ini`): platform `espressif32@7.1.3` (arduino-esp32 2.0.17), FastLED 3.10.5, ESP32Servo 1.2.1, ESPAsyncWebServer 3.12.1, AsyncTCP 3.5.0, AceRoutine 1.5.1, Adafruit LSM6DS 4.7.4, arduino-audio-tools and arduino-libhelix pinned to a commit. Keep the path of the project short on Windows: FastLED has very deep paths and GCC fails beyond 260 characters.

Size today: about 44 % of the 3.2 MB application partition, 23 % of the RAM.

## 4. Boot sequence

The order comes from the firmware plan §3.2. Steps 1 to 10 run in `setup()`, steps 11 to 13 in the `Booting` state.

| # | Step | Why |
|---|---|---|
| 1 | **Safe state** (`Board::Begin`, first line): amplifier SD low (muted), gain pins released (9 dB), both status LEDs on, inputs configured, SW1 and buttons read, reset reason and counters read | after reset every pin floats; this happens within a few milliseconds |
| 2 | **NeoPixels**: black frame, power cap | clears random colours latched while the level shifter inputs floated |
| 3 | **Console**: `Serial` (USB CDC), boot banner | diagnostics from the start |
| 4 | **Settings** from NVS (factory reset if A + B held), LED settings, **LittleFS** mounted (never formatted automatically) | everything else reads the settings |
| 5 | **I2C** on IO35/IO36 at 400 kHz, bus recovery if SDA is stuck, scan printed | the IMU may hold SDA after a warm reset |
| 6 | **IMU** LSM6DSOX at 0x6A | red LED code 3 if missing |
| 7 | **Radar** UART1 at 256000 baud | non blocking; code 4 if no frame for 3 s |
| 8 | **Hall sensors** read: real wing position | a reset during a cycle can leave the wings open |
| 9 | **Audio**: I2S clocks first, then gain with SD low, 10 ms, SD high | no pop |
| 10 | **WiFi** access point, home WiFi, web server, OTA | the RF calibration current peak happens before the servos |
| 11 | **Servos one at a time**, `ServoStagger` apart (250 ms): rotate Z, rotate X (centred), gun left, gun right (retracted) | several servos jumping together draw 3 to 4 A, the eFuse limit |
| 12 | **Homing**: guns retracted, wings closed unless the Hall sensors say they are | known mechanical state |
| 13 | End: green LED heartbeat, red LED fault code, state `Idle` (or `Manual` in bench / reduced mode) | |

PWR_FLT low at any point stops the sequence and enters `Fault`.

## 5. State machine

| State | Does | Leaves to |
|---|---|---|
| `Booting` | steps 11-13 | `Idle`, `Manual` (bench / reduced mode), `Fault` |
| `Idle` | waits for a radar target; refuses if the turret is not upright | `Activate` |
| `Activate` | opens the wings, waits until both are open (Hall), extends the guns, 0.5 s | `Firing` |
| `Firing` | gunshot sound with its loop for 3 s, then lets it end | `Disengage` |
| `Disengage` | retracts the guns, 0.5 s, closes the wings, waits, 0.5 s | `Idle` |
| `Manual` | nothing: the state machine is stopped. Used by bench mode, reduced mode and every hardware test | `Booting` with the `resume` command |
| `Fault` | power fault: servos detached, sound stopped, amplifier in shutdown, LEDs off | `Booting` after PWR_FLT has been high for 2 s |

The turret does not aim at the target yet: the rotations stay centred, as in the upstream firmware.

## 6. Modules

### Board (`board/Board.*`)

- Safe state at reset (step 1).
- **Reset reason** (`esp_reset_reason`) printed in the banner and the status.
- **Reboot-loop detection**: a counter in NVS goes up at each *suspicious* reset (power-on, brownout, panic, watchdogs, unknown; not software restarts nor deep sleep) and is cleared after 60 s of running. From 3 in a row the turret starts in **reduced mode**: no servo attached, LEDs at 10 %, amplifier at 9 dB, red LED code 1. This covers an eFuse that keeps cutting and retrying. On the ESP32-S3 the RESET button also reports a power-on, so pressing it three times within a minute also gives reduced mode.
- **Brownout counter** in NVS (lifetime).
- **PWR_FLT** (IO38, low = eFuse fault): falling-edge interrupt, so even a short dip is caught, plus the level.
- Status LEDs and buttons (section 7).

### Power handling

A power fault, from any state including `Booting`, enters `Fault` at once (plan §4). The ESP32 is powered behind the eFuse too: if the eFuse cuts completely the board simply restarts, and the reset reason and counters show it afterwards.

LED current is capped by FastLED (`LedMaxmA`, 500 mA by default; 13 LEDs at full white would draw about 0.8 A).

### Servos (`motion/*`)

Six servos: rotate Z (IO7), rotate X (IO6), gun left (IO1), gun right (IO2), wing left (IO4), wing right (IO5). Range 500-2400 µs at 50 Hz.

- **Staggered attachment**: a command sent to a detached servo is stored; `Gantry` attaches waiting servos one per `ServoStagger` window, in the order above, directly on the stored command. This applies at boot and afterwards.
- **Hold policy** (decision D6):
  - wings (continuous rotation) are released as soon as they stop: with no pulse they stop dead, which avoids the creep of a badly centred neutral;
  - guns are released 0.5 s after their last command;
  - rotations are held while a wing is open (aiming) and released after `ServoIdleMs` with the wings closed and still.
- **Wings**: speed ±475 µs around `1500 + WingTrim`. A movement ends when the Hall sensor passes the threshold, or after a 2 s timeout (counted once the servo is attached).
- **Hall sensors** (IO8 left, IO9 right, 3.3 V, 12-bit ADC): thresholds `HallOpenL/R`, `HallCloseL/R`. If the open threshold is below the closed one, the comparisons are reversed (magnet mounted the other way). A sensor stuck at a rail (≤ 15 or ≥ 4080 for 1 s) or not changing during a whole movement sets red LED code 5.

### IMU (`sensors/Motion.*`)

LSM6DSOX at 0x6A on the shared I2C bus (also the Qwiic port J11), 104 Hz, ±4 g, gyro 2000 dps. Read every 20 ms, gravity low-pass filtered. The board is mounted vertically, so the axis pointing up is a setting (`ImuUpAxis`, 0 = not calibrated). Once calibrated, the wings refuse to open when less than 0.8 g lies along that axis (tilt beyond about 37°). With no IMU or no calibration, deployment is allowed.

### Radar (`sensors/Radar.*`)

HLK-LD2450 on UART1 (RX IO17, TX IO18, 256000 baud), three targets per frame (position, speed, resolution). Unchanged parser from upstream, plus a "radar alive" flag.

### Audio (`audio/*`)

MAX98357A on I2S (BCLK IO11, LRCLK IO12, DIN IO10), 44.1 kHz, 16-bit, mono (the amplifier plays the left channel).

- Gunshot: sample in flash, loop between samples 1537 and 10102 while firing, then the tail.
- Gain 9, 12 or 15 dB (`AmpGain`), latched only when leaving shutdown, so a change goes through SD low for 10 ms.
- Software volume (`Volume`, %).
- Test tone (`tone` command).
- Mute (button B) keeps the I2S running and only drives SD low.
- Shutdown before any restart: SD low, then I2S stopped.
- `data/fire.mp3` is mounted and selected as in upstream, but the MP3 player is not used.

### Lights (`light/*`)

WS2812 through the SN74AHCT125 level shifter: ring on IO14 (9 LEDs, GRB; the last one has red and green swapped), left gun IO15 and right gun IO16 (2 LEDs each, RGB). The ring shows red, the guns a flickering fire colour. `LedBright`, `LedMaxmA`, test colours from the console or page.

### Settings (`settings/*`)

Table of typed entries (int, float, bool, string) with default, min and max, stored in NVS namespace `turret`. Values are clamped. Modules keep a copy and reload it when a setting changes (`Actions::ApplyAllSettings`).

### Log (`board/Log.*`)

All messages go through `Log.print*`: to the USB console and to a 4 kB RAM ring buffer read by the web page.

### Commands (`control/Actions.*`)

One interpreter for the console, the buttons and the web page. The web server runs in another task: it only queues command lines (24 × 128 bytes), and `loop()` executes them. Hardware is never touched from the web task.

## 7. Buttons, switch and status LEDs

| Control | Action |
|---|---|
| Button A (IO26) | demo cycle, from `Idle` only, if upright |
| Button B (IO34) short | mute / unmute |
| Button B held 3 s | WiFi on / off (access point and home network) |
| A + B held 1 s at power-up | all settings back to defaults, WiFi passwords included |
| SW1 (IO3) closed at power-up | **bench mode**: no servo attached automatically, state machine stopped (`Manual`), one servo at a time from the console / page |

| LED | Meaning |
|---|---|
| Green (IO33) | steady during boot, then a short flash every second (the loop is alive) |
| Red (IO48) | steady during boot, then blinks the number of the lowest active fault, pause, repeat |

| Red blinks | Fault |
|---|---|
| 1 | brownout, or reboot loop (reduced mode) |
| 2 | eFuse fault (PWR_FLT went low) |
| 3 | IMU missing |
| 4 | radar silent for 3 s |
| 5 | Hall sensor stuck |
| 6 | LittleFS not mounted (run `uploadfs`) |

## 8. Settings

All settings are listed, with their limits, in the *Settings* tab of the web page, and with `get` on the console. The WiFi group is applied at the next boot; the home network also right away with `wifi join`.

| Key | Group | Default | Range | Meaning |
|---|---|---|---|---|
| `AngleOffsetX` | Motion | 0 | -90..90 | rotation X offset (degrees, before the gear ratio) |
| `AngleOffsetZ` | Motion | 0 | -90..90 | rotation Z offset |
| `ImuUpAxis` | Motion | 0 | 0..6 | axis pointing up: 0 off, 1 +X, 2 -X, 3 +Y, 4 -Y, 5 +Z, 6 -Z |
| `AmpGain` | Audio | 9 | 9..15 | amplifier gain, rounded to 9, 12 or 15 dB |
| `Volume` | Audio | 80 | 0..100 | software volume (%) |
| `HallOpenL` / `HallOpenR` | Wings | 2500 | 0..4095 | ADC value beyond which the wing is open |
| `HallCloseL` / `HallCloseR` | Wings | 1500 | 0..4095 | ADC value beyond which the wing is closed |
| `WingTrimL` / `WingTrimR` | Wings | 0 | -200..200 | stop point of the wing servo, µs around 1500 |
| `ServoStagger` | Power | 250 | 50..1000 | ms between two servo attachments |
| `ServoIdleMs` | Power | 5000 | 0..60000 | rotations released after this idle time (0 = never) |
| `LedBright` | Lights | 255 | 0..255 | LED brightness |
| `LedMaxmA` | Power | 500 | 100..1500 | LED current limit (mA) |
| `ApSsid` | WiFi | Portal Turret | 1..32 chars | name of the turret's network |
| `ApPassword` | WiFi | stillalive | 8..63 chars | turret network **and** web page password |
| `StaSsid` | WiFi | (empty) | 0..32 chars | home network; empty = not used |
| `StaPassword` | WiFi | (empty) | empty or 8..63 | home network password |
| `Timezone` | WiFi | `CET-1CEST,M3.5.0,M10.5.0/3` | POSIX TZ | time zone (Paris, with daylight saving time) |

Passwords are never sent back by the API nor printed by `get`.

## 9. WiFi, first setup, time

- **Turret network**: access point `ApSsid`, WPA2 with `ApPassword`, address `192.168.4.1`. It stays on even when the home network is connected, so the turret can always be reached.
- **Captive portal**: every DNS name answers `192.168.4.1`, and the connectivity checks of Android, iOS / macOS and Windows are redirected to the page. A phone joining the network opens the page by itself.
- **First setup**: in the *Home WiFi* card at the top of the page, *Scan*, pick the network, type its password, *Connect*. The turret joins it straight away (no reboot) and shows its IP address.
- **Home network** (station mode): automatic reconnection, host name `portal-turret`, mDNS **`http://portal-turret.local`**.
- **Time**: NTP (`pool.ntp.org`, `time.google.com`) as soon as the home network is up, local time with `Timezone`. Shown on the page and by `time`.
- The radio has a single channel: in AP + STA mode the access point follows the channel of the home network. A phone on the turret's network may drop for a second when the turret joins the home network.
- Button B held 3 s, or `wifi off`, turns both off.

## 10. Web page and API

`http://192.168.4.1` (turret network) or `http://portal-turret.local` (home network). User **`turret`**, password **`ApPassword`**. The page is embedded in the firmware (gzip, about 6 kB), needs no Internet and is always the version of the firmware.

Tabs: **Status** (home WiFi card, values refreshed every second, active faults, warning while the factory password is used), **Settings** (generated from the setting table, default button per field, save and reset per group), **Calibration** (Hall capture, IMU axis capture, wing trim sliders), **Tests** (wings, guns, tone, gain, mute, LED colours, any servo, I2C scan, demo, resume, free command line), **Log**, **Maintenance** (firmware upload, reboot, factory reset).

| Method and path | Role |
|---|---|
| `GET /` | the page |
| `GET /api/status` | JSON: version, uptime, state, reset reason, counters, PWR_FLT, modes, active faults, radar, IMU, Hall, wings, servo pulses (`null` = released), amplifier, WiFi (access point, home network), time, free heap |
| `GET /api/settings` | the setting table (key, label, group, type, value, default, min, max, `reboot`, `secret`) |
| `POST /api/settings` | form fields `key=value`; each becomes a `set` command (answer in the log) |
| `POST /api/settings/reset` | optional field `group`; all settings otherwise |
| `POST /api/action` | field `cmd`: any console command |
| `GET /api/wifi/scan` | `{"scanning":bool,"networks":[{ssid,rssi,secure}]}` after `wifi scan` |
| `GET /api/log` | last 4 kB of the log |
| `POST /api/reboot` | reboot with the shutdown sequence |
| `POST /update` | firmware upload (section 12) |

Every route needs the credentials, except the captive portal checks (`/generate_204`, `/hotspot-detect.html`…), which only redirect to the page. Commands are queued and executed by `loop()`: `POST` answers `202 {"queued":n}` and the result appears in the log.

## 11. Serial console

USB, 115200 baud. Type `help`.

| Command | Does |
|---|---|
| `status` | the status JSON |
| `scan` | I2C scan |
| `imu` | gravity, dominant axis, upright, gyro, temperature |
| `hall` | raw Hall values and positions |
| `flt` / `reset-reason` | PWR_FLT and fault count / reset reason and counters |
| `get [key]` / `set <key> <value>` | read / change a setting (saved in NVS, applied at once except WiFi) |
| `reset-settings [group]` | defaults |
| `servo <0-5\|rotz\|rotx\|gunl\|gunr\|wingl\|wingr> <angle 0-180 \| µs 500-2400 \| off>` | drive one servo |
| `wings open\|close`, `guns extend\|retract` | move the mechanics |
| `trim left\|right <µs>` | runs a wing at its stop point with this trim for 2 s (not saved) |
| `led ring\|left\|right\|all <RRGGBB\|off>` | test colours |
| `tone <Hz> [ms]`, `gain 9\|12\|15`, `mute [on\|off]` | audio tests (gain not saved) |
| `wifi [on\|off\|scan\|join]`, `time` | WiFi state and control, local time |
| `demo` | one activation cycle (from `Idle`) |
| `resume` | leaves the tests: homing, then `Idle` |
| `reboot` | restart with the shutdown sequence |

Hardware tests stop the state machine (`Manual`); they are refused during `Booting` and `Fault`. In bench mode, driving a servo releases the others.

## 12. Firmware update over WiFi

It works from the turret's network and, once the turret has joined it, from the home network: no need to switch the computer to the turret's WiFi.

From the page (either network): *Maintenance*, choose `.pio/build/turret2/firmware.bin`, *Upload*. With PlatformIO:

```
$env:TURRET_PASSWORD = "stillalive"      # PowerShell: your ApPassword
pio run -e turret2_ota_home -t upload    # from the home network (portal-turret.local)
pio run -e turret2_ota -t upload         # from the turret's network (192.168.4.1)
```

If `portal-turret.local` does not resolve on the computer, use the IP address shown on the page: `curl -u turret:<password> -F "firmware=@.pio/build/turret2/firmware.bin" http://<ip>/update`.

The credentials are checked on the first block of the upload. The turret then stops (servos detached, amplifier off, I2S stopped, LEDs off) **before** accepting the data, and reboots afterwards whether the update succeeded or not.

## 13. Hardware safety rules

These rules protect the board; the code follows them and any change must too (details: plan §2).

- **IO21 and IO47 are never driven high** (amplifier gain, one of them compared against 5 V): open drain or input only. The variant has no `RGB_BUILTIN` for that reason.
- Never stop the I2S clocks, change the sample rate or restart while the amplifier runs: SD low first (`Amp::Shutdown`).
- No PSRAM (IO26 is button A), never `memory_type opi` (IO33-IO37).
- I2C always on `PIN_SDA` / `PIN_SCL` (IO35 / IO36), never on the defaults of a generic variant (IO8 / IO9 are the Hall sensors).
- Never use IO0, IO19, IO20, IO45, IO46.
- **Never burn an ESP32 eFuse** (`espefuse.py summary` only). Burning `STRAP_JTAG_SEL` would take IO3 (SW1) away.
- Do not power the servos from a computer USB port (0.5 to 0.9 A): use bench mode (SW1) there.
- Never connect the on-board USB-C and the remote USB (J1) to two hosts at the same time.

## 14. Calibration and bring-up

Plan §8 gives the full bring-up order (one peripheral at a time). Use `turret2_bringup` for the first boards. To calibrate, all from the *Calibration* tab:

1. **Hall sensors**: move a wing fully open, *capture open*; fully closed, *capture closed*; same for the other wing; *Compute and save*. Thresholds are set at 75 % (open) and 25 % (closed) between the two values.
2. **IMU**: turret standing upright, *capture*. The dominant axis becomes `ImuUpAxis`.
3. **Wing trims**: move the slider until a wing driven at its stop point does not move, *Save trims*.
4. **Axis offsets** (`AngleOffsetX/Z`) as on the V4.
5. Change **`ApPassword`**.

## 15. Testing without the board

`tools/mock_server.py` (Python 3, standard library only) serves the real page and simulates the API: moving Hall values, wings, servos, log, settings read from `Settings.cpp`, network scan and home WiFi (password `password123`), captive portal checks.

```
python tools/mock_server.py          # then http://localhost:8080, turret / stillalive
```

Extra console commands of the simulator: `sim fault on|off`, `sim radar on|off`, `sim imu on|off`.

## 16. Known limits

- Nothing has run on a board: every timing, threshold and default is a starting point.
- The turret does not aim: the rotations are only centred.
- `data/fire.mp3` and the MP3 decoder are compiled but unused (upstream leftover).
- The web page uses HTTP Basic over plain HTTP: fine on a home network, not on an open one.
- On iOS the captive portal window may not handle the password prompt: open Safari on `192.168.4.1` instead.
- The reboot-loop counter also counts three presses of RESET within a minute.
