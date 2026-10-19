#include "RotaryEncoder.h"

#include "board/Log.h"
#include <Wire.h>

namespace {
// Seesaw registers, from Adafruit_seesaw.h (github.com/adafruit/Adafruit_Seesaw).
const uint8_t STATUS_BASE = 0x00;
const uint8_t STATUS_HW_ID = 0x01;
const uint8_t STATUS_VERSION = 0x02; // product id in the upper 16 bits

const uint8_t GPIO_BASE = 0x01;
const uint8_t GPIO_DIRCLR_BULK = 0x03;
const uint8_t GPIO_BULK = 0x04;
const uint8_t GPIO_BULK_SET = 0x05;
const uint8_t GPIO_PULLENSET = 0x0B;

const uint8_t ENCODER_BASE = 0x11;
const uint8_t ENCODER_DELTA = 0x40; // + encoder number; turns since the last read (cleared by the read)

const uint16_t READ_DELAY_US = 250; // the seesaw needs time between register and data

// Adafruit 4991: switch on seesaw pin 24. Adafruit 5752: knob 0 switch on pin 12
// (pins 14, 17, 9 for knobs 1 to 3, PID5752_QuadEncoder_demo.ino).
const uint8_t ADDRESS_4991 = 0x36;
const uint8_t ADDRESS_5752 = 0x49;
const uint8_t BUTTON_PIN_4991 = 24;
const uint8_t BUTTON_PIN_5752 = 12;

// Soldered 333188 (Soldered-Rotary-Encoder-With-easyC-Arduino-Library): one
// read of 5 bytes = counter (int32, little endian) + last event, cleared by
// the read. Writing 170 resets the counter.
const uint8_t ADDRESS_SOLDERED = 0x30;
const uint8_t SOLDERED_CLICK = 1;
const uint8_t SOLDERED_DOUBLE_CLICK = 2;
const uint8_t SOLDERED_LONG_PRESS = 3;
// Counter steps per detent and direction: to check on the first board
// (EC11E encoder, 15 pulses / 30 detents).
const int32_t SOLDERED_COUNTS_PER_STEP = 1;

const ulong POLL_MS = 30;
const uint8_t MAX_FAILURES = 10; // in a row: unplugged, stop polling

uint32_t BigEndian32(const uint8_t *b) {
  return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) | ((uint32_t)b[2] << 8) | b[3];
}

bool IsSeesawChip(uint8_t id) {
  // SAMD09, ATtiny806/807/816/817/1616/1617
  return id == 0x55 || (id >= 0x84 && id <= 0x89);
}

bool Probe(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}
} // namespace

const char *RotaryEncoder::ModelName() const {
  switch (model) {
  case Model::Adafruit4991: return "Adafruit 4991";
  case Model::AdafruitQuad5752: return "Adafruit 5752 (knob 0)";
  case Model::Soldered: return "Soldered 333188";
  default: return "none";
  }
}

// ------------------------------------------------------------------ seesaw

bool RotaryEncoder::SeesawWrite(uint8_t module, uint8_t reg, const uint8_t *data, uint8_t length) {
  Wire.beginTransmission(address);
  Wire.write(module);
  Wire.write(reg);
  Wire.write(data, length);
  return Wire.endTransmission() == 0;
}

// Two transactions: the bus is free between them (IMU, OLED task).
bool RotaryEncoder::SeesawRead(uint8_t module, uint8_t reg, uint8_t *data, uint8_t length) {
  Wire.beginTransmission(address);
  Wire.write(module);
  Wire.write(reg);
  if (Wire.endTransmission() != 0) {
    return false;
  }
  delayMicroseconds(READ_DELAY_US);
  if (Wire.requestFrom(address, length) != length) {
    return false;
  }
  for (uint8_t i = 0; i < length; i++) {
    data[i] = Wire.read();
  }
  return true;
}

bool RotaryEncoder::BeginSeesaw(uint8_t addressIn, uint16_t product, uint8_t buttonPin) {
  if (!Probe(addressIn)) {
    return false; // nothing there: no message, the I2C scan already shows it
  }
  address = addressIn;
  uint8_t id = 0;
  uint8_t version[4];
  if (!SeesawRead(STATUS_BASE, STATUS_HW_ID, &id, 1) || !IsSeesawChip(id) ||
      !SeesawRead(STATUS_BASE, STATUS_VERSION, version, 4)) {
    Log.printf("Encoder: device at 0x%02X is not a seesaw (id 0x%02X), ignored\n", address, id);
    return false;
  }
  uint16_t found = BigEndian32(version) >> 16;
  if (found != product) {
    Log.printf("Encoder: seesaw product %u at 0x%02X, expected %u, ignored\n", found, address, product);
    return false;
  }
  // Push switch: input with pull-up.
  buttonMask = 1UL << buttonPin;
  const uint8_t mask[4] = {(uint8_t)(buttonMask >> 24), (uint8_t)(buttonMask >> 16), (uint8_t)(buttonMask >> 8),
                           (uint8_t)buttonMask};
  if (!SeesawWrite(GPIO_BASE, GPIO_DIRCLR_BULK, mask, 4) || !SeesawWrite(GPIO_BASE, GPIO_PULLENSET, mask, 4) ||
      !SeesawWrite(GPIO_BASE, GPIO_BULK_SET, mask, 4)) {
    Log.printf("Encoder: could not set up the push button at 0x%02X, ignored\n", address);
    return false;
  }
  // Turns made before the boot are forgotten; a button held now is not a press.
  uint8_t data[4];
  SeesawRead(ENCODER_BASE, ENCODER_DELTA, data, 4);
  bool down = SeesawRead(GPIO_BASE, GPIO_BULK, data, 4) && (BigEndian32(data) & buttonMask) == 0;
  button.Reset(down, millis());
  return true;
}

