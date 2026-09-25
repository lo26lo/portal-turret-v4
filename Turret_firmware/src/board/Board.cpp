#include "board/Log.h"
#include "Board.h"

#include "pins.h"
#include <esp_system.h>

namespace {
const char *NVS_NAMESPACE = "board";
const char *KEY_BOOT_LOOP = "bootLoop";  // consecutive suspicious boots (D5)
const char *KEY_BROWNOUTS = "brownouts"; // lifetime brownout resets

const ulong DEBOUNCE_MS = 30;
const ulong LONG_PRESS_MS = 3000;       // D3: B held 3 s = WiFi on / off
const ulong FACTORY_RESET_HOLD_MS = 1000; // A + B must stay held this long at boot
const ulong STABLE_RUN_MS = 60000;      // D5: counter cleared after 60 s of uptime
const uint32_t REBOOT_LOOP_LIMIT = 3;   // D5: from 3 suspicious boots in a row

const ulong HEARTBEAT_PERIOD_MS = 1000;
const ulong HEARTBEAT_ON_MS = 80;
const ulong RED_BLINK_MS = 250; // on and off time of one blink
const ulong RED_PAUSE_MS = 1500; // pause between two repetitions of the code

volatile uint32_t powerFaultEdges = 0;

void IRAM_ATTR OnPowerFault() { powerFaultEdges++; }

// Resets that may come from the power supply or a crash. EXT/SW/DEEPSLEEP are
// deliberate. On the ESP32-S3 the EN pin (RESET button SW3) reports POWERON,
// so pressing RESET three times within a minute also enters reduced mode.
bool IsSuspicious(esp_reset_reason_t reason) {
  switch (reason) {
  case ESP_RST_POWERON:
  case ESP_RST_BROWNOUT:
  case ESP_RST_PANIC:
  case ESP_RST_INT_WDT:
  case ESP_RST_TASK_WDT:
  case ESP_RST_WDT:
  case ESP_RST_UNKNOWN:
    return true;
  default:
    return false;
  }
}
} // namespace

// ---------------------------------------------------------------- Button

void Button::Initialize() {
  // External 10k pull-up + 100 nF on the board, active low.
  pinMode(pin, INPUT);
  rawDown = stableDown = digitalRead(pin) == LOW;
  rawChangedAt = downSince = millis();
  // A button already held at boot is not an event (A + B is read by Board::Begin).
  longSent = stableDown;
}

void Button::Update(ulong now) {
  bool down = digitalRead(pin) == LOW;
  if (down != rawDown) {
    rawDown = down;
    rawChangedAt = now;
  }
  if (rawDown != stableDown && now - rawChangedAt >= DEBOUNCE_MS) {
    stableDown = rawDown;
    if (stableDown) {
      downSince = now;
      longSent = false;
    } else if (!longSent) {
      pending = ButtonEvent::ShortPress;
    }
  }
  if (stableDown && !longSent && now - downSince >= LONG_PRESS_MS) {
    longSent = true;
    pending = ButtonEvent::LongPress;
  }
}

ButtonEvent Button::TakeEvent() {
  ButtonEvent event = pending;
  pending = ButtonEvent::None;
  return event;
}

// ---------------------------------------------------------------- Board

Board::Board() : buttonA(PIN_BUTTON_A), buttonB(PIN_BUTTON_B) {}

