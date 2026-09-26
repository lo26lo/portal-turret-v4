#pragma once

#include <Arduino.h>
#include <esp_system.h>

// Console output: goes to Serial (USB CDC) and to a RAM ring buffer that the
// web page reads through GET /api/log (diagnostics without a USB cable).
// Use Log.print / println / printf instead of Serial for messages.
class LogPrint : public Print {
public:
  size_t write(uint8_t c) override;
  size_t write(const uint8_t *buffer, size_t size) override;
  // Last `maxBytes` bytes of the buffer (whole lines when possible).
  String Tail(size_t maxBytes = 4096);

private:
  void Append(const uint8_t *buffer, size_t size);
};

extern LogPrint Log;

// A3: the last 2 kB of the log are also kept in RTC memory, which survives a
// software restart, a panic and the watchdogs (not a power-up).
namespace CrashLog {
// First thing in setup(), before any message: keeps the tail of the previous
// run (if the RTC memory is valid and the reset was not a power-up), then
// starts a new one.
void Begin(esp_reset_reason_t reason);
// Tail of the previous run's log, empty if none.
const String &PreviousRun();
} // namespace CrashLog
