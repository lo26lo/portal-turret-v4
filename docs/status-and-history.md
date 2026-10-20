# Turret2 — status and design history

ESP32-S3 board for the Portal turret, derived from the community project **portal-turret-v4**. This file records the current state of the board, the decisions taken session by session, and the pitfalls met along the way. The full design rationale is in [design-plan.md](design-plan.md).

> Dates are in DD.MM.YYYY format (2026). Section numbers are kept stable because the design plan refers to them. Board coordinates are in mm, in KiCad's page coordinates (the outline runs from X 63.5 to 141.6 and Y 86.36 to 127.0).

---

## Current status — 24.09.2026

Routing finished, ground checked, silkscreen harmonised, Hall sensors split onto J17/J18, **pre-production review done** (details in §9 to §13).

- **DRC: 0 errors, 0 unconnected items, 4 harmless warnings**: U7's silkscreen outline clipped by the board edge (overhanging antenna), the two overlapping "\*" in U9's footprint, the logo without a courtyard.
- **Silkscreen harmonised** (§11): function labels at 1.0 mm, designators at 0.8 mm, crowded designators and passive values hidden (kept on F.Fab).
- **Hall sensors** (§12): J6 (4-pin) replaced by J17 HALL_L (front) and J18 HALL_R (back), 3-pin JST-PH 3V3 / signal / GND, one 100 nF each (C31, C32; C28 removed).
- **20.10.2026 — radar**: the radar fitted is an **Ai-Thinker RD-03D V2**, not an HLK-LD2450 (same frames; different connector order, crossed cable described in the README). In the master KiCad project, J5's value and silkscreen go from "HLK-LD2450" to **"Radar"**; the v0.1 boards keep "HLK-LD2450" printed. J5 pinout (board file): 1 +5V · 2 RADAR_TX = IO18 · 3 RADAR_RX = IO17 · 4 GND.
- **19.10.2026 — green LED resistor**: R18 (IO33 → green LED D4) changed from 1 kΩ to **100 Ω** (user's decision). The change is made in the **master KiCad project** (kept outside this repository); `Turret2_portable/` is only a portable copy: its schematic already shows 100 Ω, its board file still 1 kΩ until the copy is refreshed from the master. Red LED unchanged: R12 680 Ω → D3 (IO48).
- **Schematic ↔ board parity**: only 13 "Description" fields differ (TP1–TP4, R13, R17, J12, J13, J16, C31, C32, J17, J18) — the next *Update PCB from Schematic* (F8) refreshes them. No effect on fabrication.
- **ERC**: only the 4 warnings on IO39–42 (J2 still in the schematic, excluded from the board) and the pin type of U9's PAD remain, plus warnings for LCSC symbols not in a declared library.
- **Teardrops** on every via and through-hole pad, generated when filling the zones (B).
- **Stencil**: 2.2 mm F.Paste openings over H1 and H2, to align the stencil on two 2 mm pins.
- **Mounting**: two M2 screws in H1/H2. No room at the top for a third screw (M2 or M1.6); hold the top edge with a groove in the enclosure, away from the antenna.

### Before ordering

1. **SW1**: without JTAG it no longer serves as a strap. Either keep it as a configuration switch read by the firmware on IO3 (R9 as pull-up) — and then **never burn the STRAP_JTAG_SEL eFuse** — or leave SW1 and R9 unpopulated. In both cases change its value "USB / JTAG".
2. **U7**: put **ESP32-S3-MINI-1-N8** in the value or in an MPN field (the value is currently "ESP32-S3-MINI-1"). The N8 is mandatory: on the N4R2, IO26 is taken by the PSRAM.
3. **JLCPCB assembly only**: add LCSC / manufacturer part numbers (almost all are missing); J16, R13 and R17 are SMD parts on the back (double-sided assembly). Fiducials would also have to be added.
4. Schematic clean-up: remove J2 and the JTAG labels (IO39–42 stay "no connect").
5. Order: 4 layers, 1.6 mm, ENIG advised (LGA of U6, TQFN, WSON), stencil 0.10–0.12 mm with the comment "keep the 2 non-pad openings (alignment holes)".

---

## 1. Files

| Item | File |
|---|---|
| KiCad 10.0.6 project | `Turret2.kicad_pro` |
| Root sheet (power) | `Turret2.kicad_sch` |
| MCU sheet | `mcu.kicad_sch` |
| IO sub-sheet | `untitled.kicad_sch` *(to be renamed `io.kicad_sch`)* |
| PCB | `Turret2.kicad_pcb` |
| Custom DRC rules | `Turret2.kicad_dru` |
| Imported footprints (SamacSys) | library nickname `samsic` — `USB412003C`, `LGA-14_1` (U6, courtyard added on 22.09), and others. **Must be shipped with the project**, see the README |
| JTAG footprint | `${KIPRJMOD}/TC2050_IDC` (nickname `TC2050-IDC`) — *J2 removed from the board on 22.09* |

KiCad's automatic backups (`Turret2-backups/*.zip`, one per save) proved useful to find out when something disappeared — that is how the deletion of the `5V_logique` zone was dated (§5).

---

## 2. Decisions taken during the schematic phase (18.09)

### Power

- **C1** on VBUS: 100 µF → **100 nF** (a large capacitance before the eFuse is not protected by its soft start, and USB-C limits it to 10 µF).
- **C2** (eFuse dVdt): 10 nF → **47 nF**. TPS2595 datasheet formula: slew = 42,000 / C(pF) in V/ms, inrush current = slew × C_out. With ≈ 1.2 mF downstream: 0.89 V/ms and 1.1 A inrush, versus 5 A with 10 nF.
- **C4** at the TPS631000 input: non-polarised 100 µF (left over from the old schematic) → **100 nF**.
- Regulator output: **three** 22 µF 25 V 1210 (C5, C6, C7) + C3 at the input. Three and not two: the datasheet asks for 47 µF, and a 22 µF 25 V only gives ≈ 21 µF effective at 3.3 V.
- **C15** (module decoupling): 10 µF 50 V 0805.

### Servo bulk — change of technology

- History: 2 × 470 µF FK (Ø8 × 10.2) → 3 × 220 µF FK (Ø8 × 6.2) → **final decision: 2 × 220 µF OS-CON polymer `16SVPG220M`**, footprint `Capacitor_SMD:CP_Elec_6.3x5.9`, as **C9 and C18**.
- Reason: ESR 14 mΩ instead of 160, 4.1 A ripple instead of 0.6, Ø6.3 × 5.9 mm, 5000 h at 105 °C. On fast edges the ESR sets the dip, and the eFuse caps the current at 3.86 A anyway. The risk of a polymer failing short is covered by the eFuse.

### Buttons

- IO26 → `ButtonA` (J12) and IO34 → `ButtonB` (J13), with R13/R17 1 kΩ in series, R19/R20 10 k pull-ups and C11/C12 100 nF. *Dropped on 20.09, **back on 22.09** (J12/J13 on the back).*
- IO45 and IO46 left as no-connect: they are straps. IO45 high at boot switches the flash to 1.8 V and the module no longer starts; IO46 high prevents download mode.
- Any further extension goes through I²C (J11). IO26 imposes the N8 module (PSRAM on IO26 in the N4R2).

### Connectors

- 2.54 mm Dupont headers dropped.
- **Vertical 2 mm JST-PH** (2 A/contact): J1 (remote USB), J3 and J4 (guns), J7, J8, J9, J10 (servos), J14 (LED ring), J5 (radar, 4-pin, back), J17/J18 (Hall, 3-pin, §12).
- **Right-angle 1 mm JST-SH** (1 A/contact): J11 (Qwiic), J16 (UART0, 5-pin, on the back).
- J12/J13 (buttons): 2-pin headers at 2 mm pitch, on the back.
- J15 speaker: 3.5 mm screw terminal.
- **Every JST-SH has an MP pad**: symbol `Connector_Generic_MountingPin:Conn_01x0N_MountingPin` with MP tied to GND, otherwise "No net found for pad MP" on import.
- Consequence: JST housings must be crimped onto the servo cables (Dupont originally) and the radar cable (1.25 mm). Do not go below 26 AWG for the servos.

### USB-C

- **GCT USB4120-03-C**, **vertical** receptacle (the board is mounted vertically, access is from the side with the cover open). Imported footprint: `samsic:USB412003C`.
- The symbol's shield pins were renumbered **0–3 → MH1–MH4** to match the footprint's through-hole anchors: the shield goes to GND automatically.
- The remote port J1 is kept (the cover must be opened to reach the on-board socket).

### Schematic, miscellaneous

- **SW1** (USB/JTAG selection): CAS-120A slide switch → **1-position DIP switch**, symbol `Switch:SW_DIP_x01`, footprint `Button_Switch_SMD:SW_DIP_SPSTx01_Slide_Copal_CHS-01A_W5.08mm_P1.27mm_JPin`. Pin 1 on `JTAG_SEL`, pin 2 to GND.
- **SW2 and SW3** (BOOT, RESET): `SW_SPST_TS-1088-xR020`. Never a slide switch on RESET.
- **J16 UART0**: removed on 20.09, **back on 22.09** in place of JTAG. TXD0, RXD0, GND, EN, IO0, MP to GND. Automatic flashing with an ESP-Prog (built-in auto-reset circuit); with a plain USB-serial adapter use SW2/SW3, and never wire DTR/RTS directly to EN/IO0.
- 100 nF added on each connector (Hall, radar, ring).
- **Radar J5: the original pinout was right.** `pins.h` gives RADAR_RX = 17 (ESP32 input) and RADAR_TX = 18 (output). The crossover is done in the cable. The plan was wrong, not the schematic.
- Pin types of the LCSC symbols changed to **Passive** (resistors, capacitors, USBLC6, TPS631000, LSM6DSOX, USB-C): this removes the false "power input not driven" errors.
- PWR_FLAG on VBUS, +5V, +3V3 and GND.

### Net classes and widths

Four patterns matched no real net and did nothing:

- `3V3` → **`*3V3`** (otherwise the whole 3.3 V rail went to 0.2 mm);
- `*OUT+` / `*OUT-` → **`OUTP`** / **`OUTN`**;
- `*1Y`, `*2Y`, `*3Y` (5 V outputs of the AHCT125) and `*RADAR_*` added to Analog;
- new classes **I2C** (0.25 mm) and **Buck_LX** (0.8 mm, pattern `Net-(U1-LX*)`).

Widths reduced on 18.09.2026, because they prevented routing to small parts:

| Class | Before | After |
|---|---|---|
| Power5V | 2.0 mm | **0.8 mm** |
| Power3V3 | 1.0 mm | **0.5 mm** |
| GND | 1.0 mm | **0.5 mm** |
| Audio | 1.5 mm | **0.8 mm** |

Predefined project widths: 0.2 / 0.25 / 0.3 / 0.4 / 0.5 / 0.8 / 1.0 / 1.5 / 2.0 mm (0.4 added on 23.09). The 5 V trunk is laid by hand at 2.0 mm.

USB nets are named `USB_D+` / `USB_D-` (module side) and `in_USB_D+` / `in_USB_D-` (connector side): the `*USB_D+*` and `*USB_D-*` patterns cover them, and the suffixes suit differential pairs.

### PCB

- **Outline** taken from the V4 gerber `Portal Turret V4-Edge_Cuts.gbr`: trapezoid **78.105 × 40.64 mm** (3.075 × 1.600 in), top edge 68.58 mm, 9.525 mm chamfer at the top left, four **R 3 mm** fillets. Original coordinates kept: X 63.5 to 141.6 mm, Y 86.36 to 127.0 mm — the reference's other layers overlay directly. *(Notched under the antenna on 20.09, see design-plan §9.1b.)*
- **H1 and H2**, M2 non-plated holes Ø 2.2 mm, at (67.342; 123.825) and (139.065; 123.825), taken from the V4 `NPTH.drl`: the printed mount stays compatible.

---

## 3. Status on the evening of 20.09.2026

- Schematic: 83 footprints, ERC 0 errors on 18.09 (19 harmless warnings).
- PCB: outline, holes, 17 footprints placed, L3 layer fully drawn (design-plan §9.1b). No signal routing yet.

## 4. Open items at that date

*Mostly obsolete: placement and routing were finished on 22.09 (§9). Remaining cosmetic schematic items:*

- Rename `untitled.kicad_sch` to `io.kicad_sch`, sheet name "IO".
- Fill in the title blocks of the three sheets (title, date, revision).
- Fix the block title "SN74AHCT128PWR": the part is an AHCT**125**.
- MPN fields of C5 and C6 have an extra space; C3 has no MPN.
- The symbols of C9 and C18 are still named `EEE-FK1V470P` while their manufacturer field says `16SVPG220M`.

---

## 5. Pitfalls met — not to be learned twice

- **Capacitor placed across a wire**: C8 shorted +5V to GND, which merged both rails in the whole netlist. Symptom: the GND net disappears from the netlist.
- **Pin in the middle of a wire**: KiCad does not connect without a junction. Six pins were affected.
- **Net class pattern and sheet path**: a local label carries its path (`/MCU/2Y`), a global label does not (`1Y`). Hence the leading stars in patterns.
- **Module footprint**: `RF_Module:ESP32-S2-MINI-1` is **correct** for the ESP32-S3-MINI-1 symbol; it is KiCad's official association. Do not "fix" it.
- Capacitors rejected along the way: Würth 865080140004 (47 µF **6.3 V**, voltage too low) and Panasonic EEE-1CA220WAR (22 µF but only **28 mA** ripple).
- **A graphic line on a copper layer is not a track.** The EN/OVLO link had been drawn with the line tool: it grazed DVDT and KiCad did not count it as a connection. Always route with the track tool.
- **Two zones of the same net do not keep clear of each other.** To keep a ground corridor between the two +5V zones, the gap must be at least 2 × clearance + minimum width: 1 mm here, not 0.5.
- **A zone with no item of its net is deleted** with "remove islands: always" — hence the two 3V3 vias placed from the start.
- **0.45/0.2 via**: 0.125 mm annular ring, below the project's original 0.13 minimum. The minimum was lowered to 0.125 on 20.09 (JLCPCB accepts it); otherwise use 0.5/0.2.
- **A deleted zone is invisible.** `5V_logique` disappeared on 21.09: the +5V was split into 6 islands (U1, hence no 3.3 V, U9, IC2, J14, J5) and the DRC only reported it as "unconnected items". After any zone edit, check continuity pad by pad.
- **GND symbol placed directly on a connector pin** (J5.4, J6.4): it carries the pin's ground. Do not delete it even if the wire leaving it dangles.
- **ERC in JSON**: positions in hundredths of mm, and "length 0.0508 mm" means 5.08 mm. The reported sheet can be wrong: the 3 "root sheet wire stubs" were in `untitled.kicad_sch`.
- **Rounded coordinates**: a track "reset" to (109.75; 105.29) instead of its real coordinate was enough to create a 0.001 mm defect 25 mm further on. Never straighten a long track: add a small 45° link.
- **F8 and MP pins**: a symbol with an MP pin on a footprint without an MP pad raises an error on F8. Use `Conn_01x0N` (same pin positions 1 to N).
- **Teardrops**: `kicad-cli pcb drc --refill-zones` also generates them, but does not save them.
- **Starved thermal with a single spoke** (J17/J18, 24.09): when a pad sits next to a narrow corridor of its zone, a custom rule `(constraint min_resolved_spokes 1)` scoped with `A.memberOfFootprint('J17')` accepts it. `A.Parent.Reference` is not accepted and silently invalidates the whole rules file.

### pcbnew scripting (KiCad 10)

- Delete with `board.Delete()`, never `Remove()` (silent crash); duplicate a zone with `Duplicate(False).Cast()`; `GetBoardPolygonOutlines(poly, True)`.
- `GetCourtyard().Contains()` without a layer answers "no" even at the centre of the module — 14 vias ended up under the ESP32 that way. `GetCourtyard(pcbnew.F_CrtYd).Contains()` works when the layer is passed.
- `CONNECTIVITY_DATA.GetConnectedItems(pad)` gives the whole island, `GetConnectedPads` does not.
- `SHAPE_CIRCLE.Collide` only accepts a segment — to test a via, use `shape.Collide(VECTOR2I, radius + clearance)`.
- Teardrops: `SetTeardropsEnabled(True)`; they are generated when the zones are filled.
- For a footprint field, the angle passed to `SetTextAngleDegrees` combines with the footprint's rotation: on J14 (90°), "90°" gives a horizontal text. Compute placement candidates from the real bounding box, with centred justification, and call `SetMirrored(True)` on the back.
- Boolean operations of `SHAPE_POLY_SET` on the zones' filled polygons triggered an overflow in kimath ("Overflow converting value … to int") and a wxWidgets alert window. To inspect a region of the board, render a copy of the board whose outline is cut down to that region instead.
- Work on a copy with KiCad closed, check with the DRC, then copy back — and keep a backup before every step.

---

## 6. Session of 19.09.2026 — PCB

- **H1/H2** deleted by an F8 ("delete footprints with no symbol"), restored at their NPTH.drl coordinates with the "Not in schematic" attribute: F8 no longer removes them.
- **USB**: the ESP32-S3 runs at Full Speed, pair length matching is not critical. A6↔B6 and A7↔B7 must be connected (reversibility); D+ diagonally on F.Cu, D− through two 0.6/0.3 vias via B.Cu.
- **Vias**: 0.6/0.3 preset added; the class's GND vias reduced to 0.6/0.3 (stitching). Differential pair 0.2 / 0.15, via gap 0.25.
- **Minimum drill** 0.3 → 0.2 mm: the thermal vias of IC1's footprint are 0.2 mm, and this global constraint cannot be relaxed by a rule.
- **`Turret2.kicad_dru` revised**: widths aligned with the 18.09 classes (5V and audio min 0.8); GND min 0.2, 3V3 min 0.3; "Power via" limited to 5V/audio vias; Hall rule targeted by net name (the radar UART sits next to the 5 V on J5); In2 as a whitelist; "Fine pitch" rule (0.2 mm) inside USBC1, IC1, U1, U9, D2; dead 5V-peripheral rule removed.
- **J2 (TC2050)**: a keepout forbade its own pads, fixed on the board and in the library.
- **DRC**: no rule errors left. Remaining: D1/H1, J12/J13, J5/J6 (placement).
- **Decided**: J1 stays JST-PH, remote cable in 24 AWG (design-plan §10 adjusted). TP5/TP6 removed, UART0 only on J16.

## 7. Session of 20.09.2026 — +5V star, L3 zones, bulk

Work done directly in `Turret2.kicad_pcb` by pcbnew script, KiCad closed. Full details in design-plan §9.1b.

- **EN/OVLO**: the R4 → IC1.2 link was a *graphic line* on F.Cu (not recognised as a connection), 0.014 mm from the DVDT pad. Replaced by a real 0.2 mm track.
- **+5V star**: IC1.5 → 0.25 mm neck → F.Cu node `5V_etoile` → 3 × 0.9/0.5 vias to `5V_logique` and 4 to `5V_servos`. Single junction point of the two branches. Check: none of the 17 +5V vias touches both planes.
- **L3 (PWR) zones**: `5V_logique` = U1 pocket/node + top-right quarter (949 mm²); `5V_servos` = bottom right (674 mm²); 1 mm gap (raised from 0.5 to 1 mm at the end of the session to leave room for a ground corridor, checked by a dry-run fill); priorities VBUS 1, +5V 2.
- **U1**: one 0.9/0.5 via per VIN pin (4 and 5); pin 5 was not connected.
- **Bulk**: C9 and C18 on the front, at (103.5; 120.5) and (111.5; 120.5), vertical, + at the top. Each terminal: solid copper pour and 4 × 0.9/0.5 vias.
- **Connector layout decided**: the six servos (J3, J4, J7–J10) bottom right, the others top right.
- **L3 GND**: `GND_L3` zone (priority 0) and 6 × 0.6/0.3 stitching vias under the module (Y 108 and 111). The 14 vias first placed under the ESP32 were removed (courtyard test pitfall, §5).
- **U7 exposed pad**: 3 × 3 grid of GND pads joined by a solid F.Cu pour, 4 plugged 0.5/0.2 vias between the pads.
- **+3V3 zone**: fed by two 0.9/0.5 vias at U1 VOUT and at U7's 3V3 pin.

## 8. Backups

*(Local backup list — omitted from the public version.)*

---

## 9. Session of 22.09.2026 — inspection and fixes

Placement and routing were finished by hand; JTAG was removed, UART0 and the buttons came back. Full inspection (ERC, DRC with zone fill, netlist, pad-by-pad continuity), then fixes by pcbnew script on a copy, checked with the DRC, copied back into the project. KiCad closed.

### Inspection

- **I²C properly routed**: IO35/36 → U6 + J11, about 40 mm, 2 to 3 vias, 0.2–0.25 mm; a single pair of 2.2 kΩ 3 mm from U6; J11 in Qwiic order; SA0 to GND (0x6A), CS to 3V3; LGA-14 footprint matches KiCad's. On F.Cu over solid GND, on B.Cu over the 3V3 without crossing a split. SDA ran alongside LRCLK at 0.2 mm over 13 mm: moved apart, checked.
- **The real problem was power**: the `5V_logique` zone was missing (§5) and U6's VDD/VDDIO had no link to 3V3 — U6 would have been parasitically powered through CS and the SDA/SCL pull-ups.
- **Speaker output**: 1.5 mm is more than enough (3.2 W into 4 Ω: 0.9 A RMS, 1.3 A peak; 1.5 mm on an outer layer carries about 3 A). Short pair, 0.3 mm gap, no via.
- **Test points**: never on the board in any backup. Added to the schematic on 22.09, never imported. Nothing had been deleted.

### Fixes

- **`5V_logique` zone (In2) restored** identically from KiCad's automatic backup of 21.09.
- **J10**: schematic footprint back to `JST_PH_B3B-PH-K_1x03_P2.00mm_Vertical` (it had switched to a 2-pin JST-SUR); "Do not populate" removed from J10 and R18 on the board.
- **U6**: VDD/VDDIO tied to 3V3 with a track to the via at (100.25; 106.25); standard 3.5 × 3.0 courtyard on the board and in the library; C14, C17, C27 moved down 0.25 mm (courtyard overlap).
- **J5**: +5V 0.9/0.5 via to `5V_logique` at (129.1; 109.1). **C11/C12**: GND through J13.2. **U7 pins 42/43**: tied to the exposed pad.
- **+5V under U9**: pin exits at 0.3, 0.8 bus from C25 to C24, 0.5/0.4 to C8/IC2.
- **VBUS**: 0.7 bridge from C1 to pins 3–4 of IC1, instead of two 0.25 tracks.
- **OUTP/OUTN**: 0.3 necks limited to about 1 mm inside U9's footprint, then 0.8 and 1.5; GND pin 11 tied to the exposed pad, GND via at (113.5; 97.5) removed to make room.
- **J9/J10**: ROTATE_X and ROTATE_Z bypass J9.2/J10.2 on the right on B.Cu, the thermal reliefs keep their spokes.
- **Clean-up**: orphan PWR_FLT via, 0.03 mm SCL stub, 5 zero-length segments, RADAR_RX neck re-traced; 3 dangling wires on the IO sheet (old MP → GND).
- **Stitching**: 10 × 0.6/0.3 GND vias added (perimeter, GND corridors of In2, B.Cu GND islands with a single link). The 7 GND vias removed during routing cannot come back: their spots are taken by tracks. Very dense board: barely 1 % of the points of a 1 mm grid are free on all 4 layers.
- **Teardrops** enabled (vias, through-hole pads, SMD pads of J11/J16).

DRC: 144 → 118 (108 of them silkscreen); unconnected items 11 → 1; parity 17 → 14.

---

## 10. Session of 23.09.2026 — ground, stencil, F8, last defects

### Ground check

- Path from every GND pad to the In1 plane computed by script: everything is connected; through-hole pads touch the plane directly.
- **ESP32 top row (pins 46–60, 65)**: they had been daisy-chained with a 0.2 mm track to a single via (up to 15.6 mm). Acceptable, since the module ties its grounds internally and the return flows through the centre pad (9 pads, pour, 4 vias). No vias possible under the row: board edge 0.75 mm above, 5 B.Cu tracks below (ROTATE_Z, HALL_LEFT, DIN, HALL_RIGHT, BCLK). Chain widened to 0.3 mm and a second exit from pin 48 to the centre pad: worst path 9.8 mm.
- **U1 (3.3 V)**: C5, C6, C7 each have their GND via at 0.6 mm (C7: via between its pads, under the body); GND links of pins 6/7 at 0.3; C3 has its via; C4 at 0.4 mm (no room for a via).
- The 10 stitching vias of 22.09 are at (69.5; 105.25), (70.5; 101), (106; 91.25), (99.75; 96.75), (93; 98.75), (96.75; 102.75), (96.5; 106), (130; 95.75), (123; 110.25), (129.5; 110.25).

### Stencil and mounting

- 2.2 mm F.Paste discs over H1 and H2 (checked in the paste gerber).
- H3 (1 mm, at the top): could not be enlarged (1.15 mm max, HALL_LEFT underneath); no room for an M2 or M1.6 screw at the top. Removed along with its opening.

### Schematic and parity

- J5, J6 → `Conn_01x04`; J12, J13 → `Conn_01x02`: end of the 4 errors on F8.
- J11 MP tied to GND (GND symbol on the pin, pads and vias set to GND on the board).
- R17: `R_1206` HandSolder footprint. D5 not fitted and R17 fitted, on the board as in the schematic. TP1–TP4 excluded from BOM and placement files in the schematic too.
- TP2: +5V track widened from 0.5 to 0.8 mm.

### Last DRC defects fixed

- HALL_RIGHT / DIN via: HALL_RIGHT bend moved back 0.1 mm (95.21; 93.80).
- RADAR_TX via / NEOPIXEL_CENTER: via at (109.82; 105.36) with a 45° link; NEOPIXEL_CENTER hook lowered to y = 104.67 and widened through x = 110.60.
- C15 against the notch: moved down 1.1 mm (and TP4 by 0.15 mm for the courtyards); moving it right was impossible (courtyard of C16, then of U7).
- GUN_RIGHT via against the edge: moved up to (72.60; 126.35) with a 45° link.

---

## 11. Session of 23.09.2026 (evening) — C15, 0.4 mm tracks and silkscreen

### C15 supply

- C15, C16 and TP4 share a single 0.9/0.5 3V3 via at (73.75; 96.5): correct. Order via → C16 (100 nF, against the pin) → U7 pin 3; C15 (10 µF) 3 mm away, which does not matter for a reservoir capacitor.
- The last 1.8 mm from C16 to pin 3 are now 0.4 mm (the pad width).
- **General move to 0.4 mm** of the thinner GND, +3V3, +5V and VBUS tracks, wherever clearance allows: 97 segments (≈ 95 mm), including all of U7's ground (pins 1, 2, 46–65) and the decoupling of U1/IC2. Deliberately left thinner: pin exits narrower than 0.4 mm (U1, U6, U9, IC1) — a track wider than the pad hampers soldering —, 42 segments blocked by clearance, the link of U7 pins 42/43 under the module (0.25), and the GND chain between C5–C7, back to 0.2 (redundant with their own vias, and at 0.4 it created 0.08 mm copper necks against those vias).
- The two right angles of ROTATE_X/Z (left by the 22.09 script) replaced by 45° segments.

### Silkscreen rules

| Level | What | Height / stroke |
|---|---|---|
| Function | Connector values (GUN_LEFT, ROTATE_X, Speaker, Ring Led, I2C extender, UART0, HLK-LD2450, HALL_L, HALL_R, ButtonA/B…), buttons, test points (VBUS, 5V, 3V3), title "Turret2 v0.1" | 1.0 / 0.15 (0.8 if there is no room) |
| Designator | IC and passive references where there is room | 0.8 / 0.15 |
| Hidden | The 30 designators that were 0.2–0.3 mm high, passive values, designators of connectors, buttons, test points, holes and logo | hidden; visible on F.Fab |

- Placement by script: first free position around the part (above, below, left, right; 0° or 90°), never on a pad, on another part's body (F.Fab/B.Fab outline) or closer than 0.3 mm to the edge. Result checked visually on a kicad-cli render.
- Test point values changed from "TestPoint" to VBUS, 5V, 5V, 3V3, in the schematic and on the board, so that F8 does not revert them.
- Special cases: "USB extender" (J1) placed on the back for lack of room on the front; "USB / JTAG" (SW1) hidden pending the SW1 decision; "Reset" (SW3) and "5V" (TP3) hidden, the nearest free spot being 5–7 mm away.
- DRC: from 140 to 4 warnings, all harmless.

## 12. Session of 24.09.2026 — Hall sensors split

### Rewiring

- J6 (Hall Sensors, 4-pin JST-PH on the back) removed, replaced by **J17 "HALL_L"** on the front at (125.5; 102.95) and **J18 "HALL_R"** on the back at (125.5; 97.8), vertical JST-PH B3B. Pinout: 1 = +3V3, 2 = HALL_LEFT (IO8) / HALL_RIGHT (IO9), 3 = GND. Same connector as the servos: use cables of a different colour.
- C28 removed; **C31** (123.75; 97.5) 1.75 mm from J18.1 and **C32** (126.25; 100) 3 mm from J17.1, 100 nF 3V3/GND.
- Check (netlists and boards compared with the previous version): no errors. No unconnected item; 3V3 at 0.8/0.5 mm, GND at 0.5 mm, one GND via per connector and per capacitor; Hall at 0.25 mm (0.5 mm clearance rule to 5 V respected); 1Y and 2Y rerouted at 0.3 mm.
- Cable mix-up: IO8/IO9 can never receive 5 V (a Hall socket carries no 5 V source, and a servo or LED cable plugged into it brings none). The only risk: a Hall sensor plugged into a servo socket is reverse-powered.

### Harmonisation by script

- **Teardrops** enabled on the 18 new vias and the 6 pads of J17/J18.
- **Thermal relief**: on In2, the GND pads of J17/J18 touch the narrow GND corridor and only get one spoke (2 "starved thermal" errors; 45° spokes tried without success). New rule at the end of `Turret2.kicad_dru`, "Hall: one spoke is enough on In2": `min_resolved_spokes 1` for those pads only. Harmless: the pads are also on the solid In1 plane and have their own GND via through an F.Cu track.
- **Silkscreen**: HALL_L on the front above J17 (0.8 mm, the only free spot; C32's GND via runs under the text); HALL_R on the back below J18 (0.8 mm); "Ring Led" (J14), which overlapped J18's pads, moved to the back, right of J14's pins (0.8 mm, mirrored). Designators of J17, J18, C31, C32 hidden (visible on F.Fab).
- DRC: 0 errors, 0 unconnected, the usual 4 warnings.

## 13. Pre-production review (24.09.2026)

Board saved after zone fill (B); F8 not yet done (13 "Description" fields, no effect on fabrication).

### Checked, compliant

- **DRC** with and without forced refill: 0 errors, 0 unconnected items, the usual 4 warnings. All zones filled, 314 teardrops saved.
- **ERC**: only the known warnings (LCSC symbols outside a library, IO39–42 of J2, U9 pad).
- **Antenna**: fully overhanging above the notched edge, no copper under or around it on all 4 layers (checked on a render).
- **Fabrication**: 4 layers, 1.6 mm, 77.4 × 40.7 mm. Tracks ≥ 0.2 mm, clearance ≥ 0.15 mm. Vias: 23 × 0.45/0.2, 4 × 0.5/0.2, 127 × 0.6/0.3, 68 × 0.9/0.5, all tented. PTH drills 0.2 to 1.2 mm, NPTH 0.52 / 0.89 (USB-C) and 2.2 (H1/H2). Within JLCPCB's 4-layer capabilities (minimum via 0.25/0.15).
- **Export** with kicad-cli: Gerbers for the 4 copper layers, masks, pastes, silkscreens, outline; separate PTH/NPTH drill files and drill map — no errors.
- **Silkscreen**: no visible text below 0.8 mm height or 0.15 mm stroke. "USBC1" designator hidden on 24.09 (it read like D5's label); kept on F.Fab.
- **BOM**: D5 "do not populate"; TP1–TP4, H1, H2 and the logo excluded from BOM and placement; J2 excluded from BOM.

### Still to decide before ordering

See "Before ordering" at the top of this file.