bool RotaryEncoder::UpdateSeesaw(ulong now) {
  uint8_t data[4];
  if (!SeesawRead(ENCODER_BASE, ENCODER_DELTA, data, 4)) {
    return false;
  }
  // The seesaw counts anticlockwise turns as positive (Adafruit's examples negate it).
  steps -= (int32_t)BigEndian32(data);
  if (!SeesawRead(GPIO_BASE, GPIO_BULK, data, 4)) {
    return false;
  }
  Push(button.Update((BigEndian32(data) & buttonMask) == 0, now));
  return true;
}

// ------------------------------------------------------------------ Soldered

bool RotaryEncoder::BeginSoldered() {
  if (!Probe(ADDRESS_SOLDERED)) {
    return false;
  }
  // No identification register: a device at 0x30 that returns 5 bytes is taken for it.
  address = ADDRESS_SOLDERED;
  uint8_t data[5];
  if (Wire.requestFrom(address, (uint8_t)5) != 5) {
    Log.println("Encoder: device at 0x30 does not answer like a Soldered encoder, ignored");
    return false;
  }
  for (uint8_t i = 0; i < 5; i++) {
    data[i] = Wire.read();
  }
  // Counter kept as it is (no reset write): only differences matter.
  lastCount = (int32_t)((uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) |
                        ((uint32_t)data[3] << 24));
  return true;
}

bool RotaryEncoder::UpdateSoldered() {
  if (Wire.requestFrom(address, (uint8_t)5) != 5) {
    return false;
  }
  uint8_t data[5];
  for (uint8_t i = 0; i < 5; i++) {
    data[i] = Wire.read();
  }
  int32_t count = (int32_t)((uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) |
                            ((uint32_t)data[3] << 24));
  int32_t moved = count - lastCount;
  // Whole detents only; the remainder waits for the next poll.
  steps += moved / SOLDERED_COUNTS_PER_STEP;
  lastCount += (moved / SOLDERED_COUNTS_PER_STEP) * SOLDERED_COUNTS_PER_STEP;
  switch (data[4]) {
  case SOLDERED_CLICK:
    Push(logic::PressEvent::ShortPress);
    break;
  case SOLDERED_DOUBLE_CLICK: // two clicks
    Push(logic::PressEvent::ShortPress);
    Push(logic::PressEvent::ShortPress);
    break;
  case SOLDERED_LONG_PRESS:
    Push(logic::PressEvent::LongPress);
    break;
  default: // idle, turns (the counter covers them), long press release
    break;
  }
  return true;
}

// ------------------------------------------------------------------ common

bool RotaryEncoder::Begin() {
  model = Model::None;
  if (BeginSeesaw(ADDRESS_4991, 4991, BUTTON_PIN_4991)) {
    model = Model::Adafruit4991;
  } else if (BeginSeesaw(ADDRESS_5752, 5752, BUTTON_PIN_5752)) {
    model = Model::AdafruitQuad5752;
  } else if (BeginSoldered()) {
    model = Model::Soldered;
  } else {
    return false;
  }
  Log.printf("Encoder: %s at 0x%02X\n", ModelName(), address);
  return true;
}

void RotaryEncoder::Update(ulong now) {
  if (model == Model::None || now - lastPoll < POLL_MS) {
    return;
  }
  lastPoll = now;
  bool ok = model == Model::Soldered ? UpdateSoldered() : UpdateSeesaw(now);
  if (ok) {
    failures = 0;
  } else if (++failures >= MAX_FAILURES) {
    Log.printf("Encoder: %s no longer answers (unplugged?), polling stopped\n", ModelName());
    model = Model::None;
  }
}

void RotaryEncoder::Push(logic::PressEvent event) {
  if (event == logic::PressEvent::None) {
    return;
  }
  // Two slots: a double click of the Soldered board gives two presses.
  if (pending[0] == logic::PressEvent::None) {
    pending[0] = event;
  } else {
    pending[1] = event;
  }
}

int32_t RotaryEncoder::TakeSteps() {
  int32_t taken = steps;
  steps = 0;
  return taken;
}

logic::PressEvent RotaryEncoder::TakeEvent() {
  logic::PressEvent event = pending[0];
  pending[0] = pending[1];
  pending[1] = logic::PressEvent::None;
  return event;
}
