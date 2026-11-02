#include "Expander.h"

#include "pins.h"
#include <Wire.h>

namespace {
const uint8_t REG_INPUT0 = 0x00;
const uint8_t REG_OUTPUT0 = 0x02;
const uint8_t REG_CONFIG0 = 0x06;

// P00..P03 drive the rails, P04..P07 stay inputs (1 = input).
const uint8_t CONFIG0_RAILS = 0xF0;
// All off: SERVO_EN, LED_EN, RADAR_EN (P00..P02) low, HALL_EN_N (P03) high.
// The free ports keep their reset value 1 (inputs: the register does not
// reach them). Also used by a v0.1 build that finds a v2 board (BoardCheck).
const uint8_t OUTPUT0_ALL_OFF = 0xF8;
#if BOARD_HAS_EXPANDER
static_assert(EXPANDER_SERVO_EN == 0 && EXPANDER_LED_EN == 1 && EXPANDER_RADAR_EN == 2 && EXPANDER_HALL_EN_N == 3,
              "OUTPUT0_ALL_OFF assumes the rails on P00..P03");
#endif

bool WriteRegister(uint8_t reg, uint8_t value) {
  Wire.beginTransmission(EXPANDER_ADDRESS);
  Wire.write(reg);
  Wire.write(value);
  return Wire.endTransmission() == 0;
}

bool ReadRegisters(uint8_t reg, uint8_t *data, uint8_t length) {
  Wire.beginTransmission(EXPANDER_ADDRESS);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) {
    return false;
  }
  if (Wire.requestFrom((uint8_t)EXPANDER_ADDRESS, length) != length) {
    return false;
  }
  for (uint8_t i = 0; i < length; i++) {
    data[i] = Wire.read();
  }
  return true;
}
} // namespace

namespace Expander {

bool Probe() {
  uint8_t config[2];
  return ReadRegisters(REG_CONFIG0, config, 2);
}

bool AllOff() {
  if (!WriteRegister(REG_OUTPUT0, OUTPUT0_ALL_OFF) || !WriteRegister(REG_CONFIG0, CONFIG0_RAILS)) {
    return false;
  }
  uint8_t output = 0, config = 0, input = 0;
  return ReadPort0(output, config, input) && output == OUTPUT0_ALL_OFF && (config & 0x0F) == 0;
}

bool SetPort(uint8_t port, bool high) {
  uint8_t output = 0;
  if (port > 7 || !ReadRegisters(REG_OUTPUT0, &output, 1)) {
    return false;
  }
  uint8_t wanted = high ? (output | (1u << port)) : (output & ~(1u << port));
  if (!WriteRegister(REG_OUTPUT0, wanted)) {
    return false;
  }
  uint8_t check = 0;
  return ReadRegisters(REG_OUTPUT0, &check, 1) && check == wanted;
}

bool ReadPort0(uint8_t &output, uint8_t &config, uint8_t &input) {
  return ReadRegisters(REG_OUTPUT0, &output, 1) && ReadRegisters(REG_CONFIG0, &config, 1) &&
         ReadRegisters(REG_INPUT0, &input, 1);
}

} // namespace Expander
