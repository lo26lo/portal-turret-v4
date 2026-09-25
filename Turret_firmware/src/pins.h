#pragma once

// Turret2 pinout (ESP32-S3-MINI-1-N8). The board is wired to these pins: never reassign them.
// Reference: docs/firmware-plan.md §1.2. Never use IO0, IO19, IO20, IO45, IO46.

#define PIN_GUN_LEFT 1  // PWM
#define PIN_GUN_RIGHT 2  // PWM
#define PIN_WING_LEFT 4  // PWM
#define PIN_WING_RIGHT 5  // PWM
#define PIN_ROTATE_X 6  // PWM
#define PIN_ROTATE_Z 7  // PWM
#define PIN_HALL_LEFT 8  // ADC
#define PIN_HALL_RIGHT 9  // ADC
#define PIN_NEOPIXEL_CENTER 14  // PWM
#define PIN_NEOPIXEL_LEFT 15  // PWM
#define PIN_NEOPIXEL_RIGHT 16  // PWM
#define PIN_RADAR_RX 17  // UART_RX
#define PIN_RADAR_TX 18  // UART_TX
#define PIN_SDA 35  // I2C_SDA
#define PIN_SCL 36  // I2C_SCL
#define PIN_DIN 10  // Custom
#define PIN_BCLK 11  // Custom
#define PIN_LRCLK 12  // Custom

// Turret2 additions
#define PIN_AMP_SD 13  // MAX98357A SD_MODE via 2k: low = shutdown, high = left channel
#define PIN_AMP_GAIN 21  // OUTPUT_OPEN_DRAIN only, NEVER drive high (low = 12 dB)
#define PIN_AMP_GAIN_100K 47  // OUTPUT_OPEN_DRAIN only, NEVER drive high (low = 15 dB)
#define PIN_PWR_FLT 38  // eFuse fault, input only, external pull-up, low = fault
#define PIN_LED_GREEN 33  // status LED, high = on
#define PIN_LED_RED 48  // status LED, high = on
#define PIN_BUTTON_A 26  // J12, external pull-up, active low
#define PIN_BUTTON_B 34  // J13, external pull-up, active low
#define PIN_BENCH_MODE 3  // SW1 to GND, pull-up R9, closed = bench mode (D1); JTAG strap, never burn STRAP_JTAG_SEL
#define PIN_IMU_INT1 37  // LSM6DSOX INT1
#define PIN_UART0_TX 43  // J16, backup console
#define PIN_UART0_RX 44  // J16, backup console
