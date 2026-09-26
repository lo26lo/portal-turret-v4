#include "Log.h"

#include <esp_attr.h>

namespace {
const size_t BUFFER_SIZE = 4096;
char ring[BUFFER_SIZE];
size_t head = 0;   // next write position
size_t filled = 0; // bytes in use
// Written from loop() and from the web server task (OTA messages).
portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;

// A3: copy of the end of the log in RTC memory that is not cleared at reset.
// It survives a software restart, a panic and the watchdogs (not a power-up),
// so the next boot can show what happened just before a crash.
const uint32_t RTC_MAGIC = 0x7E7A2C0D;
const size_t RTC_SIZE = 2048;
struct RtcLog {
  uint32_t magic;
  uint32_t head;
  uint32_t filled;
  char data[RTC_SIZE];
};
RTC_NOINIT_ATTR RtcLog rtcLog;
String previousRun;
} // namespace

LogPrint Log;

void LogPrint::Append(const uint8_t *buffer, size_t size) {
  portENTER_CRITICAL(&lock);
  for (size_t i = 0; i < size; i++) {
    ring[head] = (char)buffer[i];
    head = (head + 1) % BUFFER_SIZE;
  }
  filled = min(BUFFER_SIZE, filled + size);
  if (rtcLog.magic == RTC_MAGIC) {
    for (size_t i = 0; i < size; i++) {
      rtcLog.data[rtcLog.head] = (char)buffer[i];
      rtcLog.head = (rtcLog.head + 1) % RTC_SIZE;
    }
    rtcLog.filled = min(RTC_SIZE, (size_t)rtcLog.filled + size);
  }
  portEXIT_CRITICAL(&lock);
}

namespace CrashLog {

void Begin(esp_reset_reason_t reason) {
  // After a power-up the RTC memory holds garbage: the magic and the bounds
  // tell a valid buffer from noise.
  bool valid = reason != ESP_RST_POWERON && rtcLog.magic == RTC_MAGIC &&
               rtcLog.head < RTC_SIZE && rtcLog.filled <= RTC_SIZE;
  if (valid && rtcLog.filled > 0) {
    size_t count = rtcLog.filled;
    size_t start = (rtcLog.head + RTC_SIZE - count) % RTC_SIZE;
    previousRun.reserve(count);
    for (size_t i = 0; i < count; i++) {
      char c = rtcLog.data[(start + i) % RTC_SIZE];
      previousRun += (c == '\0') ? '?' : c;
    }
    // Drop the first, probably cut, line when the buffer was full.
    if (count == RTC_SIZE) {
      int newline = previousRun.indexOf('\n');
      if (newline >= 0) {
        previousRun.remove(0, newline + 1);
      }
    }
  }
  rtcLog.head = 0;
  rtcLog.filled = 0;
  rtcLog.magic = RTC_MAGIC;
}

const String &PreviousRun() { return previousRun; }

} // namespace CrashLog

size_t LogPrint::write(uint8_t c) {
  Append(&c, 1);
  return Serial.write(c);
}

size_t LogPrint::write(const uint8_t *buffer, size_t size) {
  Append(buffer, size);
  return Serial.write(buffer, size);
}

String LogPrint::Tail(size_t maxBytes) {
  // Heap, not stack (web server task) nor static (two requests at once).
  char *copy = (char *)malloc(BUFFER_SIZE + 1);
  if (copy == nullptr) {
    return String();
  }
  portENTER_CRITICAL(&lock);
  size_t count = min(maxBytes, filled);
  size_t start = (head + BUFFER_SIZE - count) % BUFFER_SIZE;
  for (size_t i = 0; i < count; i++) {
    copy[i] = ring[(start + i) % BUFFER_SIZE];
  }
  portEXIT_CRITICAL(&lock);
  copy[count] = '\0';

  // Drop the first, probably cut, line when the buffer has wrapped.
  char *text = copy;
  if (count == filled && filled == BUFFER_SIZE) {
    char *newline = strchr(copy, '\n');
    if (newline != nullptr) {
      text = newline + 1;
    }
  }
  String result(text);
  free(copy);
  return result;
}