void Board::Begin() {
  // 1. Amplifier muted (SD low), then gain pins released: open drain, '1' = high
  //    impedance = 9 dB. IO21 / IO47 must never be driven high (plan §2.2).
  digitalWrite(PIN_AMP_SD, LOW);
  pinMode(PIN_AMP_SD, OUTPUT);
  digitalWrite(PIN_AMP_SD, LOW);
  pinMode(PIN_AMP_GAIN, OUTPUT_OPEN_DRAIN);
  digitalWrite(PIN_AMP_GAIN, HIGH);
  pinMode(PIN_AMP_GAIN_100K, OUTPUT_OPEN_DRAIN);
  digitalWrite(PIN_AMP_GAIN_100K, HIGH);

  // 2. Both LEDs on during boot (D4, also a LED test).
  pinMode(PIN_LED_GREEN, OUTPUT);
  digitalWrite(PIN_LED_GREEN, HIGH);
  pinMode(PIN_LED_RED, OUTPUT);
  digitalWrite(PIN_LED_RED, HIGH);

  // 3. Inputs. PWR_FLT has an external 10k pull-up (open drain of the eFuse).
  //    SW1 has R9 as pull-up; the internal one covers R9 not being fitted.
  pinMode(PIN_PWR_FLT, INPUT);
  pinMode(PIN_BENCH_MODE, INPUT_PULLUP);
  buttonA.Initialize();
  buttonB.Initialize();
  attachInterrupt(digitalPinToInterrupt(PIN_PWR_FLT), OnPowerFault, FALLING);

  // 4. Boot-time choices, read once.
  benchMode = digitalRead(PIN_BENCH_MODE) == LOW;
  if (buttonA.IsDown() && buttonB.IsDown()) {
    ulong start = millis();
    factoryReset = true;
    while (millis() - start < FACTORY_RESET_HOLD_MS) {
      if (digitalRead(PIN_BUTTON_A) != LOW || digitalRead(PIN_BUTTON_B) != LOW) {
        factoryReset = false;
        break;
      }
      delay(10);
    }
  }

  // 5. Reset reason and counters (D5).
  resetReason = esp_reset_reason();
  prefsReady = prefs.begin(NVS_NAMESPACE, false);
  if (prefsReady) {
    bootLoopCount = prefs.getUInt(KEY_BOOT_LOOP, 0);
    brownoutCount = prefs.getUInt(KEY_BROWNOUTS, 0);
    if (IsSuspicious(resetReason)) {
      bootLoopCount++;
      prefs.putUInt(KEY_BOOT_LOOP, bootLoopCount);
    }
    if (resetReason == ESP_RST_BROWNOUT) {
      brownoutCount++;
      prefs.putUInt(KEY_BROWNOUTS, brownoutCount);
    }
  }
  reducedMode = bootLoopCount >= REBOOT_LOOP_LIMIT;
  if (reducedMode || resetReason == ESP_RST_BROWNOUT) {
    SetFault(Fault::BrownoutLoop, true);
  }
  if (IsPowerFault()) {
    SetFault(Fault::EFuse, true);
  }
}

void Board::PrintBanner() {
  Log.println("---- Turret2 ----");
  Log.printf("Build: %s %s\n", __DATE__, __TIME__);
  Log.printf("Reset reason: %s\n", ResetReasonName(resetReason));
  Log.printf("Suspicious boots in a row: %u, brownouts: %u%s\n",
                (unsigned)bootLoopCount, (unsigned)brownoutCount,
                prefsReady ? "" : " (NVS unavailable)");
  if (reducedMode) {
    Log.println("REDUCED MODE: reboot loop detected (servos stay detached)");
  }
  Log.printf("SW1 bench mode: %s\n", benchMode ? "ON" : "off");
  Log.printf("PWR_FLT: %s\n", IsPowerFault() ? "LOW (fault)" : "high (ok)");
  if (factoryReset) {
    Log.println("A + B held at boot: factory reset requested");
  }

  uint32_t flash = ESP.getFlashChipSize();
  uint32_t psram = ESP.getPsramSize();
  Log.printf("Flash: %u MB, PSRAM: %u bytes\n", (unsigned)(flash >> 20), (unsigned)psram);
  // Plan §2.1: the module must be an N8 (8 MB, no PSRAM; IO26 is button A).
  if (flash != 8u * 1024 * 1024 || psram != 0) {
    Log.println("WARNING: expected ESP32-S3-MINI-1-N8 (8 MB flash, no PSRAM)");
  }
}

