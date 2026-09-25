#include "Log.h"

namespace {
const size_t BUFFER_SIZE = 4096;
char ring[BUFFER_SIZE];
size_t head = 0;   // next write position
size_t filled = 0; // bytes in use
// Written from loop() and from the web server task (OTA messages).
portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
} // namespace

LogPrint Log;

void LogPrint::Append(const uint8_t *buffer, size_t size) {
  portENTER_CRITICAL(&lock);
  for (size_t i = 0; i < size; i++) {
    ring[head] = (char)buffer[i];
    head = (head + 1) % BUFFER_SIZE;
  }
  filled = min(BUFFER_SIZE, filled + size);
  portEXIT_CRITICAL(&lock);
}

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
