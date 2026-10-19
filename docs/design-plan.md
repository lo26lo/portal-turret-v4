# Turret2 — ESP32-S3 board for the Portal Turret v4: full design plan

Project: `Turret2.kicad_pro` (KiCad 10). Target firmware: [joranderaaff/portal-turret-v4](https://github.com/joranderaaff/portal-turret-v4), reused as is. This document supersedes all earlier versions. **Updated 20.09.2026**: +5 V star, L3 zones and bulk capacitors done on the board (§9.1b). **21.09**: removal of J12, J13, J16, R13 and R17 recorded (§1, §3.5, §12, §12b.4). **23.09**: board routed and checked; **24.09**: Hall sensors split onto J17/J18 — see §0c, which takes precedence over the rest of this document wherever they disagree.

> Dates are in DD.MM.YYYY format. This plan was written before and during layout; it keeps the **old reference designators** used in the early schematics. The actual designators on the board are listed in §0c.

---

## 0c — Status on 24.09.2026: board routed

Placement and routing finished on 22.09, followed by inspection and fixes (details in [status-and-history.md](status-and-history.md), §9 and §10). **DRC: 0 errors, 0 unconnected items**, silkscreen harmonised on the evening of 23.09. What changed compared with this plan:

- **JTAG removed** (§3.4): no TC2050 on the board, J2 excluded. IO39–IO42 left free (not connected).
- **UART0 back** (§3.5): J16, 5-pin JST-SH on the back — TXD0, RXD0, GND, EN, IO0, no 3V3. Automatic flashing with an ESP-Prog; with a plain USB-serial adapter use the BOOT/RESET buttons, and never wire DTR/RTS directly to EN/IO0.
- **Buttons back**: J12/J13 (2-pin headers, 2 mm pitch, on the back) on IO26/IO34, with 1 kΩ in series, 10 k pull-up and 100 nF.
- **SW1 (former JTAG selector, IO3)**: still to be decided — either a configuration switch read by the firmware, or removed. In the first case, **never burn STRAP_JTAG_SEL**.
- **Connectors**: radar moved to a 4-pin JST-PH on the back; **Hall sensors split** (24.09) onto two 3-pin JST-PH, J17 HALL_L on the front and J18 HALL_R on the back (1 = 3V3, 2 = signal, 3 = GND, one 100 nF each: C31, C32) — one straight cable per sensor, in a different colour from the servo cables; Qwiic on JST-SH (MP tied to ground).
- **Actual designators on the board** (this plan keeps the old ones): IMU LSM6DSOX = **U6**, MAX98357A amplifier = **U9**, AHCT125 level shifter = **IC2**, eFuse = **IC1**, 3.3 V regulator = **U1**, Qwiic = **J11**, radar = **J5**, Hall = **J17/J18**, LED ring = **J14**, speaker = **J15**.
- **Ground**: every GND pad traced to the plane; GND vias right next to the 3.3 V output capacitors; 10 stitching vias; teardrops on vias and through-hole pads.
- **Stencil**: 2.2 mm openings over the H1/H2 holes, to align the stencil on two pins. Mounting with two M2 screws (H1/H2); no room for a third one at the top.
- **Module**: ESP32-S3-MINI-1-**N8** is mandatory (IO26 is used; on the N4R2 it is taken by the PSRAM).
- **Silkscreen**: three levels — functions (connectors, buttons, VBUS/5V/3V3 test points, title) at 1.0 mm, designators at 0.8 mm, everything else hidden and kept on F.Fab. DRC: 4 harmless warnings.

---

## 0 — Frozen decisions

| Topic | Decision | Why |
|---|---|---|
| Power | **USB-C 5 V only, no battery** | servos, LEDs, radar and amplifier need 5 V with 3–3.5 A peaks; a 1S cell behind a BQ24075 cannot supply that |
| Module | **ESP32-S3-MINI-1-N8** (8 MB flash, no PSRAM) | IO35/36/37 are used; on an -R8 variant they are taken by the PSRAM |
| Pinout | **identical to the firmware's `src/pins.h`** | direct compatibility with the code and with builds already made on a Wemos S3 |
| Accelerometer | **LSM6DSOXTR** on I²C, address 0x6A | schematic already done, adds a gyroscope, and porting the `Motion` class (ten lines, no logic) is trivial |
| Audio | MAX98357A on 5 V, mono by sample duplication | 3.2 W available, fully GPIO-controlled |
| NeoPixels | SN74AHCT125PWR between the ESP32 and the 3 data lines | SK6812 need a ≥ 3.5 V high level when powered at 5 V |
| Input protection | **TPS259573 eFuse**, no polyfuse, no switch | adjustable current limit, 6 V overvoltage cut-off, soft start, fault output — all in a WSON-8. Unplugging USB acts as the power switch |
| Board | **4 layers**, JLCPCB | a continuous ground plane is required for Wi-Fi, the class-D amplifier and the buck-boost; the extra cost is marginal |
| Debug | Native USB-C + backup UART0 on J16 (5-pin JST-SH: TXD0, RXD0, GND, EN, IO0) — no external JTAG | flashing and serial console over USB; UART0 for recovery flashing and logs. The TC2050 JTAG footprint was dropped on 22.09 (§3.4) |

**Removed compared with the original schematics:** BQ24075 and its surroundings (R7, R8, R12, R13, C1, C2, C9, LED1, TP1), BQ27441 (U3, R14–R16, C12, C13), battery connector CN1, C5/C7/C8/C10/C11, nets `SYS`, `VBAT`, `SYSOFF`, `SYSComp`, `SYSSense`, `BatPGOOD`. Header U9 and its labels `GPIO1/2/21/47`. Old JTAG header U8 with STM32 pinout.

**Kept:** USBC1, D1 USBLC6-2SC6, R1/R5 5.1 kΩ, U1 TPS631000 with L1 and its FB divider, LED2/LED3, R25/C29 on EN, SW2 BOOT. *(The two LEDs ended up as D3 red / D4 green with R12 / R18, §3.7.)*

**Also removed:** SW1 and its surroundings R39/R40/C46, net `3V3EN`, polyfuse F1, the AP22811 and net `5V_PERIPH`, connector J9 and its home-made remote board, the five 100 µF 6.3 V capacitors, the R2/R3/R9/R10 dividers and C6 (`SYSCOMP`, `SYSSense`).

---

## 0b — KiCad project structure

### 0b.1 Sheet hierarchy

| Page | File | Content |
|---|---|---|
| 1 | `Turret2.kicad_sch` (root) | USB-C input, protection, eFuse, 3V3 regulator |
| 2 | `mcu.kicad_sch` — sheet `MCU` | ESP32-S3 module, reset/boot, JTAG, UART0, LSM6DSOX, LEDs, amplifier |
| 3 | `io.kicad_sch` — sheet `IO` | connectors, SN74AHCT125, servo bulk capacitors |

**Pitfalls met when attaching a sheet** — all silent, all experienced on this project:

- The root's `(sheet …)` block must carry its own `(instances (project "Turret2" (path "/<root-uuid>" (page "N"))))` section. Without it, KiCad renumbers and rewrites on every opening.
- A sub-sheet **must not** have `(sheet_instances …)`: only the root schematic has one, with `(path "/" (page "1"))`.
- Every symbol in a sub-sheet carries an instance path `/<root-uuid>/<sheet-uuid>`. A stale old path creates a **ghost sheet** with its own references — re-annotating does not fix it, since KiCad annotates the real instance and leaves the other one untouched.
- `Turret2.kicad_pro` holds two lists to watch: `sheets` and above all `top_level_sheets`. A sheet once opened on its own gets written into the latter and becomes a parallel root. That is what produced a duplicated "MCU" in the hierarchy tree.

Check: `kicad-cli sch export netlist` must run without a single warning and list exactly the expected pages. An "*error found … automatically fixed*" message on opening always points to one of these inconsistencies.

### 0b.2 Net naming and label scope

| Net | Form | Scope |
|---|---|---|
| `VBUS`, `+5V`, `+3V3`, `GND` | power symbols | global by nature, cross the hierarchy without declaring anything |
| `PWR_FLT`, `USB_D+`, `USB_D-`, the six servos, the two Hall sensors, the three NeoPixels, the two radar lines, `SDA`, `SCL`, `ESP32TXD0`, `ESP32RXD0` | **global labels** | cross sheets |
| `JTAG_*`, `S3En`, `S3PRG`, `ESP32Debug`, `FaultLed`, `JTAG_SEL`, `IMU_INT1`, `BCLK`, `DIN`, `LRCLK`, `AMP_*` | local labels | local — both ends are on the same page |

**A local label never crosses a sheet**, even with the hierarchy in place: it appears in the netlist prefixed with the sheet path, e.g. `/MCU/PWR_FLT`, and forms a separate net. That is the symptom to recognise.

Choosing between global labels and hierarchical labels + sheet pins is a matter of style. For a handful of signals, a global label is simpler and needs nothing maintained on both sides.

**One name, one net.** `3V3` and `+3V3` are two different nets; so are `5V` and `+5V`. A power symbol imported from elsewhere with the old name leaves a pin powered by nothing, with no visible error.

---

## 1 — Complete ESP32-S3 pinout

Pins marked **pins.h** are imposed by the firmware and are not negotiable. The others were placed around them.

> Connector designators in this table are the old ones; see §0c and the README for the actual ones.

| GPIO | Label | Function | Origin |
|---|---|---|---|
| IO0 | `S3PRG` | BOOT — SW2 to GND, strap | ESP32 |
| IO1 | `GUN_LEFT` | left gun servo PWM → J1 | **pins.h** |
| IO2 | `GUN_RIGHT` | right gun servo PWM → J2 | **pins.h** |
| IO3 | `JTAG_SEL` | strap — 10 kΩ to 3V3 + switch to GND (SW1 on the board). Former USB / external JTAG selector; since 22.09 a configuration switch, or unpopulated (§0c, §3.4) | ESP32 |
| IO4 | `WING_LEFT` | left wing servo PWM → J3 | **pins.h** |
| IO5 | `WING_RIGHT` | right wing servo PWM → J4 | **pins.h** |
| IO6 | `ROTATE_X` | X rotation servo PWM → J5 | **pins.h** |
| IO7 | `ROTATE_Z` | Z rotation servo PWM → J6 | **pins.h** |
| IO8 | `HALL_LEFT` | ADC1, left Hall sensor ← J8 | **pins.h** |
| IO9 | `HALL_RIGHT` | ADC1, right Hall sensor ← J8 | **pins.h** |
| IO10 | `DIN` | I²S data → MAX98357A | **pins.h** |
| IO11 | `BCLK` | I²S bit clock → MAX98357A | **pins.h** |
| IO12 | `LRCLK` | I²S word clock → MAX98357A | **pins.h** |
| IO13 | `AMP_SD` | MAX98357A SD_MODE via 2 kΩ | board |
| IO14 | `NEOPIXEL_CENTER` | → AHCT125 gate 1 → J12 ring (9 LEDs) | **pins.h** |
| IO15 | `NEOPIXEL_LEFT` | → AHCT125 gate 2 → J1 (2 LEDs) | **pins.h** |
| IO16 | `NEOPIXEL_RIGHT` | → AHCT125 gate 3 → J2 (2 LEDs) | **pins.h** |
| IO17 | `RADAR_RX` | UART1 RX ← LD2450 TX (J10) | **pins.h** |
| IO18 | `RADAR_TX` | UART1 TX → LD2450 RX (J10) | **pins.h** |
| IO19 / IO20 *named `USB_D-` / `USB_D+` on the symbol, pins 23 and 24* | `USB_D-` / `USB_D+` | native USB via D1 | ESP32 |
| IO21 | `AMP_GAIN` | GAIN_SLOT direct — never drive high | board |
| IO26 | — | spare — R19 10 k to 3V3 + C11 100 nF, left over from button A removed on 20.09 (§12b.4) *(button A came back on 22.09, see §0c)* | |
| IO33 | `ESP32Debug` | D4 (green) via R18 100 Ω | board |
| IO34 | — | spare — R20 10 k to 3V3 + C12 100 nF, left over from button B removed on 20.09 (§12b.4) *(button B came back on 22.09, see §0c)* | |
| IO35 | `SDA` | I²C → LSM6DSOX | **pins.h** |
| IO36 | `SCL` | I²C → LSM6DSOX | **pins.h** |
| IO37 | `IMU_INT1` | LSM6DSOX INT1 (spare, not used by the firmware) | board |
| IO38 | `PWR_FLT` | eFuse fault output, open drain, 10 k pull-up to 3V3 — active low | board |
| IO39 | `JTAG_TCK` | MTCK → JTAG header pin 4 *(not connected since 22.09)* | board |
| IO40 | `JTAG_TDO` | MTDO → JTAG header pin 6 *(not connected since 22.09)* | board |
| IO41 | `JTAG_TDI` | MTDI → JTAG header pin 8 *(not connected since 22.09)* | board |
| IO42 | `JTAG_TMS` | MTMS → JTAG header pin 2 *(not connected since 22.09)* | board |
| IO43 / IO44 *named `TXD0` / `RXD0` on the symbol, pins 39 and 40* | `ESP32TXD0` / `ESP32RXD0` | no-connect — header J16 removed on 20.09 *(back on 22.09, see §3.5)* | ESP32 |
| IO45 / IO46 | — | straps, no-connect | ESP32 |
| IO47 | `AMP_GAIN_100K` | GAIN_SLOT via 100 kΩ — never drive high | board |
| IO48 | `FaultLed` | D3 (red) via R12 680 Ω | board |
| EN | `S3En` | R25 10 kΩ → 3V3, C29 1 µF → GND, reset button, UART0 connector J16 | ESP32 |

---

## 2 — Power sheet

**Done.** Netlist checked by kicad-cli export. Designators match the actual schematic.

### 2.1 USB-C input and protection

| Part | Wiring |
|---|---|
| USBC1 | 16-pin Type-C. CC1 → R1 5.1 kΩ → GND; CC2 → R5 5.1 kΩ → GND. D+ A6/B6 tied together, D− A7/B7 tied together. SBU1/SBU2 NC. VBUS → net `VBUS` |
| D1 USBLC6-2SC6 | D+/D− diagonally (1↔6, 3↔4), VBUS pin 5, GND pin 2. Right next to USBC1. Connector side: `USB_D+in` / `USB_D-in`; MCU side: `USB_D+` / `USB_D-` |
| D2 SMAJ5.0A | **in parallel**: one lead on `VBUS`, the other on `GND`. Never in series — the whole board current would flow through it. Clamps plug-in transients that the eFuse would not see under its 20 V absolute rating |
| C2 100 nF | also **in parallel** VBUS → GND, right at the eFuse input |

**No more polyfuse.** The 1812L300MR is removed: the eFuse is better in every respect — an instantaneous, resettable limit instead of a slow trip that degrades with every event, 34 mΩ instead of 40, plus the overvoltage cut-off that a PPTC cannot provide.

**Usage constraint.** The board is a passive sink (5.1 kΩ on CC): it does not read the current limit advertised by the source. Use a 5 V / 3 A wall charger; a PC port may cut out during a servo move.

### 2.2 eFuse IC1 TPS259573DSGT — the heart of the rail

On its own it replaces the polyfuse, the overvoltage protection, the inrush control and the master switch. WSON-8 2×2 mm, 2.7–18 V, 4 A, 34 mΩ.

| Pin | Connection | Value |
|---|---|---|
| 3, 4 (IN) | `VBUS` — **both tied**, they share the current | — |
| 5 (OUT) | net `+5V` | — |
| 2 (EN/OVLO) | R3 from `VBUS`, R6 to `GND` | **1 M / 249 k** |
| 7 (ILM) | R7 to `GND` | **523 Ω** |
| 1 (DVDT) | C1 to `GND` | **10 nF / 50 V** *(changed to 47 nF, see status-and-history §2)* |
| 6 (FLT) | R2 to **`+3V3`**, net `PWR_FLT` → IO38 | 10 k |
| 8, 9 (GND + pad) | `GND`, 4 vias under the pad | — |

**EN/OVLO is active low** in its OVLO role: the device conducts below 1.2 V and turns off above. The R3/R6 divider sets the threshold: 1.2 × (1000+249)/249 = **6.02 V**. Above it, the eFuse turns off — it does not merely clamp. Do not reuse the 1 M / 316 k from TI's application circuits: they cut at 5.0 V, so with a charger legitimately delivering 5.25 V the board would switch itself off.

**Current limit**: I = 2000/R + 0.04, i.e. 3.86 A with 523 Ω. Set it above the real peaks: an eFuse does not cut above the threshold, it goes into constant-current mode and the output collapses. With 3 servos the peaks are around 2 A, with 6 around 3.5 A.

**FLT pull-up to 3V3, never to 5 V.** FLT is an open-drain output: it can only pull to ground, and the pull-up defines its high level. TI's schematics pull it up to the 5 V output because nobody reads the signal there; here it goes to a GPIO that is not 5 V tolerant. The 10 k resistor divides nothing — no current flows through it at rest — it only limits the current when the eFuse pulls the line low.

**Start-up.** With ~500 µF downstream, the eFuse briefly starts in current limit, for about a millisecond. That is within its thermal limits. The dVdt capacitor softens the ramp; increasing the bulk capacitance lengthens this phase.

### 2.3 +5 V rail — star from the eFuse output

One single `+5V` net, but **two physical branches** that only meet at the eFuse OUT pad. It is a routing constraint, not a schematic one.

| Branch | Supplies | Decoupling |
|---|---|---|
| Servos | J1 to J6 (5 V pin) | bulk **2 × 220 µF OS-CON polymer** Panasonic 16SVPG220M (C9, C18) + 100 nF per connector |
| Logic | U1 TPS631000, U3 MAX98357A, U5 SN74AHCT125, J10 radar, J12 ring | amplifier: 100–220 µF + 10 µF + 100 nF. Regulator: see 2.4. AHCT125: 100 nF right at the pin |

**Current schematic designators** (the table keeps the old ones): servos J3, J4 (guns), J7, J8, J9, J10; logic U1, U9 MAX98357A, IC2 SN74AHCT125, J5 radar, J14 ring. Implementation on the board: §9.1b.

Servo inrush currents must never flow through the logic branch: this is what protects the audio from "plops" at every wing movement.

Voltage-drop budget at a 3 A peak: charger cable 0.2–0.3 V, eFuse 0.10 V (34 mΩ), remote cable ≈ 0.08 V round trip in 24 AWG over 15 cm, traces 0.05 V. Aim for **> 4.5 V at the servos** at peak.

**Net `5V_PERIPH` and the AP22811 are removed.** The eFuse already switches everything off, servos included, which makes the load switch redundant.

### 2.4 3V3 regulator — U1 TPS631000

A buck-boost is oversized for a fixed 5 V input, but useful: when the 5 V dips under servo load, the 3V3 does not move.

| Pin | Connection |
|---|---|
| VIN | `+5V`, C6 **22 µF 25 V X7R 1210** + 100 nF 0603 50 V right at the pin |
| EN | **`+5V` directly** — SW1, R39, R40 and C46 are removed along with the switch, net `3V3EN` no longer exists |
| MODE | GND (auto PFM), or 3V3 to force PWM if idle noise bothers the audio |
| LX1 / LX2 | L1 1 µH — short before wide, never on L3 |
| VOUT | `+3V3`, **2 × 22 µF 25 V X7R** (~42 µF effective, the datasheet asks for 47) *(three were fitted in the end, see status-and-history §2)*; R4 56 k / R11 10 k on FB, 0.5 V reference |

**The ceramic's voltage rating is the real lever.** A 22 µF 6.3 V part gives only 5 to 8 µF at 5 V; the same value rated 25 V gives nearly all of it. X5R and X7R lose capacitance under bias in the same way — the difference between them is the temperature range, not the DC-bias behaviour.

Part chosen for C6, C4 and C5: **Taiyo Yuden MCAST32MSB7226KPNA01**, 22 µF 25 V X7R 1210, soft termination. Five pieces are enough, spare included. 1210 is the size most prone to cracking when the board flexes, hence the soft termination.

**Removed from this block:** the five 100 µF 6.3 V capacitors (original C5, C7, C8, C10, C11 — X5R parts that only gave 15 to 25 µF each and added to the eFuse's capacitive load), and the R2/R3/R9/R10 dividers with C6 that fed `SYSCOMP` and `SYSSense`, nets that died with the charger.

### 2.5 Traces and vias — power

| Net | Peak current | L1 trace (1 oz) | L3 zone (0.5 oz) | Vias per transition |
|---|---|---|---|---|
| `VBUS` → eFuse IN | 3 A | 2.0 mm | 4 mm minimum | 4 × 0.5 mm |
| `+5V` servo branch | 3 A | 2.0 mm (trunk), 1.0 mm per connector | 4 mm | 4 × 0.5 mm at the trunk, 2 × 0.5 mm per connector |
| `+5V` logic branch | 1.5 A | 1.0 mm | 2 mm | 2 × 0.5 mm |
| `+3V3` | 0.5 A + Wi-Fi transients | 1.0 mm | 2 mm | 2 × 0.5 mm |
| LX1/LX2 → L1 | — | 0.8 mm, **as short as possible** | never route on L3 | no via |
| `EN_OVLO`, `ILM`, `DVDT`, `PWR_FLT`, CC1/CC2 | < 1 mA | 0.2 mm | — | 1 × 0.3 mm |
| USB D+ / D− | — | ≈ 0.2 mm, gap ≈ 0.15 mm, lengths matched to ±2 mm | — | avoid; otherwise 2 × 0.3 mm, symmetrical |

12 Mbit/s Full Speed does not need strictly controlled impedance: a tight, symmetrical pair over a solid L2.

**Inner copper.** JLCPCB's standard 4-layer stack-up has **0.5 oz on L2/L3**, hence the doubled widths in the L3 column. KiCad shows 0.035 mm everywhere, which is wrong for the inner layers.

## 3 — MCU sheet

### 3.1 Module U7 ESP32-S3-MINI-1-N8

- 3V3 on the power pins, 10 µF + 100 nF right at the pins, GND and thermal pad to L2 with at least 4 vias.
- Antenna: at the board edge, keepout on all 4 layers (§8).
- Labels per the table in §1. Every unused IO gets an explicit no-connect for the ERC.
- **Mind the symbol naming** of `RF_Module:ESP32-S3-MINI-1`: four GPIOs carry their function instead of their number — `USB_D-` (IO19, pin 23), `USB_D+` (IO20, pin 24), `TXD0` (IO43, pin 39), `RXD0` (IO44, pin 40). Searching for "IO43" in the pin list finds nothing. And **module pins 42 and 43 are grounds**, which adds to the confusion between pin numbers and GPIO numbers.

### 3.2 Reset and boot

| Signal | Wiring |
|---|---|
| EN (`S3En`) | R25 10 kΩ → 3V3, C29 1 µF → GND, reset button SW3 → GND, UART0 connector J16 |
| IO0 (`S3PRG`) | SW2 → GND, internal pull-up is enough |
| IO3 | strap, read at reset — SW1 + 10 kΩ pull-up. Former JTAG_SEL (§3.4); if SW1 is used as a configuration switch, never burn `STRAP_JTAG_SEL` |
| IO45, IO46 | no-connect, nothing else — boot straps |

### 3.3 Native USB

IO19 → D1 → USBC1 D−, IO20 → D1 → USBC1 D+. Route as a 90 Ω differential pair if possible, otherwise short and side by side. This is the main flashing path (`ARDUINO_USB_CDC_ON_BOOT=1` in the reference `platformio.ini`) and the serial console.

### 3.4 JTAG — Tag-Connect TC2050 footprint — *removed from the board on 22.09*

*22.09: the TC2050 did not fit during routing; J2 is excluded from the board and replaced by UART0 (§3.5). Text kept for the record.*

**Chosen: TC2050 footprint**, no component, for the TC2050-IDC cable already in stock. It plugs into the probe's 2×5 1.27 mm header (ESP-Prog, J-Link) and the wiring is straight pin-to-pin — the table below is the board pad table.

- Use the **official Tag-Connect KiCad footprint**, not a redrawn one: the tolerances are tight.
- Pads without solder mask, **no via inside**, flat finish (ENIG) for the pogo pins.
- Component-side keepout over the whole area. **If the cable is the legged version**: 3 plated locating holes and a keepout **under the board** too, opposite those holes.
- **Check the cable version**: the NL version (no legs) needs the TC2050-CLIP, otherwise the cable has to be held by hand for the whole session.

**Alternative:** Amphenol 20021121-00010T4LF header (2×5, 1.27 mm), two in stock. More comfortable during development, but takes room and height. One *or* the other, not both.

| Pin | Signal | Connection |
|---|---|---|
| 1 | VTref | 3V3 |
| 2 | TMS | `JTAG_TMS` IO42 |
| 3 | GND | GND |
| 4 | TCK | `JTAG_TCK` IO39 |
| 5 | GND | GND |
| 6 | TDO | `JTAG_TDO` IO40 |
| 7 | KEY | no-connect |
| 8 | TDI | `JTAG_TDI` IO41 |
| 9 | GNDDetect | GND |
| 10 | nRESET | `S3En` |

No external pull-ups on TMS/TCK/TDI: the ESP32-S3 has them internally. Share nothing with IO39–IO42.

#### USB / external JTAG selection by switch — SW4

By default, the ESP32-S3's JTAG is routed to the built-in USB_SERIAL_JTAG peripheral, not to the pins. **Decision: select with a switch rather than a permanent eFuse burn**, to keep both modes available.

**Slide switch, not a push button.** IO3 is a strapping pin, read *only once, at reset*. A push button would have to be held down while pressing RESET; a slide switch keeps its state — flip it and restart.

| Part | Wiring |
|---|---|
| R_SEL | 10 kΩ from IO3 to `+3V3` |
| SW4 | SPST between IO3 and `GND` |

| Position | IO3 at reset | Active JTAG |
|---|---|---|
| open | high | **built-in USB** — everyday use |
| closed | low | **IO39–42**, external probe on the TC2050 |

The open-state default is the everyday mode: if the switch breaks or its position is forgotten, the board remains usable.

**One eFuse burn is still required**, once, over USB:

`espefuse.py --port COMx burn_efuse STRAP_JTAG_SEL`

It *enables* the selection mechanism. Without it, IO3 has no effect and JTAG stays on USB whatever you do — the most reported pitfall on the Espressif forum.

**Never burn `DIS_USB_JTAG` or `DIS_PAD_JTAG`.** Both must stay at 0, otherwise strap selection is ignored. Check with `espefuse.py summary` before and after.

**What is not lost:** USB CDC works in both positions. Flashing and the serial console always go through USB; only the JTAG routing changes.

Consequences: IO3 becomes dedicated and cannot be used for anything else, and SW4 must be reachable in the enclosure — at least with a screwdriver.

> **Since 22.09** there is no external JTAG anymore; the switch (SW1 on the board) no longer serves this purpose. See §0c: either use it as a configuration switch read by the firmware on IO3 — and then **never** burn `STRAP_JTAG_SEL` — or leave it unpopulated.

### 3.5 Backup UART0 — removed on 20.09, *back on 22.09*

**22.09: J16 is back**, as a right-angle 5-pin JST-SH on the back of the board: TXD0 (IO43), RXD0 (IO44), GND, EN (`S3En`), IO0 (`S3PRG`), MP to GND. No 3V3 on the connector. It replaces JTAG for backup flashing and logs.

**J16 removed from the schematic on 20.09**: TXD0 and RXD0 become no-connect, native USB and JTAG (TC2050) cover the need. Original text kept for the record: 5-pin 2.54 mm header: `ESP32TXD0` (IO43), `ESP32RXD0` (IO44), GND, `S3En`, `S3PRG`. TP5/TP6 removed on 19.09, replaced by this header. Useful if native USB is broken by a firmware.

### 3.6 I²C

**The pull-ups sit on the sensor sheet, right next to the LSM6DSOX** (see §5.1). They do not belong to the MCU: an I²C pull-up belongs to the *bus*, not to a component. One pair, and only one, whatever the number of slaves.

Bus at 100 or 400 kHz, single slave.

### 3.6b Traces and vias — MCU

| Signal | Width | Note |
|---|---|---|
| 3V3 to the module | 1.0 mm | 10 µF + 100 nF right at the pin, 0.5 mm via to L3 |
| Module GND + pad | — | 6 to 9 × 0.3 mm vias to L2, spread under the pad |
| I²S IO10/11/12 | 0.25 mm | grouped, < 50 mm, over solid L2, no via if possible |
| I²C IO35/36 | 0.25 mm | paired, away from the servos and the switching node |
| JTAG IO39–42 | 0.25 mm | 4 short traces, similar lengths |
| UART0, EN, IO0, LEDs | 0.25 mm | — |
| Servo PWM IO1/2/4–7 | 0.3 mm | signal only, no power |

### 3.7 Debug LEDs

Green LED (D4): IO33 → **R18 100 Ω** → LED → GND. Red LED (D3, KT-0603R): IO48 → **R12 680 Ω** → LED → GND. Not used by the reference firmware; the Turret2 firmware uses them as status LEDs (green heartbeat, red fault codes).

*Updated 19.10.2026:* R18 lowered from 1 kΩ to **100 Ω** (user's decision, made in the master KiCad project; the portable copy in `Turret2_portable/` follows when it is refreshed). With 3.3 V and a green LED forward voltage of 2.0 to 3.0 V, that is 3 to 13 mA, well within the GPIO's default drive (20 mA). Earlier versions of this plan said 470 Ω for both LEDs, then 1 kΩ (R26, R29, LED2 / LED3): those were the names and values before the final schematic.

---

## 4 — Audio sheet: U3 MAX98357A

Symbol to create or import (JLCPCB C910544, TQFN-16 3×3 mm). Values from the MAX98357A/B datasheet, rev. 16.

| Pin | Connection |
|---|---|
| VDD | logic `5V` — never 3V3. At 5 V: 3.2 W into 4 Ω, 1.8 W into 8 Ω. 3.3 V I²S logic accepted (as on the Adafruit / SparkFun boards) |
| GND + exposed pad | GND; the pad is thermal, not connected internally → solid ground plane + 4 vias |
| DIN / BCLK / LRCLK | IO10 / IO11 / IO12 direct |
| SD_MODE | `AMP_SD` IO13 via a 2 kΩ resistor. **No pull resistor**: the internal 100 kΩ pull-down keeps the amp muted at reset, in the bootloader and during a reflash. GPIO high = 3.3 V > 1.4 V threshold = left-channel mode; mono is done by duplicating the sample into both I²S slots |
| GAIN_SLOT | `AMP_GAIN` IO21 direct + `AMP_GAIN_100K` IO47 via 100 kΩ ±5 %. Nothing else, no capacitor |
| OUT+ / OUT− | 2-pin terminal block J11. BTL output: never tie OUT− to ground, never drive headphones. 4–8 Ω speaker, never a purely resistive load or a piezo |

### 4.1 Firmware-controlled gain

| IO21 | IO47 | What the chip sees | Gain |
|---|---|---|---|
| input | input | floating | 9 dB (default at reset) |
| low | input | GND direct | 12 dB |
| input | low | 100 kΩ to GND | 15 dB |

The +6 and +3 dB states (pull-up to VDD) are not reachable: detection compares against VDD = 5 V, and a GPIO only goes up to 3.3 V. **Never drive IO21 or IO47 high.** The gain is sampled when leaving shutdown: to change gain, `AMP_SD` low → set → 10 ms → `AMP_SD` high. Everyday volume is done in software on the samples.

### 4.1b Traces and vias — audio

| Net | Width | Note |
|---|---|---|
| VDD from the bulk | **1.5 mm** | peaks > 1 A into 4 Ω; doubled 0.5 mm via to L3 |
| OUT+ / OUT− to J11 | **1.5 to 2.0 mm**, 0.3 mm gap | on L1 only, over solid L2. At 1.5 mm over 30 mm the trace is ~10 mΩ: negligible compared with the 100 mΩ that would cost 5 % of the power |
| QFN thermal pad | — | at least 4 × 0.3 mm vias, 9 if room allows, plugged or tented |
| DIN / BCLK / LRCLK | 0.25 mm | — |
| SD_MODE, GAIN_SLOT | 0.2 mm | short, with the 2 kΩ and the 100 kΩ right at the chip |
| GND around OUT+/OUT− | — | 0.3 mm stitching vias every 5 mm on each side |

### 4.2 Decoupling and protection

- Order from the VDD pin: 100 nF 0603 right at the pin, then 10 µF X7R 25 V 0805, then the bulk slightly further towards the power input. The ceramic absorbs the switching transient (~300 kHz), the bulk is the reservoir for audio peaks (> 1 A into 4 Ω). Keep the VDD → capacitor → GND loop short and compact, otherwise the bulk is useless.
- **Bulk: 100–220 µF / 10 V electrolytic**, on the logic branch of the 5 V, not on the servo branch. **No tantalum here**: the 5 V rail is shared with the servos up to the eFuse output and sees transients (hot-plugging, motor deceleration); a failing tantalum goes into a hard short. If height requires a flat package, use an **aluminium polymer** (SP-Cap, OS-CON) 6.3–10 V: the ESR of a tantalum with the failure mode of an electrolytic. Tantalum only as a last resort, and then **25 V** minimum, never 16 V.
- No ferrite on the outputs (spread spectrum, EMI fine up to 30 cm of cable); optional unpopulated 0603 footprints. No series DC-blocking capacitor.
- Speaker polyfuse optional: the chip protects itself (2.8 A thermal); a polyfuse would only protect the speaker coil.

---

## 5 — Sensor sheet

### 5.1 U15 LSM6DSOXTR (LGA-14) — chosen

**Revised decision: the LSM6DSOX is kept, the ADXL345 dropped.** The firmware porting effort had been overestimated. The `Motion` class of *portal-turret-v4* is ten lines long: it instantiates the object, calls `begin()`, and in `Update()` reads the event without doing anything with it — all the print lines are commented out. It is a stub, not a feature.

Porting boils down to changing the object type in `Motion.h` and the `lib_deps` line in `platformio.ini`. The Adafruit LSM6DSOX library exposes the same `sensors_event_t` interface through `Adafruit_Sensor`, so `getEvent()` works the same way.

And it is the better part: it adds a **three-axis gyroscope** that the ADXL345 lacks. For a turret that must detect being knocked over or picked up — the behaviour of the in-game turret — that is exactly what is needed. The ADXL345 is a 2009 part, chosen for the reference project because it was available as a ready-made module.

| Pin | Connection |
|---|---|
| VDD, VDDIO | 3V3, 100 nF + 1 µF right at the pins |
| CS | 3V3 (I²C mode) |
| SA0 | GND → address **0x6A** |
| SDA, SCL | `SDA` IO35, `SCL` IO36 |
| INT1 | `IMU_INT1` IO37 (spare, not used by the current firmware) |
| INT2, SDO_AUX, OCS_AUX | no-connect |
| GND | GND |

The existing schematic is correct: it only needs to be copied into the new sheet. Placement: away from the servos and the speaker, not under the antenna. On the firmware side, loud audio can falsely trigger motion detection.

#### I²C terminations — here and nowhere else

**The two 2.2 kΩ pull-ups to `+3V3` sit right next to the LSM6DSOX**, the only slave on the bus. A pull-up belongs to the bus, not to a component: one pair, and only one, whatever the number of slaves. With a bus a few centimetres long and a single slave, putting them at the sensor or at the module is electrically the same — but a future second slave will plug in near the first one, so the resistors will already be in the right place.

- To **`+3V3`**, never to 5 V — the classic mistake when reusing a schematic.
- **2.2 kΩ** (already in stock): the best of the three options. The current to sink when a device pulls the line low is 3.3 V / 2.2 kΩ = **1.5 mA**, i.e. 40 % of the 4 mA the LSM6DSOX can sink, and well below the 3 mA of the I²C specification. Comfortable margin in 400 kHz Fast Mode, and still fine if the bus grows longer or gains a slave. The only cost: 1.5 mA per line during low bits, invisible on USB power. 4.7 kΩ would also work; 10 kΩ would be the bare minimum.
- **Only one set on the whole board.** When copying the block from the old file, make sure it does not bring a second pair: two sets in parallel give 1.1 kΩ, which overloads the open-drain outputs.

### 5.1b Traces and vias — sensors

LSM6DSOX 3V3: 0.4 mm, 0.3 mm via to L3 within 2 mm of the 100 nF. SDA/SCL: 0.25 mm. INT1: 0.25 mm. GND: 2 × 0.3 mm vias under the package. Hall outputs IO8/IO9: 0.3 mm, away from the servo traces and the speaker cable — these are analog measurements.

### 5.2 J8 Hall sensors

*Superseded on 24.09: two 3-pin connectors J17/J18 instead, same 3V3 supply, see §0c.*

4-pin 2.54 mm: 1 = **3V3**, 2 = `HALL_LEFT` IO8, 3 = `HALL_RIGHT` IO9, 4 = GND. **3V3, not 5 V**: the sensor output follows its supply voltage, and IO8/IO9 are not 5 V tolerant — this is the trace the reference board had to cut after the fact. 100 nF on pin 1.

---

## 6 — IO sheet: connectors and level shifter

Pinouts copied **exactly** from the reference board, to stay compatible with the community's cables and gun daughter-board.

> Old designators; see the README for the actual ones on the board.

| Ref. | Function | Pins | Rail |
|---|---|---|---|
| J3 | Wing left | 1 GND · 2 `5V` · 3 IO4 | servos |
| J4 | Wing right | 1 GND · 2 `5V` · 3 IO5 | servos |
| J5 | Rotate X | 1 GND · 2 `5V` · 3 IO6 | servos |
| J6 | Rotate Z | 1 GND · 2 `5V` · 3 IO7 | servos |
| J1 | Gun left | 1 `5V` · 2 IO1 servo · 3 AHCT 2Y (LED) · 4 GND | servos (pin 1) — the LED takes its 5 V from the same pin |
| J2 | Gun right | 1 `5V` · 2 IO2 servo · 3 AHCT 3Y (LED) · 4 GND | servos |
| J12 | Ring LED (9 × SK6812) | 1 `+5V` · 2 AHCT 1Y · 3 GND | logic |
| J10 | HLK-LD2450 | 1 `+5V` · 2 radar TX → IO17 · 3 radar RX ← IO18 · 4 GND | logic |
| J8 | Hall | see §5.2 | 3V3 |
| J11 | Speaker | 2-pin terminal block OUT+ / OUT− | — |
| — | Remote USB | no connector any more: Adafruit ADA6050 breakout wired directly, see §10 *(finally a 4-pin JST-PH, J1 "USB extender", see status-and-history §2)* | — |
| ~~J7~~ | Backup UART0 — removed on 20.09 | see §3.5 | — |
| ~~TC2050~~ | JTAG — removed on 22.09 | see §3.4 | — |

Servos: GND / 5 V / signal order = brown / red / orange on the MG90S cable. The 3.3 V PWM signal comes straight from the ESP32: **there is no motor driver**, the servo contains its own. Radar: the LD2450's TX line is 3.3 V, no level shifting needed towards IO17.

### 6.0b Traces and vias — connectors

| Net | Width | Vias |
|---|---|---|
| 5 V to each servo connector (J1–J6) | 1.0 mm | 2 × 0.5 mm |
| GND of each servo connector | 1.0 mm or straight to L2 | 2 × 0.5 mm |
| Servo PWM signal | 0.3 mm | 1 × 0.3 mm |
| `+5V` to J12 (9 LEDs ≈ 0.55 A full white) | 0.8 mm | 2 × 0.5 mm |
| NeoPixel data (AHCT outputs) | 0.3 mm | 1 × 0.3 mm; keep < 100 mm to the connector |
| `+5V` to J10 radar (0.15 A) | 0.5 mm | 1 × 0.5 mm |
| Radar UART IO17/IO18 | 0.3 mm | — |
| 3V3 to J8 Hall | 0.4 mm | 1 × 0.3 mm |
| 3V3 to J13 Qwiic | 0.5 mm | 1 × 0.3 mm |
| SDA / SCL to J13 | 0.25 mm | — |
| SN74AHCT125 VCC | 0.5 mm | 1 × 0.5 mm, 100 nF right at the pin |

### 6.1 J13 — Qwiic / STEMMA QT port

The best use of the remaining space: I²C is the only bus that accepts new peripherals **without using any GPIO**. `SDA` and `SCL` are already routed, so this connector costs none.

Connector chosen: **Adafruit 4208** (4-pin right-angle JST-SH, already in stock), with the Qwiic cable kit. Standardised pinout, so dozens of Adafruit and SparkFun modules can be chained without soldering.

| Pin | Net | Cable colour |
|---|---|---|
| 1 | `GND` | black |
| 2 | `+3V3` | red |
| 3 | `SDA` IO35 | blue |
| 4 | `SCL` IO36 | yellow |

**Current budget.** The TPS631000 supplies 1.5 A; the ESP32-S3 draws up to 0.5 A at Wi-Fi peaks, the rest of the 3V3 less than 20 mA. That leaves **about 1 A**. A 1.2″ OLED draws 20 to 40 mA, a backlit 2.8″ TFT about 150 mA: no constraint. The real limit is the 28 AWG JST-SH cable, which should not carry more than a few hundred milliamps — irrelevant for displays and sensors.

**No pull-ups on this connector.** They stay next to the LSM6DSOX (§5.1). Many Qwiic modules carry their own: one 10 kΩ in parallel with the 2.2 kΩ gives 1.8 kΩ, still acceptable, but two or three modules would pull it too low. Worth noting in the board documentation.

**Address 0x6A is taken** by the LSM6DSOX. Uncommon, so a collision is unlikely, but worth printing near the connector.

### 6.2 U5 SN74AHCT125PWR (TSSOP-14)

| Pin | Connection |
|---|---|
| 14 VCC | `+5V` logic branch, 100 nF right at the pin |
| 1, 4, 10, 13 (OE) | GND |
| 2 (1A) / 3 (1Y) | IO14 / J12 pin 2 |
| 5 (2A) / 6 (2Y) | IO15 / J1 pin 3 |
| 9 (3A) / 8 (3Y) | IO16 / J2 pin 3 |
| 12 (4A) / 11 (4Y) | GND / no-connect |
| 7 GND | GND |

Purpose: SK6812 LEDs need a high level ≥ 0.7 × VDD = 3.5 V; the ESP32 outputs 3.3 V, hence randomly flickering LEDs without this chip. It has nothing to do with the servos. Do not replace it with a BSS138 or TXS0108 bidirectional shifter (too slow for 800 kHz).

---

## 7 — Component choices

| Use | Choice |
|---|---|
| 10 µF decoupling | X7R 50 V 0805 — a 6.3 V X5R loses 60 to 80 % of its value at 4 V. The lever is the voltage rating, not the package |
| 100 nF next to the chips | X7R 50 V **0603** (distance to the pin matters more than the size) |
| Servo bulk | **2 × 220 µF OS-CON polymer** Panasonic 16SVPG220M (C9, C18), SMD Ø 6.3 × 5.9 mm, ESR 14 mΩ, 4.1 A ripple |
| Amplifier bulk | 100–220 µF / 10 V electrolytic. Flat alternative: 100 µF / 10 V aluminium polymer. Avoid tantalum (rail shared with the servos); if unavoidable, 47–100 µF / **25 V** |
| Dielectric | X7R everywhere, never Y5V |
| Resistors | **0603** 1 % — no 0402 on the board |
| Servo/LED/Hall/radar connectors | straight 2.54 mm, JST-XH or keyed headers, like the reference *(finally JST-PH 2 mm, see status-and-history §2)* |
| JLCPCB | check basic-part availability: TPS631000, TPS259573, SN74AHCT125PWR and MAX98357A are extended parts (assembly surcharge) |

### 7.1 Parts already in stock

The passives in stock are 0402: **not reused**, the board is entirely 0603 (§13.2 and §13.3). They remain available as spares.

| Need | Part | Note |
|---|---|---|
| MCU module | ESP32-S3-MINI-1-N8 | exactly the required variant (no PSRAM) |
| USB-C receptacle + ESD | TYPE-C-31-M-12 + USBLC6-2SC6 | *(receptacle finally GCT USB4120-03-C)* |
| 3V3 regulator | TPS631000DRLR + L1 1 µH FTC303020D | with its 56 k / 10 k FB divider |
| Accelerometer | LSM6DSOXTR | schematic already done |
| Reset + boot buttons | TS-1088-AR02016 ×2 | exactly the count needed |
| Status LEDs (D3 / D4) | KT-0603R ×2 | red; no green in stock, doesn't matter *(finally D3 red KT-0603R, D4 green)* |
| JTAG header | 20021121-00010T4LF ×2 | spare in case the TC2050 footprint does not work out |
| 10 µF 50 V | GRM21BR61H106KE43L 0805 ×3 | 50 V: almost no loss at 5 V |
| Amplifier bulk | EEE-FT1V101AP 100 µF SMD ×2 | low-ESR electrolytic — check the voltage code on the can |

### 7.2 To order

| Part | Package | Why nothing in stock replaces it |
|---|---|---|
| **MAX98357A** | TQFN-16 3×3 | neither board has an I²S amplifier |
| **SN74AHCT125PWR** | TSSOP-14 | required for the SK6812: they need a high level ≥ 3.5 V at 5 V, the ESP32 only outputs 3.3 |
| **TPS259573DSGT** | WSON-8 2×2 mm | eFuse: replaces polyfuse, OVP and load switch |
| **MCAST32MSB7226KPNA01 ×5** | 1210 | 22 µF 25 V X7R soft termination — C6 input, C4 and C5 output, 2 spares. ~0.40 CHF each |
| **SMAJ5.0A** | SMA (DO-214AC) | TVS in parallel on `VBUS` — clamps plug-in transients |
| **Adafruit ADA6050** | module | remote USB-C breakout, 5.1 kΩ on CC already fitted — replaces the home-made remote board and the second USBLC6 |
| **Panasonic 16SVPG220M** × 2 | SMD Ø6.3 × 5.9 mm | servo bulk 220 µF / 16 V OS-CON polymer, ESR 14 mΩ, 4.1 A ripple, 5000 h at 105 °C. Replaces the EEE-FK1C471P (ESR 160 mΩ, 0.6 A). **No tantalum** (§4.2) |
| **2.54 mm male headers** | 3-pin ×4, 4-pin ×2 | J3–J6 and J7/J9: imposed by the servo cable housings, no other connector fits *(finally JST-PH, crimped onto the servo cables)* |
| **JST-XH 2.54 mm** (or headers) | 3-pin ×1, 4-pin ×2 | J10 LED ring, J11 radar, J12 Hall — home-made cables, keying and latch are useful |
| **JST-PH 2 mm 2-pin** or 3.5 mm terminal block | ×1 | J14 speaker |
| **Speaker terminal block J11** | 2-pin 3.5 mm | — |
| **100 nF 0603 50 V** | 0603 | decoupling right at the TPS631000 VIN |

---

## 8 — 4-layer stack-up and placement

| Layer | Content |
|---|---|
| L1 | Components + signals: ESP32, MAX98357A and its bulk, I²S, OUT+/OUT− |
| L2 | **Solid GND, no traces** — reference for Wi-Fi, class D and buck-boost. Only opening: antenna keepout |
| L3 | Power zones: `VBUS`, `+5V` servos, `+5V` logic, `+3V3`; the rest is GND |
| L4 | Secondary signals + GND fill |

One ground, one GND net, never a split plane. Separation between noisy and sensitive circuits is done by placement, not by cut-outs.

### 8.1 Placement

- Module antenna overhanging the PCB outline, or a U-shaped cut-out open to the outside. Keepout on all 4 layers: no copper, no via, no component, no cable above, no screw or metal insert nearby. Aim for 15 mm lateral clearance.
- Amplifier + speaker J11 at the end opposite the antenna; the speaker cable leaves on that side.
- **eFuse right next to USBC1**, with D2 and C2 tight against its input. Its exposed pad is thermal: 4 vias to L2. Unlike a polyfuse, its current limit does not drift with ambient temperature.
- TPS631000's L1 opposite the antenna; its input/output capacitors right at the pins, smallest possible loop.
- Servo bulk and servo connectors grouped, opposite the antenna, on the servo branch of the 5 V. **Decided on 20.09:** the six servo connectors (J3, J4, J7–J10) bottom right, the other connectors top right.
- LSM6DSOX away from the servos and the speaker, not under the antenna.
- USBC1, D1 and the tap for the remote cable close together; D+/D− stubs in parallel < 10 mm.
- ~~TC2050 footprint at the board edge~~ *(JTAG removed on 22.09; UART0 J16 on the back instead)*. SW2/SW3 buttons reachable once the board is in the enclosure — native USB has no automatic reset.
- Every decoupling capacitor tight against its pin, via to L3 within < 2 mm, via to L2 on the other side.

---

## 9 — Routing

### 9.0 DRC rules and summary

Rules to enter in KiCad before routing (JLCPCB 4 layers, standard process):

| Parameter | Chosen value | Process limit |
|---|---|---|
| Minimum track width | 0.2 mm | 0.127 mm |
| Minimum clearance | 0.2 mm | 0.127 mm |
| Signal via | 0.3 mm drill / 0.6 mm pad | 0.3 / 0.45 mm |
| Power via | 0.5 mm drill / 0.9 mm pad | — |
| Track ↔ board edge clearance | 0.3 mm | 0.2 mm |
| Copper | L1/L4: 1 oz · L2/L3: **0.5 oz** | stack-up JLC04161H |

Current capacity used, for a 10 °C rise: 1 oz outer trace ≈ 1.5 A per millimetre of width; 0.3 mm via ≈ 1 A; 0.5 mm via ≈ 2 A. These values are deliberately conservative — on this board the real issue is not heating but the **voltage drop** when the servos start.

Summary by width:

| L1 width | Where |
|---|---|
| **2.0 mm** | `VBUS`, `+5V` servo trunk |
| **1.5 mm** | amplifier VDD, OUT+ / OUT− |
| **1.0 mm** | logic `5V`, `3V3`, 5 V and GND of each servo connector |
| **0.8 mm** | TPS631000 LX node, 5 V of J12 |
| **0.5 mm** | radar 5 V, AHCT125 VCC |
| **0.3 mm** | servo PWM, NeoPixel data, Hall outputs, radar UART |
| **0.2 to 0.25 mm** | everything else: I²S, I²C, JTAG, UART0, EN, boot, LEDs, SD_MODE, GAIN |

*Widths were reduced on 18.09 to reach small parts, and a 0.4 mm width was added on 23.09 — see status-and-history §2 and §11.*

### 9.1 L3 zones

| Zone | From | To | Current |
|---|---|---|---|
| `VBUS` | USBC1 + remote cable tap | D2, C2, eFuse IN, R3 | 3 A peak — short, it only crosses the input |
| `+5V` servos | eFuse OUT | C9 + C18 (2 × 220 µF OS-CON) then J3, J4, J7–J10 | 3 A peak — the largest zone. The bulk sits **between** the eFuse output and the connectors, not as a side branch; the through-hole pins act as vias |
| `+5V` logic | eFuse OUT (separate branch) | TPS631000, MAX98357A, AHCT125, J10, J12 | 1.5 A peak |
| `+3V3` | TPS631000 VOUT | ESP32 module, LSM6DSOX, Hall sensors, I²C pull-ups, FLT pull-up, Qwiic | 0.5 A peak |

Order in KiCad: GND zones on L2/L4 and GND fill on L3 at priority 0; L3 power zones at priority 1 (`VBUS`) and 2 (`+5V`) — applied on 20.09. No floating L3 island. No signal routed on L3 between two zones. Clearance between islands: 0.5 mm between different nets; **1 mm between the two +5V zones** (same net) so that a 0.5 mm ground corridor fits in between — applied on 20.09.

#### Three 5 V zones, but not of the same kind

`VBUS` is a **different net**: KiCad keeps it separate on its own, nothing to watch.

The two `+5V` zones carry the **same net**. KiCad accepts that, but **will never warn if they touch** — same net, so no DRC error and no short. They would merge silently and the separation would be lost without any warning. After every fill, check *visually* that a ground corridor remains between them. A 3D render of L3 or a track-length measurement gives an objective check.

#### The eFuse OUT pad is the star point

It is a WSON-8 pin, about 0.25 mm wide, and 4.5 A in total flows through it. Not a new constraint — all the current goes through it anyway — but the copper right around it must be generous: a pour on L1 from the pad, as wide as the package allows, then down to L3 through **at least 4 × 0.5 mm vias, two towards each zone**. That is where the voltage drop is decided, not in the zones themselves.

#### Where to put the bulk capacitors

A capacitor only supplies current to what is *electrically* close to it — little resistance and, above all, little inductance between them. When a servo starts it demands 0.7 A within a few microseconds: the path inductance decides whether the capacitor can respond in time.

So when routing: **mentally trace the current path from the eFuse OUT pad to each servo connector, and put the capacitor pads on that path** — not as a side branch. Same for the amplifier's 100–220 µF, which go between the eFuse output and the MAX98357A VDD pin.

The trap: with a large copper zone it seems position no longer matters, "everything is connected to everything". True for resistance, false for inductance — which depends on the area of the loop the current flows through. The further the capacitor is from the load, the larger that loop.

**Principle: large zone + vertical drops, no power traces.** The 5 V is not routed as a trace to the servo connectors. The L3 zone carries the current, and each pin drops straight into it. Lower resistance and inductance than a trace, and nothing to draw.

- **Through-hole connectors (J1–J6, J8, J10, J12): no via to add.** The plated hole of the pin already connects all four layers; it acts as a via.
- **Pad-to-zone connection — mixed assembly:** SMD on a hot plate, through-hole with an iron (Metcal).

  | Pad | Connection | Reason |
  |---|---|---|
  | Power SMD pads (eFuse, amplifier, TPS631000, bulk C9/C18) and thermal pads | **solid** | heated from below, the zone's thermal mass is not a problem |
  | Through-hole connector pins (J1–J6, J8, J10, J12) | **thermal relief**, 4 spokes of **0.6 mm** | soldered with an iron, and above all *desolderable* for a revision. ~15 mΩ per pin on 0.5 oz L3, i.e. 10 mV at 0.7 A: negligible |

  Set with a zone rule or in the footprint properties. Widen the spokes to 0.6 mm instead of KiCad's default 0.5 mm.
- **SMD parts (bulk C9/C18, TPS631000, amplifier, eFuse): real vias**, 4 × 0.5 mm per power terminal, right at the pads.
- **Extend the servo zone onto L4** if room allows, connected with a few 0.5 mm vias: two layers in parallel halve the resistance, and L4 is only fill anyway.
- The shape of the zone matters little as long as the cross-section is enough — **what matters is that it only joins the logic zone at the eFuse OUT pad**. Two distinct islands, a single junction point.

### 9.1b Implementation on the board — 20.09.2026

State of `Turret2.kicad_pcb` after the 20.09 session.

| Zone | Layer | Net / priority | Outline (mm) | Copper |
|---|---|---|---|---|
| `5V_etoile` | F.Cu | +5V / 2 | L-shaped node at the IC1 output: X 90.2 → 97.8, Y 117.4 → 120.7; lower part limited to X ≥ 92.6 (clear of the USB-C footprint) | 20 mm² |
| `5V_logique` | PWR (L3) | +5V / 2 | U1 pocket + node (X 89 → 100, Y 107.5 → 118.55) and top-right quarter (X 106.5 → edge, Y edge → 110) | 949 mm² |
| `5V_servos` | PWR (L3) | +5V / 2 | starts under the node (X 92.6 → 101, Y 119.55 → edge) then the whole bottom right (X 101 → edge, Y 111 → edge) | 674 mm² |
| `3V3` | PWR (L3) | +3V3 / 1 | left quarter under the module: X edge → 106, Y edge → 107, extended down to Y 111.5 left of X 88.5; 0.5 mm from VBUS and 5V_logique; cut by the antenna keepout | 638 mm² |
| `VBUS` | PWR (L3) | VBUS / 1 | unchanged, X edge → 88, Y 112 → edge | 296 mm² |
| `C9_plus`, `C18_plus` | F.Cu | +5V / 3 | solid pour between the + terminal and its 4 vias | 13 mm² each |
| `C9_gnd`, `C18_gnd` | F.Cu | GND / 3 | solid pour between the GND terminal and its 4 vias | 12 mm² each |
| `GND` | In1 (L2) | GND / 0 | solid plane | 2,680 mm² |
| `GND_L3` | PWR (L3) | GND / 0 | whole board; fills whatever VBUS, +3V3 and the +5V zones leave free, 1 mm corridors included; islands removed | 76 mm² |

    IC1.5 ──(0.25 mm, pad width)── F.Cu node 5V_etoile
                                     ├─ 3 vias 0.9/0.5 (Y 117.9) ──► L3 5V_logique ──► U1 (2 vias), amplifier, AHCT125, radar, ring
                                     └─ 4 vias 0.9/0.5 (Y 120.05) ─► L3 5V_servos ──► C9, C18 (4 vias each) ──► J3, J4, J7–J10

- **Star.** The two +5V planes are **1.0 mm** apart on L3 (measured, copper to copper) and only meet through the F.Cu node, less than 1 mm from the OUT pin. Scripted check: of the 17 +5V vias, 5 touch the logic plane, 12 the servo plane, none touches both. Difference from §9.1: 3 + 4 vias instead of 2 + 2, on the safe side.
- **Output neck.** The 0.25 mm trace leaving IC1.5 is covered by the existing "Fine pitch: width" rule (0.2 mm minimum inside IC1's footprint area). The "5V output fine" rule that was considered was not added: it would have duplicated it.
- **U1.** The 0.3 mm stub and its single via are replaced by a downward exit and one 0.9/0.5 via per VIN pin (4 and 5). Pin 5 was not connected.
- **Bulk C9 and C18**, front side, vertical, + at the top: C9 at (103.5; 120.5), C18 at (111.5; 120.5). Each terminal: solid pour and 4 × 0.9/0.5 vias in a row (per §9.1). The + vias are at X ≥ 101.25, outside the logic pocket.
- **EN/OVLO.** The R4 → IC1.2 connection was a *graphic line* on F.Cu, not a track: 0.014 mm from the DVDT pad, and not recognised as a connection. Replaced by a 0.2 mm track that leaves horizontally then angles down to R4.1.
- **U7 exposed pad** (done on 20.09): 3 × 3 grid of 1.2 mm GND pads at 1.65 mm pitch (pad 61). Solid GND pour on F.Cu (`U7_EPAD`, X 80.6 → 85.1, Y 97.1 → 101.6) joining the 9 pads, and 4 × 0.5/0.2 vias, plugged both sides, at the 4 diagonal centres, *between* the pads (no via in a pad). 0.15 mm annular ring, above the 0.13 minimum. They also stitch the L3 GND under the module.
- **+3V3 zone** (done on 20.09): fed by two 0.9/0.5 vias — U1 VOUT (pin 1) exits upwards, away from LX, via at (92.16; 106.4); U7's 3V3 pin (pin 3) exits to the left, outside the package, via at (74.2; 95.1). C15/C16 and C5–C7 sit against those pins, the vias stay behind them. Consequence: the L3 GND shrinks to the corridors and edges (76 mm²); the 6 stitching vias under the module and the 4 exposed-pad vias now go through the 3V3 plane and only stitch L1, L2 and L4.
- **Cut-out under the antenna (20.09)**: the Edge.Cuts outline is notched at the top left so that nothing sits behind U7's antenna — horizontal edge at Y 91.60 (just below the keepout, which stops at 91.55), from the slanted left edge at X 71.80 to X 105.60, then straight up to the top edge at Y 86.36, which now only runs from X 105.60 to 138.60. The top-left corner arc was removed. Outline closed in a single piece, checked. Y 91.80 was tried first but put the copper of U7's top pads exactly on the 0.3 mm edge limit.
- **DRC**: 43 violations, none of clearance or width on +5V. New one: C9 silkscreen over C18's outline (cosmetic).

**Still to do on L3** (at that date).

- **Done on 20.09:** `GND_L3` zone at priority 0 and 6 × 0.6/0.3 stitching vias in the band under the module (Y 108 and 111), the only place "around the module" where L3 is GND. No via under the module (back side reserved for passives), nor under the +5V planes, where it would connect nothing on L3. The L3 ground under the module is also stitched by the 4 exposed-pad vias of U7. The gap between the two +5V zones was increased to 1 mm on 20.09: a dry-run fill confirms that a 0.5 mm ground corridor forms along the whole boundary (under the node, vertical at X 100–101, horizontal at Y 110–111). That corridor must stay connected to the rest of the L3 GND, otherwise "remove islands" will delete it.
- **DON'T FORGET — after routing:** extend `5V_servos` onto L4 (B.Cu) under the servo group, and connect it to the L3 zone with 0.9/0.5 vias spread over the common area, roughly every 5 mm (§9.1, "Extend the servo zone onto L4"). Two layers in parallel halve the resistance. To be done *after* routing: L4 also carries signals, and +5V copper poured too early would block them. These are the only "area" +5V vias to plan: the +5V plane only exists on L3, it has no stitching like GND, just one drop at the input (star node) and one per load.
- Extend `+3V3` towards the LSM6DSOX, J6 and the I²C pull-ups once they are placed.
- Adjust the logic/servo boundary (Y ≈ 110.5–111) once the connectors are placed.

### 9.1c End of day, 20.09 — clean-up, zones, rules

- **Outline**: fillets added on the antenna notch (R 1 mm where it meets the slanted left edge, R 2 mm on both corners of the step).
- **D5, second SMAJ5.0A**: added to the schematic in parallel VBUS/GND next to D1, **marked DNP** ("D3" was taken by an LED). On the PCB it is *on the back*, at (95; 123.5), VBUS via into the L3 plane and GND via into In1. Two shorter locations rejected: to the east on the front side (CC2, GND stubs, NPTH screw) and under the connector ("no through-hole inside a courtyard" rule).
- **USB: routed by hand**, after three unsuccessful automatic attempts. Row B joins the J1 node going south then diagonally on the back. Coupled pair at a constant 0.35 mm pitch, lengths 34.3 and 37.0 mm, one 0.45/0.20 via per net. 17 to 18 mm stubs for the reversed orientation: no effect, the ESP32-S3's USB is Full Speed (12 Mbit/s). **Why the automatic attempts failed**: between the D+ drop and MH1, the D− (Y 118.43), D+ (Y 118.79) and CC1 (Y 119.19) traces then the MH1 pad (Y 119.49) leave nowhere the 0.9 mm a via needs.
- **5V servo zone extended up the right side**: L-shaped, column X 131 → edge over the full height plus the bottom band, 854 mm². J7 to J10 were previously fed by mistake from the logic plane. `5V_logique` shrinks to X 89 → 130, 656 mm². 1 mm gap.
- **J11** moved down 1.6 mm: its retention tabs bit into the antenna notch.
- **Schematic cleaned**: J12, J13 (buttons), J16 (UART0), R13 and R17 removed *(they came back on 22.09)*. **J2 (TC2050) and SW1 kept** — no test pads, JTAG is enough. SW1 had been deleted by mistake and was recovered from KiCad's automatic backups.
- **J5 and J6** changed to **vertical** 1.0 mm JST SH (`BM04B-SRSS-TB`) instead of horizontal *(finally 4-pin JST-PH on the back, see status-and-history §2)*.
- **Grounds to review**: U9 has its 4 GND pads 7.5–9 mm from the nearest via, U7's bottom row 4–6 mm, IC2 3 mm. Each pad needs a via right next to it, especially under the class-D amplifier. *(Done on 22–23.09.)*

**Rules changed on the evening of 20.09**: via annular ring 0.13 → **0.125 mm** (makes the 0.45/0.20 via legal, the smallest without a JLCPCB surcharge); 0.45/0.20 size added to the predefined vias; "3V3 width" 0.3 → **0.2 mm** (0.2 mm in 35 µm carries 0.7 A, the rail peaks at 0.5 A and mostly flows through the plane); "5V width" 0.8 → **0.3 mm** for package exits; "Fine pitch: width" extended to IC2, U6 and U7; and a new **"5V servo width"** rule requiring 0.8 mm as soon as a 5 V track touches J3, J4, J7–J10, C9 or C18, so as not to lose the protection where the current flows.

**DRC on the evening of 20.09: 67 violations**, 60 of them silkscreen. Remaining: 2 × 5 V segments at 0.25 mm, the 0.6/0.3 VBUS via at (87.5; 119.5) against "Power via", two overlapping GND vias at (82.02; 98.52), C8 overlapping C26, and U6 without a courtyard. **ERC: 27**, 19 of them background; remaining: wire stubs and two free pins (TXD0, RXD0) to mark "no connect". *(All fixed since, see §0c.)*

### 9.2 Rules

- Exposed pad of the MAX98357A and of the module: 4 vias to L2. *Module: done on 20.09 (§9.1b).*
- OUT+ / OUT− as a tight, short and wide pair, on L1 over solid L2: ~100 mΩ of trace = 5 % of the power lost into 4 Ω. GND stitching vias alongside.
- Ground return of the servos and speaker: neither under the LSM6DSOX nor under the antenna.
- I²S IO10/11/12: short, grouped, on L1.
- USB D+/D−: pair, matched lengths, no via if possible.
- JTAG: 4 short traces, nothing shared.
- GND stitching vias every ~5 mm along the edges of the L3 zones and around the module.

---

## 10 — Remote USB-C port (15 cm)

USBC1 stays on the board. The socket reachable in the enclosure is an **Adafruit ADA6050 breakout** (sunken USB-C, 20.4 × 14.7 × 3.3 mm), wired directly — no more J9 connector, no home-made board, no second USBLC6 to design. *(Finally connected through J1, a 4-pin JST-PH "USB extender".)*

| Item | Detail |
|---|---|
| Wires | **only 4**: VBUS, D−, D+, GND. The 5.1 kΩ on CC are already fitted on the breakout, nothing to add |
| Tap point | **`USB_D+in` and `USB_D-in`** — connector side of the USBLC6 (D1/3 and D1/1), not the MCU side. Otherwise the remote socket bypasses the ESD protection. VBUS and GND tap the nets of the same name |
| Wire gauge | VBUS and GND in **24 AWG**, the largest the J1 JST-PH contacts accept (decision 19.09). ≈ 25 mΩ round trip over 15 cm, i.e. 0.08 V at a 3 A peak. PH contacts are rated 2 A: servo peaks are fine, a continuous 3 A is not. D+/D− in twisted 26–28 AWG, kept apart from the power wires and away from the antenna and the speaker cable |
| Stub on the board | a few millimetres between the tap point and the cable exit: a long stub on a differential pair causes a mismatch — tolerable at Full Speed, but no need to push it |
| Mounting | the breakout is not a panel connector: plan a printed bracket or a cable gland, otherwise every plug-in pulls on the wires |

**Two accepted limitations.** The breakout's 5.1 kΩ advertise 1.5 A, not 3 A — in practice a wall charger supplies what it can, but a strict source would cap. And D1 is 15 cm away: a discharge on that socket travels the whole cable before being clamped, so ESD protection is degraded there. A panel-mount USB-C socket with an integrated cable tail would solve both, at the price of a more expensive part.

**Never plug both sockets into two hosts at the same time**: the two VBUS would end up in parallel.

The CC lines of the two sockets remain separate nets — each connector has its own CC pins. The board's R1 and R5 and the breakout's resistors never see each other; there is no conflict.

---

## 10b — KiCad settings applied to the project

Already written in `Turret2.kicad_pro`.

| Setting | Value |
|---|---|
| Net classes | 10: Default 0.2 · Power5V 0.8 · Audio 0.8 · Power3V3 and GND 0.5 · Servo, LEDData, Analog 0.3 · I2S 0.25 · USB 0.2 mm. 0.9/0.5 mm vias on Power5V, Power3V3 and Audio, 0.6/0.3 mm elsewhere, GND included (stitching vias). Widths reduced on 18.09 so small parts can be reached; the 5 V trunk (2.0 mm) and OUT+/OUT− (1.5 mm) are laid by hand. 19.09 revision: GND vias reduced from 0.9/0.5 to 0.6/0.3. `Power5V_Periph` removed with the AP22811; patterns target `+5V`, `VBUS` and `+3V3` |
| Automatic assignment | 21 patterns (`*GUN_*`, `*NEOPIXEL*`, `*OUT+`, `*HALL_*`, `*BCLK`…). They match nothing while the schematic uses the old net names; nets get their class as they are renamed |
| pcbnew drop-down lists | widths 0.2 / 0.25 / 0.3 / 0.5 / 0.8 / 1.0 / 1.5 / 2.0 mm *(0.4 added on 23.09)*; vias 0.6/0.3 and 0.9/0.5; differential pair 0.2 / 0.15 mm, via gap 0.25 mm (preset recreated on 19.09, it had disappeared) |
| DRC constraints | track 0.15 · clearance 0.15 · via 0.45 / drill 0.2 · annular ring 0.13 *(0.125 since 20.09)* · hole-to-hole 0.25 · board edge 0.3 mm. Minimum drill lowered from 0.3 to 0.2 mm on 19.09: the thermal vias built into the WSON footprint of eFuse IC1 are 0.2 mm, and this global constraint cannot be relaxed by a custom rule |
| Thermal relief | 0.6 mm spokes, 0.4 mm gap, `pad_connection` left on *thermal* (the right default: power SMD pads are set to *solid* individually) |
| Severity | `missing_courtyard` raised from *ignore* to *warning* |
| Already in place | 4 layers, `In1.Cu` = GND, `In2.Cu` = PWR — per §8, nothing to change |
| Deliberately unchanged | dielectric stack-up left generic (0.48 mm core). It only feeds the impedance calculator, the total thickness and the 3D view — irrelevant here, Full Speed USB needs no controlled impedance |

### 10b.1 Custom DRC rules — `Turret2.kicad_dru`

File at the project root, loaded automatically by pcbnew (visible in *Board Setup → Design Rules → Custom Rules*). 13 rules since the 19.09.2026 revision *(plus the rules added later, see the file itself)*.

**Why this file exists:** a net class's `track_width` and `via_diameter` values are only *routing defaults* — pcbnew preselects them, but nothing prevents routing thinner and the DRC says nothing. Only `clearance` and `diff_pair_gap` are checked. These rules turn the widths into real constraints.

| Rule | Effect |
|---|---|
| 5V width | min 0.8 / opt 2.0 mm. The minimum follows the Power5V class (0.8 mm since 18.09): with the old 0.9 min, every track laid at the default width was an error. The opt reminds of the 2.0 mm servo trunk *(min lowered to 0.3 on 20.09, with the "5V servo width" rule enforcing 0.8 near the servos)* |
| Audio width | min 0.8 / opt 1.5 mm on OUT+/OUT− (min aligned on 19.09 with the Audio class at 0.8 mm) |
| GND width | min 0.2 / opt 0.5 mm. GND and 3V3 join their L2/L3 planes: their tracks are only short pad → via links, which cannot be wider than the 0.3 mm pads. The old 0.6 min produced errors on every pad exit |
| 3V3 width | min 0.3 / opt 0.5 mm (the main track to the module stays at 1.0 mm, §3.6b) *(min 0.2 since 20.09)* |
| Servo and LED width | min 0.25 / opt 0.3 mm (Servo, LEDData, Analog) |
| Power via | diameter ≥ 0.85 mm, drill ≥ 0.45 mm, **vias only** (condition `A.Type == 'Via'`), on Power5V and Audio. Before: it also applied to pads (errors on IC1's thermal vias) and to GND/3V3, contradicting the 0.3 mm stitching vias of §9.2 |
| Audio clearance | 0.3 mm around OUT+/OUT− |
| Analog Hall clearance | 0.5 mm between `HALL_LEFT`/`HALL_RIGHT` and the 5 V. Targeted by net name: the Analog class also contains the radar UART, whose TX sits next to the 5 V on J5 at 1 mm pitch (0.4 mm gap by construction) |
| **In1 reserved for GND** | forbids any non-GND track on L2 — guarantees the continuous ground plane of §8 |
| **In2 reserved for power** | whitelist: only Power5V, Power3V3 and GND are allowed on L3. The old blacklist let I2C, Audio and Buck_LX through. A signal crossing a slot between two zones loses its return plane |
| USB differential pair | gap ≥ 0.12 mm / opt 0.15; width ≥ 0.18 / opt 0.2 mm |
| Fine pitch: width | min 0.2 mm for tracks inside the footprint area of USBC1, IC1, U1, U9 and D2: a track cannot be wider than the 0.25–0.3 mm pad it leaves. It widens as soon as it leaves the footprint |
| Fine pitch: clearance | 0.2 mm inside the same areas (except the USB pair against itself). Adjacent pads there are 0.2 mm apart by construction: the 5 V (0.25) and audio (0.3) clearances were flagging the footprints themselves |

### 10b.2 Using net classes day to day

- **Check the assignment right after the first *Update PCB from Schematic***, not at the end of routing. In *Board Setup → Net Classes*, the right-hand column shows which nets each pattern catches. A net left in *Default* points to a pattern that does not match.
- **Colour by class.** *Appearance → Nets/Netclasses* panel: Power5V coral, Power3V3 purple, Audio amber. One glance shows whether `+5V` took its class.
- **Hierarchical name trap.** A net declared in `mcu.kicad_sch` may be called `/mcu/5V` rather than `5V`. Most patterns start with `*` for that reason, but `5V`, `/5V`, `3V3`, `/3V3`, `GND`, `/GND` and `VBUS` are hard-coded — adjust them if renaming produces other forms.
- **Priority.** A net can match several patterns; the class's `priority` field decides. All classes are at the same level, so do not create overlapping patterns without adjusting priorities.
- **Net class directive in the schematic** (*Place → Netclass Directive*): attached to a wire, it travels with the sheet and survives a change of project. Useful for an isolated net that fits no pattern; patterns are enough for the rest.

---

## 10c — Ordering from JLCPCB: what is not set in KiCad

The fabrication plugin produces the Gerbers, drill files, BOM and CPL. **None of these formats carries copper weight, surface finish or layer count** — a Gerber describes shapes, not materials. These choices are made on the order page after uploading the zip.

| Option on jlcpcb.com | Choose | Why |
|---|---|---|
| **Surface Finish** | **ENIG** | the finish is functional here: the TC2050 pogo pins need a flat surface, HASL leaves uneven solder domes *(TC2050 removed since, but ENIG is still advised for the LGA, TQFN and WSON packages)* |
| **Inner Copper Weight** | 0.5 oz (default) is enough | the large L3 zone extended onto L4 gives plenty of cross-section; 1 oz inner copper is a notable surcharge in small runs. Keep in mind that the inner zones have **half** the cross-section KiCad shows, since it displays 0.035 mm everywhere |
| Layers | 4 | detected automatically from the Gerbers |
| Impedance Control | no | useless for Full Speed USB; it is what would require a precise stack-up |
| Thickness, mask colour | to taste | 1.6 mm standard |
| Stencil | yes | SMD assembly on a hot plate (§9.1) |

---

## 11 — Firmware: what to freeze

- Pinout frozen on `src/pins.h`: never reassign IO1, IO2, IO4–IO12, IO14–IO18, IO35, IO36.
- Accelerometer: adapt `Motion.h` (object type) and `lib_deps` for the LSM6DSOX instead of the ADXL345. Same `sensors_event_t` interface.
- The firmware attaches 6 servos (`Wing` × 2, `Gun` × 2, rotate X/Z). With only 3 servos wired, IO1/IO2 output PWM into nothing: harmless.
- `AMP_SD` IO13: normal push-pull. High = active (left channel), low = shutdown. Mono by duplicating the sample into both slots, otherwise −6 dB.
- `AMP_GAIN` IO21 / `AMP_GAIN_100K` IO47: **never drive high**. Changing gain: SD low → set → 10 ms → SD high. Volume in software.
- **Never stop LRCLK while BCLK is running**: large DC voltage at the output, speaker burnt. Shutdown: ramp DIN to zero → `AMP_SD` low → stop the clocks.
- Allowed LRCLK: 8, 16, 32, 44.1, 48, 88.2, 96 kHz. BCLK = 32, 48 or 64 × LRCLK. 50 % duty cycle, otherwise it switches to TDM at 12 dB.
- ADC: only IO8/IO9 (ADC1) with Wi-Fi active. Hall sensors on 3V3, consistent full scale.
- Radar: `Serial1.begin(256000, SERIAL_8N1, 17, 18)` — unchanged.
- ~~eFuse `STRAP_JTAG_SEL` to burn before the first OpenOCD session; `DIS_USB_JTAG` and `DIS_PAD_JTAG` must stay at 0. SW4 closed = external JTAG.~~ *No external JTAG since 22.09: do not burn `STRAP_JTAG_SEL` if SW1 is used as a configuration switch.*
- `PWR_FLT` on IO38, input with external pull-up: low = the eFuse has cut off (overcurrent, overvoltage or overtemperature). Good candidate for the red LED and the serial log.

---

## 12 — Verification and pre-fabrication checklist

Run the ERC on the whole project, not sheet by sheet. Run the DRC after filling the zones. Then:

> This checklist was written before layout and uses the old designators; several items were superseded later (JTAG removed, connector families changed, Hall connectors split). The final pre-production review is in status-and-history §13.

**Schematic**

- No trace of BQ24075, BQ27441, CN1, U8 (STM32 JTAG), U9, nets SYS/VBAT/BatPGOOD/BOOT0/NRST
- §1 table respected, every free IO marked no-connect
- ESP32-S3-MINI-1-**N8** module
- D1 diagonal, right next to USBC1, R1/R5 5.1 kΩ
- D2 SMAJ5.0A and C2 100 nF each **in parallel** VBUS → GND, never in series
- eFuse: IN on `VBUS` (both pins), OUT on `+5V`, GND + pad to ground
- OVLO threshold 1 M / 249 k = 6.0 V; ILM 523 Ω = 3.86 A; dVdt 10 nF *(47 nF since)*
- FLT pull-up to `+3V3`, never to 5 V; `PWR_FLT` → IO38
- TPS631000 EN tied directly to `+5V`
- +5 V rail in two branches from the eFuse OUT; bulk 2 × 220 µF OS-CON on the servo side, 100–220 µF on the amplifier side — star done on 20.09 (§9.1b)
- Regulator ceramics 22 µF **25 V** X7R 1210 (C6 input, C4 + C5 output)
- TPS631000: VIN on `+5V`, EN on `+5V`, 22 µF 25 V + 100 nF 0603 right at the pins
- MAX98357A: VDD 5 V, SD_MODE via 2 kΩ with no pull resistor, IO21 direct + IO47 via 100 kΩ ±5 %, thermal pad
- LSM6DSOX: CS 3V3, SA0 GND (0x6A), INT1 on IO37, 100 nF + 1 µF
- I²C pull-ups **2.2 kΩ** to `+3V3` right next to the LSM6DSOX — **only one set on the whole board**
- J8 Hall on **3V3**
- SN74AHCT125: VCC logic `+5V`, OE to GND, 100 nF, 3 gates wired, 4A to GND
- J3–J14 with the §6 pinout (IO-sheet designators, not those of the reference project)
- Healthy hierarchy: `kicad-cli sch export netlist` with no warning, 3 pages, no "automatically fixed" message on opening (§0b.1)
- Cross-sheet labels really **global** — no net prefixed `/MCU/` or `/IO/` in the final netlist (§0b.2)
- J1 and J2 oriented with cables leaving outwards — §12b.1, wires pinched against the Z-axis support on the V4
- Board orientation (flat or vertical) decided **before** starting placement — §12b.3
- Centre LED: separate red LED, or addressable one chained on J12? — §12b.3
- IO26 / IO34: keep or remove R19/C11 and R20/C12, leftovers of the buttons — §12b.4
- J13 Qwiic fitted, **without local pull-ups**, reachable in the enclosure
- 10-pin Cortex JTAG, pin 10 on EN; reset and boot buttons (UART0 removed)
- Straps IO0, IO45, IO46 respected; IO3 dedicated to the JTAG_SEL strap with R_SEL 10 kΩ and SW4
- SW4 is a slide switch (not a push button), reachable in the enclosure
- Remote cable tapped on `USB_D+in` / `USB_D-in`, not on the MCU side
- `PWR_FLAG` on `VBUS`, `+5V`, `+3V3` and `GND` before the ERC

**Layout**

- 4 layers, L2 solid GND with no tracks, one single GND net
- Antenna keepout on all 4 layers, outline included
- Amplifier, speaker, servo bulk and inductor opposite the antenna
- Separate L3 zones (VBUS / +5V servos / +5V logic / +3V3), single junction at the eFuse OUT pad, the rest GND, no floating island — VBUS and both +5V done on 20.09; +3V3 and GND to do
- **After filling**: visual check that a ground corridor separates the two `+5V` zones — same net, so the DRC will say nothing
- L1 copper pour around the OUT pad + 4 × 0.5 mm vias (2 per zone)
- Servo bulk and amplifier bulk placed **on the current path**, not as side branches
- Power SMD pads with solid connection; through-hole connectors with 4 × 0.6 mm thermal relief
- Servo zone extended onto L4 and connected with vias
- Exposed pads with 4 vias; each decoupling capacitor with its two vias within < 2 mm
- OUT+/OUT− as a wide, short pair on L1
- Servo/speaker ground return away from the LSM6DSOX and the antenna
- 0.3 mm GND stitching vias every 5 mm (edges of the L3 zones, around the module, along OUT+/OUT−)
- §9.0 DRC rules entered in KiCad before routing (done: §10b) and `Turret2.kicad_dru` loaded without error
- Net class assignment checked right after the first *Update PCB from Schematic*
- Widths per the summary: 2.0 / 1.5 / 1.0 / 0.8 / 0.5 / 0.3 / 0.2 mm
- Stack-up confirmed at order time (inner copper 0.5 oz or 1 oz)

**Sourcing**

- 1210 footprint present in the library (the ceramics were 0805/1206)
- JLCPCB extended parts accepted or replaced
- Ordered: MAX98357A, SN74AHCT125PWR, TPS259573DSGT, 5 × MCAST32MSB7226KPNA01, SMAJ5.0A, ADA6050, EEE-FK1C471P *(since replaced by 2 × 16SVPG220M)*
- Connectors: 2.54 mm headers for the servos, JST-XH for home-made cables, JST-PH or terminal block for the speaker (§13.4)
- JST families checked: SH 1 mm ≠ PH 2 mm ≠ XH 2.54 mm
- Servo bulk: 2 × 16SVPG220M available (4.1 A ripple, ESR 14 mΩ)
- 5 V / 3 A wall charger planned; remote cable 24 AWG (JST-PH)
- JLCPCB order: **ENIG** selected (mandatory for the TC2050)
- Inner copper: 0.5 oz accepted, L3 zones sized accordingly
- After routing: `5V_servos` doubled on L4 and connected to L3 with 0.9/0.5 vias every ~5 mm (§9.1, §9.1b)
- Stencil ordered

---

## 12b — Field feedback (portal-turret community)

Taken from the V4 design discussions. These are observed failures and open debates, not theoretical recommendations.

### 12b.1 The two confirmed hardware bugs of the V4

| Problem | What happened | Handled in this plan |
|---|---|---|
| **Hall sensors on 5 V** | Board surgery after fabrication: traces cut and rewired to the 3.3 V pin. The sensor output follows its supply voltage, and IO8/IO9 are not 5 V tolerant | Yes — J8 pin 1 on `+3V3` (§6). Confirmed in the field, not inferred |
| **Gun wires pinched** | The cables run against the Z-axis support | Mechanical, but it dictates the orientation of J1 and J2: **their cables must leave towards the outside of the board, not towards the centre** |

### 12b.2 Voltage dips: the most cited failure

The point the discussion insisted on most. With six servos moving together, current peaks create dips on the rail that, according to contributors, **damage the microcontroller over time**. The community recommendation is 100 nF plus a 47 to 100 µF electrolytic as a minimum, with heavy-gauge wire and a good power supply.

This plan goes further (2 × 220 µF polymer on the servo side, separate branches, voltage-drop budget in §2.3), but this feedback confirms it is not overkill: it is **the most frequently reported cause of failure** on this project.

### 12b.3 Open debates — what could change the decision

| Topic | The two positions | What this plan does |
|---|---|---|
| **Connector pitch** | 2.54 mm: the servos already come with it, and many people cannot crimp. Against: "widespread, but takes too much space" — one contributor moves to 1 or 1.25 mm JST | 2.54 mm, for compatibility with the cables and the gun daughter-board. **If space runs out during routing, JST is acceptable to the community** — not frozen *(the board finally uses 2 mm JST-PH everywhere, see status-and-history §2)* |
| **Centre LED** | Replace the fixed red LED with an addressable one chained on the ring: one wire less, and the eye colour becomes configurable. One contributor has already done it and is adapting the firmware | **To consider.** IO14 already drives the ring through the AHCT125: the chain would just be one LED longer. Zero hardware cost, one connector less |
| **Board orientation** | Joran considers mounting it *vertically* to reach the connectors, and notes elsewhere that it should be "upside down" to stay accessible | **Undecided, and it changes the whole placement**: a vertical board = all connectors on one edge instead of spread around the perimeter. To decide before §8 *(the board is mounted vertically)* |
| **USB socket** | Several people prefer a detachable magnetic USB-C adapter to a fixed socket, for looks and ease of plugging | Would replace the remote ADA6050 **without changing anything on the board** — a purely mechanical decision, can be postponed |

### 12b.4 Spare pins

The reference project leaves "7 or 8 unused pins, which leaves room for fixes or future developments". This plan is tighter: after `PWR_FLT` on IO38, only IO26 and IO34 remain free, plus the unusable straps. Keeping those two as no-connect with reachable test pads would be cheap insurance.

**21.09**: the J12/J13 buttons (and their 1 k R13/R17) were removed on 20.09. On IO26 and IO34 remain R19/C11 and R20/C12 (10 k to 3V3, 100 nF to GND), useless without a connector. Neither test pads nor JTAG expose them: to decide — remove them or give them an output. *(Decided on 22.09: the buttons came back on J12/J13, see §0c.)*

---

## 13 — Reference bill of materials: values and footprints

Summary of all the decisions made in this document. Where it differs from the text, **this table prevails** — it is updated last. *(For the parts actually on the board, the KiCad schematic and BOM prevail over this table.)*

### 13.1 ICs and modules

| Ref. | Part | Footprint | Source |
|---|---|---|---|
| U7 | ESP32-S3-MINI-1-**N8** | module, overhanging antenna | in stock |
| IC1 | TPS259573DSGT — eFuse | **WSON-8** 2×2 mm, exposed pad | to order |
| U1 | TPS631000DRLR — 3V3 buck-boost | **SOT-563 / DRL** | in stock |
| U3 | MAX98357A — I²S amplifier | **TQFN-16** 3×3 mm, exposed pad | to order |
| U10 | SN74AHCT125PWR — level shifter (IO sheet) | **TSSOP-14** | to order |
| U15 | LSM6DSOXTR — accelerometer + gyroscope | **LGA-14** | in stock |
| D1 | USBLC6-2SC6 — ESD on D+/D− | **SOT-23-6** | in stock |
| D2 | SMAJ5.0A — TVS on `VBUS` | **SMA (DO-214AC)** | to order |
| L1 | 1 µH, saturation ≥ 3 A | FTC303020D or equivalent | in stock |

### 13.2 Capacitors — mandatory voltage rating, 0603 by default

**Rules for the whole board:**

- **Size: 0603 for everything still to buy.** Easier to hand-solder, reworkable, higher voltage ratings for the same capacitance.
- **No 0402**, and none is needed here: no BGA, no fine-pitch QFN. Even around the eFuse's WSON-8, decoupling sits next to the package. The existing 0402 stock remains available as spares, but the BOM is entirely 0603.
- **Voltage: at least 5× the working voltage** for ceramics, 2× for electrolytics. Class II dielectrics (X5R as well as X7R) lose capacitance under bias; the only lever is the applied / rated voltage ratio. At 20 % almost nothing is lost, at 80 % three quarters are gone. Electrolytics do not have this flaw, but their lifetime collapses near their rated voltage.

| Role | Working | Value and **voltage** | Footprint | Why |
|---|---|---|---|---|
| TPS631000 input and output (C6, C4, C5) | 5 V / 3.3 V | 22 µF **25 V** X7R MCAST32MSB7226KPNA01 | **1210** | the only size available for 22 µF 25 V X7R. Soft termination: 1210 is the most prone to flex cracking |
| General decoupling — all rails | 5 V and 3.3 V | 100 nF **50 V** X7R | **0603** | one part for the whole board: regulator, amplifier, module, LSM6DSOX, AHCT125, connectors, eFuse input |
| 10 µF ceramic on the 5 V | 5 V | 10 µF **50 V** X7R GRM21BR61H106KE43L | **0805** | 3 in stock. A 10 µF/50 V does not exist in 0603 — keep 0805 |
| C29 — module EN | 3.3 V | 1 µF **25 V** X7R | **0603** | sets the start-up delay: a 1 µF/6.3 V would lose half its value and shorten the delay accordingly |
| LSM6DSOX | 3.3 V | 1 µF **25 V** X7R | **0603** | with the 100 nF, right next to the package |
| C1 — eFuse dVdt | < 1 V | 10 nF **50 V** X7R *(47 nF since)* | **0603** | start-up ramp; voltage not critical |
| Servo bulk | 5 V | 2 × 220 µF **16 V** OS-CON polymer Panasonic 16SVPG220M (C9, C18) | **SMD Ø6.3 × 5.9 mm** — `Capacitor_SMD:CP_Elec_6.3x5.9` | 4.1 A ripple, ESR 14 mΩ, 5000 h at 105 °C. Replaces the EEE-FK1C471P (470 µF, ESR 160 mΩ, 0.6 A) |
| Amplifier bulk | 5 V | 100–220 µF **16 V** electrolytic | **SMD Ø6.3 × 6.6 mm** *(finally 6.3 × 7.7)* | the EEE-FT1V101AP in stock is fine. **No tantalum**: rail shared with the servos, a failing tantalum goes into a hard short |

**What is kept, what is bought.**

| Value | Size | Origin |
|---|---|---|
| 100 nF 50 V | 0603 | to order — **50 pieces** |
| 1 µF 25 V | 0603 | to order — 20 pieces |
| 10 nF 50 V | 0603 | to order — 10 pieces |
| 22 µF 25 V | 1210 | to order, 5 pieces |
| 10 µF 50 V | 0805 | in stock, 3 pieces |

The board therefore carries four ceramic sizes, but **only one per value**: no risk of mix-up during assembly, and nothing is thrown away.

Moving to 0603 costs nothing electrically: the inductance rises from 0.6 to 0.9 nH, negligible compared with the routing loop. **What matters is the distance to the pin, not the size.**

**What not to do**, and the original mistake on this board: using 6.3 V parts "because the rail is 5 V". The five 100 µF/6.3 V capacitors removed in §2.4 only delivered 15 to 25 µF each.

**X5R or X7R?** Both degrade identically under bias — the difference is the temperature range, +85 °C versus +125 °C. Next to a class-D amplifier and a buck-boost, use X7R when available.

### 13.3 Resistors — all **0603**, 1 %

Same rule as the capacitors: everything 0603, including values already in stock as 0402.

| Value | Qty | Use |
|---|---|---|
| 523 Ω | 25 | eFuse ILM — 511 Ω would also work (3.95 A instead of 3.86) |
| 1 kΩ | 25 | debug LEDs *(finally R12 680 Ω red, R18 100 Ω green: see §3.7)* |
| 2 kΩ | 25 | series on the amplifier's SD_MODE |
| 2.2 kΩ | 25 | I²C pull-ups |
| 5.1 kΩ | 25 | USB-C CC1 and CC2 |
| 10 kΩ | 25 | FLT pull-up, module EN, FB divider |
| 56 kΩ | 25 | regulator FB divider |
| 100 kΩ | 25 | amplifier gain — ±1 % is fine, the datasheet asks for ±5 % |
| 249 kΩ | 25 | OVLO low side — 240 kΩ would give 6.2 V instead of 6.02, acceptable |
| 1 MΩ | 25 | OVLO high side |

Yageo RC0603FR series or equivalent. About 5 CHF for the lot; at 25 pieces you are in the most interesting price bracket and never need to reorder.

| Ref. | Value | Role | Constraint |
|---|---|---|---|
| R1, R5 | 5.1 kΩ | USB-C CC1 and CC2 | one set per connector |
| R3 | 1 MΩ | OVLO high side, from `VBUS` to EN/OVLO | 1.2 × (1000+249)/249 = **6.02 V** threshold. Do not reuse the 316 k from TI's notes: they cut at 5.0 V |
| R6 | 249 kΩ | OVLO low side, to GND | |
| R7 | 523 Ω | eFuse ILM | I = 2000/R + 0.04 = **3.86 A** |
| R2 | 10 kΩ | FLT pull-up to `+3V3` | **never to 5 V** — IO38 is not 5 V tolerant |
| — | **2.2 kΩ** ×2 | I²C pull-ups to `+3V3` | right next to the LSM6DSOX, **only one set on the board** |
| R4 / R11 | 56 kΩ / 10 kΩ | TPS631000 FB divider | 0.5 V reference — do not touch |
| R25 | 10 kΩ | module EN to 3V3 | with C29 |
| R12 / R18 | 680 Ω / **100 Ω** | red LED D3 (IO48) / green LED D4 (IO33) | R18 lowered from 1 kΩ on 19.10.2026; 3 to 13 mA |
| — | 2 kΩ (or 1.5 kΩ from stock) | series on the amplifier's SD_MODE | 1.5 kΩ gives 3.25 V, well above the 1.4 V threshold |
| — | 100 kΩ **±5 %** | IO47 to GAIN_SLOT | the 100 k in stock are ±1 %, so better |

### 13.4 Connectors — choices and families

> Superseded: the board finally uses 2 mm JST-PH for all servo, gun, LED, radar, Hall and remote-USB connectors, 1 mm JST-SH for Qwiic and UART0, and a 3.5 mm screw terminal for the speaker. See the README for the final list. The table is kept for the reasoning.

**Two families, for two different uses.** The servos impose their connector; everything else is home-made cable, where a keyed connector prevents reversal.

| Ref. | Function | Chosen connector | Why |
|---|---|---|---|
| USBC1 | on-board USB-C | TYPE-C-31-M-12, 16 pins *(finally GCT USB4120-03-C)* | in stock |
| — | remote USB-C | **Adafruit ADA6050** breakout, 4 soldered wires | 5.1 kΩ on CC already fitted |
| J3–J6 | wing and rotation servos (3-pin) | **straight 2.54 mm male header**, no housing | MG90S servo cables have a Futaba-type housing that plugs straight onto a bare header. **No other connector is compatible** — a JST-XH or PH would require re-terminating every servo |
| J7, J9 | guns: servo + LED (4-pin) | | |
| J10 | LED ring + eye (3-pin) | **JST-XH 2.54 mm** through-hole, or a 2.54 mm header to stay strictly community-compatible | home-made cables: the XH keying and latch prevent reversal and vibration unplugging. Same pitch as headers, so no space lost. The reference project uses bare headers — the only argument against |
| J11 | HLK-LD2450 radar (4-pin) | | |
| J12 | Hall sensors (4-pin) — **3V3** | | |
| J13 | Qwiic / STEMMA QT | **JST-SH 1 mm, 4-pin right-angle** — Adafruit 4208, in stock | Qwiic standard, ready-made cables, chainable |
| J14 | speaker (2-pin) | **JST-PH 2 mm**, or 3.5 mm screw terminal | JST-PH is more compact and latched; the terminal lets you change speakers without crimping. Either |
| ~~J?~~ | backup UART0 (5-pin) — removed on 20.09 | 2.54 mm header, or plain test pads | backup port, used once every six months |
| — | JTAG | **Tag-Connect TC2050 footprint** | pads only, no component |
| SW2, SW3 | boot and reset | TS-1088-AR02016, in stock | push buttons |
| SW1 | USB / external JTAG selection | **SPST slide switch** | IO3 is read at reset: a push button would have to be held while pressing RESET |

**The 2.54 mm versus 1.25 mm JST debate** runs through the community (§12b.3): "widespread but takes too much space" on one side, "the servos already come with it and many people cannot crimp" on the other. The position taken here is in between: **bare header where the cable imposes it** (the six servos), **JST everywhere else**. If space runs out during routing, moving J10 to J12 to 2 mm JST-PH frees a few millimetres without breaking anything.

**Do not mix up the JST families**: SH = 1 mm (Qwiic), PH = 2 mm, XH = 2.54 mm. They are not interchangeable, and the KiCad footprints are in `Connector_JST` under those names.

### 13.5 Cables

| Link | Gauge | Reason |
|---|---|---|
| Remote USB — VBUS and GND | **24 AWG** | 3 A servo peaks; 0.08 V round trip over 15 cm; limit of the JST-PH contact |
| Remote USB — D+ and D− | twisted 26–28 AWG | away from the power wires, the antenna and the speaker cable |
| Speaker | 22–24 AWG | BTL output: never tie OUT− to ground |

### 13.6 Footprints — KiCad names and pitfalls

| Part | KiCad footprint | Watch out |
|---|---|---|
| Standard passives | `Resistor_SMD:R_0603_1608Metric` `Capacitor_SMD:C_0603_1608Metric` | **Smallest size on the board.** A 0402 footprint anywhere is a leftover from the original schematic to replace |
| Regulator 22 µF | `Capacitor_SMD:C_1210_3225Metric` | new size on this board, only for those three *(four since, C3 included)* |
| 10 µF 50 V | `Capacitor_SMD:C_0805_2012Metric` | only for the parts in stock |
| IC1 eFuse | `Package_SON:WSON-8-1EP_2x2mm_P0.5mm_EP0.9x1.6mm` | exposed pad: **4 vias underneath** |
| U3 MAX98357A | TQFN-16 3×3 mm | exposed pad, thermal and not connected internally → solid ground plane + 4 vias |
| D2 SMAJ5.0A | `Diode_SMD:D_SMA` | check the cathode direction |
| **J14 Qwiic** | `Connector_JST:JST_SH_SM04B-SRSS-TB_1x04-1MP_P1.00mm_Horizontal` | **1 mm pitch**: the finest footprint on the board. Check that pitch and direction (right-angle, exit towards the edge) match the Adafruit 4208. Both mechanical retention tabs must be soldered, otherwise the connector rips off when unplugging |
| JTAG *(removed on 22.09)* | official **Tag-Connect TC2050** footprint | do not redraw: pads without solder mask, no via inside, keepout on both sides if the cable has legs |
| Electrolytics | SMD Ø8 × 10.2 and Ø6.3 × 6.6 mm | check the direction of the polarity band — the classic mistake |
| 2.54 mm connectors | through-hole, 3, 4 and 5 pins | 4 × 0.6 mm thermal relief on the zones (§9.1), soldered with an iron |

**Before routing**, open every footprint in the editor and compare it with the datasheet — especially the WSON-8, the TQFN-16 and the JST-SH, the three where a 0.2 mm error makes the board unusable.