void Board::BootDone() {
  bootDone = true;
  digitalWrite(PIN_LED_GREEN, LOW);
  digitalWrite(PIN_LED_RED, LOW);
  redCode = 0;
  redStep = 0;
}

void Board::Update(ulong deltaTime) {
  ulong now = millis();
  buttonA.Update(now);
  buttonB.Update(now);

  uint32_t edges = TakePowerFaultEdges();
  if (edges > 0) {
    powerFaultCount += edges;
    powerFaultEvent = true;
  }
  if (edges > 0 || IsPowerFault()) {
    SetFault(Fault::EFuse, true);
  }

  UpdateBootLoopCounter(now);
  if (bootDone) {
    UpdateGreenLed(now);
    UpdateRedLed(now);
  }
}

void Board::UpdateBootLoopCounter(ulong now) {
  if (bootLoopCleared || now < STABLE_RUN_MS) {
    return;
  }
  bootLoopCleared = true;
  if (prefsReady && bootLoopCount != 0) {
    prefs.putUInt(KEY_BOOT_LOOP, 0);
  }
}

void Board::UpdateGreenLed(ulong now) {
  digitalWrite(PIN_LED_GREEN, (now % HEARTBEAT_PERIOD_MS) < HEARTBEAT_ON_MS ? HIGH : LOW);
}

// Blinks the lowest active fault number, then pauses, and starts again.
// A new code is only picked up at the start of a repetition.
void Board::UpdateRedLed(ulong now) {
  if (redStep == 0) {
    redCode = 0;
    for (uint8_t n = 1; n <= 8; n++) {
      if (faults & (1u << (n - 1))) {
        redCode = n;
        break;
      }
    }
    if (redCode == 0) {
      digitalWrite(PIN_LED_RED, LOW);
      return;
    }
    redStep = 1;
    redStepAt = now;
    digitalWrite(PIN_LED_RED, HIGH);
    return;
  }

  // Steps 1 .. 2 * code alternate on / off, then one pause.
  uint8_t lastStep = redCode * 2;
  ulong duration = redStep == lastStep ? RED_PAUSE_MS : RED_BLINK_MS;
  if (now - redStepAt < duration) {
    return;
  }
  redStepAt = now;
  if (redStep == lastStep) {
    redStep = 0;
    return;
  }
  redStep++;
  digitalWrite(PIN_LED_RED, (redStep % 2 == 1) ? HIGH : LOW);
}

bool Board::IsPowerFault() const { return digitalRead(PIN_PWR_FLT) == LOW; }

uint32_t Board::TakePowerFaultEdges() {
  noInterrupts();
  uint32_t edges = powerFaultEdges;
  powerFaultEdges = 0;
  interrupts();
  return edges;
}

bool Board::TakePowerFaultEvent() {
  bool event = powerFaultEvent;
  powerFaultEvent = false;
  return event;
}

void Board::SetFault(Fault fault, bool active) {
  uint8_t bit = 1u << ((uint8_t)fault - 1);
  if (active) {
    faults |= bit;
  } else {
    faults &= ~bit;
  }
}

bool Board::HasFault(Fault fault) const {
  return faults & (1u << ((uint8_t)fault - 1));
}

const char *Board::ResetReasonName(esp_reset_reason_t reason) {
  switch (reason) {
  case ESP_RST_POWERON:
    return "POWERON (power-up or RESET button)";
  case ESP_RST_EXT:
    return "EXT";
  case ESP_RST_SW:
    return "SW (restart)";
  case ESP_RST_PANIC:
    return "PANIC";
  case ESP_RST_INT_WDT:
    return "INT_WDT";
  case ESP_RST_TASK_WDT:
    return "TASK_WDT";
  case ESP_RST_WDT:
    return "WDT";
  case ESP_RST_DEEPSLEEP:
    return "DEEPSLEEP";
  case ESP_RST_BROWNOUT:
    return "BROWNOUT";
  case ESP_RST_SDIO:
    return "SDIO";
  default:
    return "UNKNOWN";
  }
}
