#pragma once

// Turret2 pinout (ESP32-S3-MINI-1-N8). The boards are wired to these pins: never reassign them.
// Two boards, chosen at build time (platformio.ini):
//  - v0.1 (env turret2*): docs/firmware-plan.md §1.2;
//  - v2   (env turret2_v2*, TURRET2_V2 defined): docs/carte-v2-etat.md §4.
// Each build checks at boot that it runs on its own board (board/BoardCheck).
// Never use IO0 as an output, IO19 / IO20 (USB), IO45, IO46 (straps).
// IO0 is only ever read as an input, after the boot: PIN_PROG below.

#define PIN_ABSENT 0xFF // function not wired on this board

// Same on both boards.
#define PIN_SDA 35       // I2C_SDA: always Wire.begin(PIN_SDA, PIN_SCL)
#define PIN_SCL 36       // I2C_SCL
#define PIN_HALL_LEFT 8  // ADC
#define PIN_HALL_RIGHT 9 // ADC
#define PIN_PROG 0      // SW2 "PROG" / BOOT to GND: boot strap; INPUT ONLY, read after the boot (factory reset gesture)
#define PIN_UART0_TX 43 // J16, backup console
#define PIN_UART0_RX 44 // J16, backup console

// TCAL6416 rail expander of the v2 (U13). Also the board check: nothing
// answers at this address on a v0.1.
#define EXPANDER_ADDRESS 0x20

#if defined(TURRET2_V2)

#define BOARD_NAME "Turret2 v2"
#define BOARD_HAS_EXPANDER 1

#define PIN_GUN_LEFT 33        // PWM, R28 330R -> J3
#define PIN_GUN_RIGHT 48       // PWM, R29 330R -> J4
#define PIN_WING_LEFT 26       // PWM, R30 330R -> J7; R34 10k to GND against the internal pull-up at reset
#define PIN_WING_RIGHT 34      // PWM, R31 330R -> J8
#define PIN_ROTATE_X 15        // PWM, R33 330R -> J10
#define PIN_ROTATE_Z 16        // PWM, R32 330R -> J9
#define PIN_NEOPIXEL_CENTER 12 // AHCT 3A (100k to GND) -> J14 ring
#define PIN_NEOPIXEL_LEFT 17   // AHCT 1A (100k to GND) -> J3
#define PIN_NEOPIXEL_RIGHT 18  // AHCT 2A (100k to GND) -> J4
#define PIN_RADAR_RX 11        // UART_RX (radar -> ESP32), R43 1k
#define PIN_RADAR_TX 10        // UART_TX (ESP32 -> radar), R42 1k: input before the radar rail is cut
#define PIN_DIN 39             // I2S data
#define PIN_BCLK 38            // I2S bit clock
#define PIN_LRCLK 37           // I2S word clock

#define PIN_AMP_SD 42           // MAX98357A SD_MODE via R10 2k: low = shutdown, high = left channel
#define PIN_AMP_GAIN 40         // OUTPUT_OPEN_DRAIN only, NEVER drive high (low = 12 dB)
#define PIN_AMP_GAIN_100K 41    // via R11 100k: OUTPUT_OPEN_DRAIN only, NEVER drive high (low = 15 dB)
#define PIN_PWR_FLT 21          // eFuse fault, input only, R6 10k pull-up, low = fault
#define PIN_LED_GREEN 13        // status LED (R18 100R), high = on
#define PIN_LED_RED 47          // status LED (R12 680R), high = on
#define PIN_BUTTON_A PIN_ABSENT // buttons removed on the v2 (knob, PROG and web page instead)
#define PIN_BUTTON_B PIN_ABSENT
#define PIN_BENCH_MODE 7 // SW1 "DEBUG" to GND, R9 10k pull-up, closed = debug mode (D1)
#define PIN_IMU_INT1 14  // LSM6DSOX INT1, RTC pin (EXT1 wake-up); never an output, never pulled up
// J20 remote button (added 02.11.2026), ACTIVE HIGH: the button joins +3V3
// (R50 1k) to IO2 (R51 1k), R49 100k pulls IO2 down. Active high so that it
// shares the EXT1 "any high" deep-sleep wake-up with the IMU (EXT0 is PROG).
#define PIN_BUTTON_EXT 2
// Not connected (test points TP10, TP6-TP9): inputs with pull-down. IO3 is the JTAG strap.
#define PINS_UNUSED {1, 3, 4, 5, 6}

// Expander ports (P00..P07 = bits 0..7 of port 0). Every rail is off at
// reset: 100k pull-downs on the ENs, 100k pull-up on the gate of the Hall P-MOS.
#define EXPANDER_SERVO_EN 0  // P00, TPS22965 U10 -> 5V_SERVO (J3, J4, J7-J10)
#define EXPANDER_LED_EN 1    // P01, TPS22965 U11 -> 5V_LED (J14)
#define EXPANDER_RADAR_EN 2  // P02, TPS22965 U12 -> 5V_RADAR (J5)
#define EXPANDER_HALL_EN_N 3 // P03, AO3401A Q1 gate -> 3V3_HALL (J17, J18), ACTIVE LOW

#else // Turret2 v0.1

#define BOARD_NAME "Turret2 v0.1"
#define BOARD_HAS_EXPANDER 0

#define PIN_GUN_LEFT 1         // PWM
#define PIN_GUN_RIGHT 2        // PWM
#define PIN_WING_LEFT 4        // PWM
#define PIN_WING_RIGHT 5       // PWM
#define PIN_ROTATE_X 6         // PWM
#define PIN_ROTATE_Z 7         // PWM
#define PIN_NEOPIXEL_CENTER 14 // PWM
#define PIN_NEOPIXEL_LEFT 15   // PWM
#define PIN_NEOPIXEL_RIGHT 16  // PWM
#define PIN_RADAR_RX 17        // UART_RX
#define PIN_RADAR_TX 18        // UART_TX
#define PIN_DIN 10             // Custom
#define PIN_BCLK 11            // Custom
#define PIN_LRCLK 12           // Custom

// Turret2 additions
#define PIN_AMP_SD 13          // MAX98357A SD_MODE via 2k: low = shutdown, high = left channel
#define PIN_AMP_GAIN 21        // OUTPUT_OPEN_DRAIN only, NEVER drive high (low = 12 dB)
#define PIN_AMP_GAIN_100K 47   // OUTPUT_OPEN_DRAIN only, NEVER drive high (low = 15 dB)
#define PIN_PWR_FLT 38         // eFuse fault, input only, external pull-up, low = fault
#define PIN_LED_GREEN 33       // status LED, high = on
#define PIN_LED_RED 48         // status LED, high = on
#define PIN_BUTTON_A 26        // J12, external pull-up, active low
#define PIN_BUTTON_B 34        // J13, external pull-up, active low
#define PIN_BENCH_MODE 3 // SW1 to GND, pull-up R9, closed = bench mode (D1); JTAG strap, never burn STRAP_JTAG_SEL
#define PIN_IMU_INT1 37  // LSM6DSOX INT1
#define PIN_BUTTON_EXT PIN_ABSENT // v2 only

#endif

#define BOARD_HAS_BUTTONS (PIN_BUTTON_A != PIN_ABSENT)
