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

The upstream firmware in `Fork/` works with the same pinout. Changes needed:

- **IMU**: the board carries an LSM6DSOX instead of the ADXL345. Change the object type in `Motion.h` and the `lib_deps` entry in `platformio.ini` to the Adafruit LSM6DSOX library; it exposes the same `sensors_event_t` interface through `Adafruit_Sensor`, so `getEvent()` is unchanged.
- **Optional**: read `PWR_FLT` on IO38 (low = the eFuse cut the power) and drive the debug LEDs on IO33/IO48; set the amplifier gain with IO21/IO47 (never as outputs driven high).
- Keep `ARDUINO_USB_CDC_ON_BOOT=1`: flashing and the serial console go through the native USB.
- If SW1 is used as a configuration switch on IO3, **never burn the `STRAP_JTAG_SEL` eFuse**.

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
