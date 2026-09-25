#pragma once

#include <Arduino.h>

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
