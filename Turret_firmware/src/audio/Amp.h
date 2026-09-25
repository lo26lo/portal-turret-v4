#pragma once

#include <Arduino.h>

// MAX98357A control (plan §2.3):
//  - SD_MODE on IO13 through 2k: low = shutdown, high = left channel;
//  - gain on IO21 / IO47, OPEN DRAIN ONLY, never driven high:
//      both released = 9 dB, IO21 low = 12 dB, IO47 low = 15 dB;
//  - the gain is only latched when leaving shutdown: SD low, gain, 10 ms, SD high;
//  - never stop LRCLK while BCLK runs: Shutdown() before i2s.end(), a sample
//    rate change or ESP.restart().
class Amp {
public:
  // Pins to their safe state (also done by Board::Begin at reset). Leaves the amp in shutdown.
  void Initialize();
  // Sets the gain (9, 12 or 15 dB; other values rounded to the nearest). If the
  // amp is running it goes through shutdown for 10 ms so that the gain is latched.
  void SetGain(int32_t db);
  int32_t GetGain() const { return gainDb; }

  // Leaves shutdown with the current gain. Call only while the I2S clocks run.
  void Start();
  // SD low. Safe at any time; required before stopping the I2S clocks.
  void Shutdown();
  bool IsRunning() const { return running; }

  // User mute (button B): SD low while muted, restored on unmute if started.
  void SetMuted(bool muted);
  bool IsMuted() const { return muted; }

private:
  void ApplyGainPins();

  int32_t gainDb = 9;
  bool started = false; // Start() called, Shutdown() not since
  bool running = false; // SD currently high
  bool muted = false;
};
