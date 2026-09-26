#pragma once

#include <Arduino.h>

// A3: core dump written by ESP-IDF to the "coredump" partition on a panic or a
// watchdog (enabled in the arduino-esp32 SDK: ELF format, CRC32).
// Decode it on the computer with the firmware.elf of the same build:
//   espcoredump.py --chip esp32s3 info_corefile -c coredump.elf .pio/build/turret2/firmware.elf
namespace CoreDump {
// Boot: checks the partition once and caches the result (checking reads the whole image).
void Begin();
// Size of the stored core dump, 0 if none.
size_t Size();
// One line: crashed task, program counter, backtrace (addresses for addr2line).
const String &Summary();
// Reads part of the image (web download). Returns the bytes read.
size_t Read(size_t offset, uint8_t *buffer, size_t length);
bool Erase();
} // namespace CoreDump
