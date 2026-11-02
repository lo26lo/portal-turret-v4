#ifndef Pins_Arduino_h
#define Pins_Arduino_h

// Turret2 v2 board: ESP32-S3-MINI-1-N8 (8 MB quad flash, no PSRAM).
// Same as variants/turret2 (v0.1), with the defaults moved for the v2 pinout
// (docs/carte-v2-etat.md §4):
//  - SDA/SCL on IO35/IO36, as on the v0.1;
//  - LED_BUILTIN on the green status LED, IO13 (IO33 is the left gun servo);
//  - default SPI pins on IO3-IO6, which are not connected on the v2 (test
//    points); IO39-IO42 are I2S and amplifier pins there. Nothing uses SPI.
// The full pinout is in src/pins.h.

#include <stdint.h>
#include "soc/soc_caps.h"

#define USB_VID 0x303a
#define USB_PID 0x1001

static const uint8_t LED_BUILTIN = 13;
#define BUILTIN_LED  LED_BUILTIN // backward compatibility
#define LED_BUILTIN LED_BUILTIN  // allow testing #ifdef LED_BUILTIN

static const uint8_t TX = 43;
static const uint8_t RX = 44;

static const uint8_t SDA = 35;
static const uint8_t SCL = 36;

static const uint8_t SS    = 3;
static const uint8_t MOSI  = 4;
static const uint8_t MISO  = 5;
static const uint8_t SCK   = 6;

static const uint8_t A0 = 1;
static const uint8_t A1 = 2;
static const uint8_t A2 = 3;
static const uint8_t A3 = 4;
static const uint8_t A4 = 5;
static const uint8_t A5 = 6;
static const uint8_t A6 = 7;
static const uint8_t A7 = 8;
static const uint8_t A8 = 9;
static const uint8_t A9 = 10;
static const uint8_t A10 = 11;
static const uint8_t A11 = 12;
static const uint8_t A12 = 13;
static const uint8_t A13 = 14;
static const uint8_t A14 = 15;
static const uint8_t A15 = 16;
static const uint8_t A16 = 17;
static const uint8_t A17 = 18;
static const uint8_t A18 = 19;
static const uint8_t A19 = 20;

static const uint8_t T1 = 1;
static const uint8_t T2 = 2;
static const uint8_t T3 = 3;
static const uint8_t T4 = 4;
static const uint8_t T5 = 5;
static const uint8_t T6 = 6;
static const uint8_t T7 = 7;
static const uint8_t T8 = 8;
static const uint8_t T9 = 9;
static const uint8_t T10 = 10;
static const uint8_t T11 = 11;
static const uint8_t T12 = 12;
static const uint8_t T13 = 13;
static const uint8_t T14 = 14;

#endif /* Pins_Arduino_h */
