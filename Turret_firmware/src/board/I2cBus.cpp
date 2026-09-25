#include "board/Log.h"
#include "I2cBus.h"

#include "pins.h"
#include <Wire.h>

namespace {
const uint32_t I2C_FREQUENCY = 400000;

// After a warm reset in the middle of a transaction, a slave (the IMU) can
// hold SDA low forever. Up to 9 clock pulses let it finish its byte, then a
// STOP condition releases the bus. Both lines have 2.2k pull-ups on the board.
void RecoverBus() {
  pinMode(PIN_SDA, INPUT);
  pinMode(PIN_SCL, INPUT);
  if (digitalRead(PIN_SDA) == HIGH) {
    return;
  }
  Log.println("I2C: SDA stuck low, clocking the bus free");
  pinMode(PIN_SCL, OUTPUT_OPEN_DRAIN);
  for (int i = 0; i < 9 && digitalRead(PIN_SDA) == LOW; i++) {
    digitalWrite(PIN_SCL, LOW);
    delayMicroseconds(5);
    digitalWrite(PIN_SCL, HIGH);
    delayMicroseconds(5);
  }
  // STOP: SDA low -> high while SCL is high.
  pinMode(PIN_SDA, OUTPUT_OPEN_DRAIN);
  digitalWrite(PIN_SDA, LOW);
  delayMicroseconds(5);
  digitalWrite(PIN_SCL, HIGH);
  delayMicroseconds(5);
  digitalWrite(PIN_SDA, HIGH);
  delayMicroseconds(5);
  pinMode(PIN_SDA, INPUT);
  pinMode(PIN_SCL, INPUT);
  if (digitalRead(PIN_SDA) == LOW) {
    Log.println("I2C: SDA still low after recovery");
  }
}
} // namespace

namespace I2cBus {

bool Begin() {
  RecoverBus();
  // Explicit pins, always (Turret2: SDA IO35, SCL IO36).
  if (!Wire.begin(PIN_SDA, PIN_SCL, I2C_FREQUENCY)) {
    Log.println("I2C: Wire.begin failed");
    return false;
  }
  Scan();
  return true;
}

bool Probe(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

uint8_t Scan() {
  uint8_t found = 0;
  Log.print("I2C scan:");
  for (uint8_t address = 0x08; address < 0x78; address++) {
    if (Probe(address)) {
      Log.printf(" 0x%02X", address);
      found++;
    }
  }
  Log.println(found ? "" : " no device");
  return found;
}

} // namespace I2cBus
