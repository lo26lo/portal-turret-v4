# Turret2 — custom ESP32-S3 controller board for the Portal Sentry Turret v4

![Turret2 v0.1 — KiCad 3D render](Pictures/pcb.jpg)

This fork adds **Turret2**, a custom 4-layer controller PCB for [joranderaaff/portal-turret-v4](https://github.com/joranderaaff/portal-turret-v4). It replaces the Wemos S3 mini and the hand wiring of the original build with a single board that has on-board power protection, audio amplifier, IMU, level shifting for the NeoPixels and keyed connectors for every peripheral.

The board keeps the **exact GPIO mapping of the firmware's `Fork/src/pins.h`**, so the upstream firmware runs on it with one small change (the IMU, see [Firmware](#firmware)).

> **Status: v0.1 — design complete, not yet fabricated or tested.** DRC is clean (0 errors, 0 unconnected items). Use at your own risk until a first batch has been built and validated.

---

## Features

| Block | Implementation |
|---|---|
| MCU | **ESP32-S3-MINI-1-N8** (8 MB flash, no PSRAM — the N8 variant is mandatory, IO26 is used), antenna overhanging the board edge |
| Power input | USB-C, **5 V only, no battery**. Use a 5 V / 3 A wall charger |
| Protection | **TPS259573 eFuse**: 3.86 A current limit, 6.0 V overvoltage cut-off, soft start, fault output on IO38; SMAJ5.0A TVS; USBLC6-2SC6 ESD on D+/D− |
| 5 V distribution | star from the eFuse output: separate servo and logic branches (separate L3 planes), 2 × 220 µF OS-CON polymer bulk on the servo branch |
| 3.3 V | TPS631000 buck-boost (1.5 A), stays stable when servos pull the 5 V down |
| Audio | MAX98357A I²S class-D amplifier on 5 V (3.2 W into 4 Ω), gain selectable from firmware (9 / 12 / 15 dB) |
| IMU | **LSM6DSOX** accelerometer + gyroscope on I²C, address 0x6A (replaces the ADXL345) |
| NeoPixels | SN74AHCT125 level shifter on the three data lines (ring, left gun, right gun) |
| Servos | 6 servo outputs (wings, rotation X/Z, two guns) |
| Sensors | HLK-LD2450 radar (UART1), two Hall sensors powered from **3.3 V** (fixes the V4 bug where 5 V sensors drove the ADC pins) |
| Expansion | Qwiic / STEMMA QT port (I²C, 3.3 V) |
| Debug | native USB (flashing + serial console), backup UART0 connector (TXD0, RXD0, GND, EN, IO0), BOOT and RESET buttons, green debug LED (IO33), red fault LED (IO48) |
| Remote USB | 4-pin connector for a remote USB-C socket (e.g. Adafruit ADA6050) reachable in the enclosure |

## Board

- 4 layers (L2 solid GND, L3 power planes), 1.6 mm, 0.5 oz inner copper.
- Same trapezoid outline and M2 mounting holes (H1/H2) as the V4 board, so the printed mount stays compatible. The top-left corner is notched under the antenna.
- Designed to be mounted vertically; connectors on both sides.
- Mostly 0603 passives, hand-solderable; SMD assembly on a hot plate with a stencil. H1/H2 have 2.2 mm paste openings so the stencil can be aligned on two pins.
- KiCad 10.0.6.

## Connectors

All wire-to-board connectors are **2 mm JST-PH** (vertical), except Qwiic and UART0 (**1 mm JST-SH**), the buttons (2-pin, 2 mm pitch) and the speaker (3.5 mm screw terminal). The servo cables therefore need JST-PH housings crimped on (26 AWG or thicker).

| Ref. | Silkscreen | Pins | Side |
|---|---|---|---|
| J7 | WING_LEFT | 1 GND · 2 +5 V · 3 PWM IO4 | front |
| J8 | WING_RIGHT | 1 GND · 2 +5 V · 3 PWM IO5 | back |
| J10 | ROTATE_X | 1 GND · 2 +5 V · 3 PWM IO6 | front |
| J9 | ROTATE_Z | 1 GND · 2 +5 V · 3 PWM IO7 | front |
| J3 | GUN_LEFT | 1 +5 V · 2 servo PWM IO1 · 3 LED data (IO15 via AHCT) · 4 GND | front |
| J4 | GUN_RIGHT | 1 +5 V · 2 servo PWM IO2 · 3 LED data (IO16 via AHCT) · 4 GND | back |
| J14 | Ring Led | 1 +5 V · 2 LED data (IO14 via AHCT) · 3 GND | front |
| J17 | HALL_L | 1 +3.3 V · 2 HALL_LEFT IO8 · 3 GND | front |
| J18 | HALL_R | 1 +3.3 V · 2 HALL_RIGHT IO9 · 3 GND | back |
| J5 | HLK-LD2450 | 1 +5 V · 2 IO17 (ESP32 RX ← radar TX) · 3 IO18 (ESP32 TX → radar RX) · 4 GND | back |
| J11 | I2C extender | Qwiic: 1 GND · 2 +3.3 V · 3 SDA IO35 · 4 SCL IO36 | front |
| J16 | UART0 | TXD0 (IO43), RXD0 (IO44), GND, EN, IO0 — no 3.3 V | back |
| J12 / J13 | ButtonA / ButtonB | button to GND on IO26 / IO34 (1 kΩ series, 10 k pull-up, 100 nF) | back |
| J15 | Speaker | OUT+ / OUT− (BTL — never tie OUT− to ground), 4–8 Ω | front |
| J1 | USB extender | VBUS, D+, D−, GND to a remote USB-C socket | front, label on back |

**Check the pin order against the schematic before crimping cables.** Servo, gun, ring and Hall connectors share the same family: use different cable colours. A mix-up can never put 5 V on IO8/IO9, but a Hall sensor plugged into a servo socket would be reverse-powered.

Never connect the on-board USB-C and the remote USB socket to two hosts at the same time (the two VBUS would be in parallel).

## GPIO summary

Pins marked *pins.h* are identical to upstream.

| GPIO | Function | | GPIO | Function |
|---|---|---|---|---|
| IO1 | gun left servo (*pins.h*) | | IO17 / IO18 | radar UART1 RX / TX (*pins.h*) |
| IO2 | gun right servo (*pins.h*) | | IO19 / IO20 | native USB D− / D+ |
| IO4 / IO5 | wing left / right servo (*pins.h*) | | IO21 / IO47 | amplifier gain (never drive high) |
| IO6 / IO7 | rotate X / Z servo (*pins.h*) | | IO26 / IO34 | buttons A / B |
| IO8 / IO9 | Hall left / right, ADC1 (*pins.h*) | | IO33 | green debug LED |
| IO10 / IO11 / IO12 | I²S DIN / BCLK / LRCLK (*pins.h*) | | IO35 / IO36 | I²C SDA / SCL (*pins.h*) |
| IO13 | amplifier SD_MODE (enable) | | IO37 | IMU INT1 (spare) |
| IO14 / IO15 / IO16 | NeoPixel ring / left / right (*pins.h*) | | IO38 | eFuse fault, active low |
| IO3 | SW1 (see open items) | | IO48 | red fault LED |

IO39–IO42 are unused (the JTAG header was removed). IO0, IO3, IO45 and IO46 are boot straps.

## Firmware

`Turret_firmware/` is the upstream firmware adapted to Turret2 (Turret2 only, no Wemos build). **Everything it does is described in [Turret_firmware/README.md](Turret_firmware/README.md)**; the design is in [docs/firmware-plan.md](docs/firmware-plan.md). **It compiles but has not run on a board yet**: expect calibration and fixes at bring-up.

### Build and flash

PlatformIO project; open `Turret_firmware/` itself as the folder in VS Code (the PlatformIO extension only detects a `platformio.ini` at the root of the opened folder). All versions are pinned in `platformio.ini`.

| Environment | Use |
|---|---|
| `turret2` (default) | normal firmware |
| `turret2_bringup` | same, with verbose core logs, button / PWR_FLT events on the console, and a 3 s wait for the USB host |
| `turret2_ota` | `turret2` uploaded over WiFi (set `TURRET_PASSWORD` first, see below) |

```
pio run -d Turret_firmware -e turret2 -t upload     # firmware, native USB
pio run -d Turret_firmware -e turret2 -t uploadfs   # LittleFS image (data/fire.mp3)
pio device monitor -d Turret_firmware               # console, 115200
```

First flash: check that the module is an N8 (`esptool.py flash_id` must report 8 MB; the boot banner warns otherwise). If native USB does not enumerate, hold BOOT (SW2) and press RESET (SW3), or use UART0 on J16. Never burn an ESP32 eFuse (`espefuse.py summary` is read-only).

Board definition: `boards/turret2.json` (ESP32-S3-MINI-1-N8, 8 MB QIO flash, **no PSRAM**: IO26 is button A) and `variants/turret2/pins_arduino.h` (I²C on IO35/IO36, no `RGB_BUILTIN`: IO47 is an amplifier gain pin that must never be driven high). The full pinout is in `src/pins.h`.

### What it does at boot

The order of the firmware plan §3.2: outputs to a safe state within a few milliseconds (amplifier in shutdown, gain pins released, status LEDs on), black frame on the NeoPixels, console, settings, LittleFS, I²C (bus recovery + scan), IMU, radar, Hall sensors, audio (the amplifier leaves shutdown only once the I²S clocks run), WiFi and web server, then the **servos one at a time** (250 ms apart, PWR_FLT checked in between) and homing of the wings.

- **IMU**: LSM6DSOX at 0x6A through the Adafruit LSM6DS library. Note that `getEvent()` takes three pointers there (accelerometer, gyro, temperature). The wings refuse to open when the turret is not upright, once the up axis has been calibrated.
- **Power**: a fault on PWR_FLT (IO38, even a short one) sheds the load at once: servos detached, amplifier in shutdown, LEDs off. The turret resumes 2 s after PWR_FLT goes high again. Three suspicious resets in a row (brownout, crash, power-up) within a minute start a **reduced mode**: no servo, LEDs at 10 %, 9 dB.
- **Servos at rest**: wings are released as soon as they stop, guns 0.5 s after moving, rotations after `ServoIdleMs` with the wings closed; they are re-attached on their last position, one at a time.

### Buttons, switch and LEDs

| Control | Action |
|---|---|
| Button A | demo cycle (open, fire, close) |
| Button B | mute / unmute; held 3 s: WiFi access point on / off |
| A + B held at power-up | all settings back to their defaults (WiFi password included) |
| SW1 closed at power-up | bench mode: no servo attached, state machine stopped, one servo at a time from the console or web page |

Green LED: steady during boot, then a heartbeat. Red LED: steady during boot, then a blink code, lowest number first: 1 brownout / reboot loop, 2 eFuse fault, 3 IMU missing, 4 radar silent, 5 Hall sensor stuck, 6 LittleFS not mounted.

### Web page and console

Connect to the WiFi network **Portal Turret** (WPA2, password `stillalive`): the phone opens the page by itself (captive portal), otherwise browse to `http://192.168.4.1` (user `turret`, same password).

**First setup**: in the *Home WiFi* card at the top of the page, press *Scan*, pick your network, type its password and press *Connect*. The turret joins it straight away (its own network stays on), sets its clock from the Internet (NTP, time zone in the `Timezone` setting, Paris by default) and can then be reached from your home network at `http://portal-turret.local` or at the IP address shown on the page. In AP + STA mode the radio has one channel: a phone on the turret's network may drop for a second when the turret joins the home network. The page shows the status and faults, lets you change every setting, calibrate the Hall sensors, the IMU axis and the wing trims, run hardware tests, read the log and upload a firmware. **Change `ApPassword`** (8 to 31 characters, applied at the next boot).

The same commands are available on the USB console (type `help`): `status`, `scan`, `imu`, `hall`, `servo <n> <angle|µs|off>`, `wings open|close`, `guns extend|retract`, `led all FF0000`, `tone 1000`, `gain 12`, `mute`, `get` / `set <key> <value>`, `resume`, `reboot`…

OTA from PlatformIO: join the access point, then

```
$env:TURRET_PASSWORD = "stillalive"    # PowerShell; your ApPassword
pio run -d Turret_firmware -e turret2_ota -t upload
```

The turret stops before accepting the upload and always reboots afterwards.

### To calibrate on the first board

Hall thresholds (`HallOpenL/R`, `HallCloseL/R`: the sensors now run at 3.3 V; the V4 values are only a starting point), wing trims (`WingTrimL/R`, until a stopped wing does not creep), IMU up axis (`ImuUpAxis`), and the axis offsets as on the V4. All of them are in the Calibration tab of the web page.

## Open items before the first order

- **SW1** (former USB/JTAG selector on IO3): keep it as a configuration switch read by the firmware, or leave it and R9 unpopulated.
- **BOM**: set the U7 value / MPN to **ESP32-S3-MINI-1-N8**. For JLCPCB assembly, LCSC part numbers still have to be added, and three SMD parts sit on the back (J16, R13, R17).
- Order: 4 layers, 1.6 mm, ENIG recommended, stencil 0.10–0.12 mm with the comment "keep the 2 non-pad openings (alignment holes)".

## Repository layout

```
Turret2_portable/          KiCad project (schematic, PCB, custom DRC rules)
Turret2_portable/library   project-specific symbols, footprints and 3D models
docs/design-plan.md        full design rationale, section by section
docs/status-and-history.md current status, decisions and pitfalls, session by session
Turret_firmware/           Turret2 firmware (PlatformIO project), work in progress — see docs/firmware-plan.md
Fork/                      upstream firmware, kept unchanged for reference
Pictures/                  photos and renders
```

Open `Turret2_portable/Turret2.kicad_pro` with **KiCad 10.0.6 or later**. The PCB embeds all its footprints, so it opens as is. To edit the schematic or run *Update PCB from Schematic*, the project-specific libraries (`samsic` SamacSys imports such as `USB412003C` and `LGA-14_1`, and `TC2050-IDC`) must be available as project libraries.

## Documentation

- [docs/design-plan.md](docs/design-plan.md) — why every part and value was chosen: power path and eFuse settings, audio, IMU, connectors, stack-up, routing rules, custom DRC rules, JLCPCB order options, pre-fabrication checklist, community feedback on the V4. It keeps the early reference designators; §0c lists the actual ones.
- [docs/status-and-history.md](docs/status-and-history.md) — current state of the board, what changed in each session, and a list of KiCad and pcbnew-scripting pitfalls.

## Credits and license

- Original project, firmware, 3D parts and V4 board: **joranderaaff** — [joranderaaff/portal-turret-v4](https://github.com/joranderaaff/portal-turret-v4). The board outline and mounting holes are taken from the V4 gerbers so that the original mount fits.
- Community feedback from the portal-turret V4 discussions is summarised in design-plan §12b.
- Turret2 board design: [lo26lo](https://github.com/lo26lo).

The upstream repository does not include a license file at the time of writing; its files remain under their author's terms. The license for the Turret2 hardware files in `Turret2_portable/` is still to be added.
