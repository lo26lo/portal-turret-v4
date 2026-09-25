#pragma once

#include <Arduino.h>

class AudioLoop {
 public:
  AudioLoop(const uint8_t* samplesIn, int sampleCountIn, int loopStartSampleIn, int loopEndSampleIn);
  void Read(uint8_t* buffer, int len);
  void Begin();
  void Stop();
  bool IsPlaying();

 private:
  const uint8_t* samples;
  int sampleReadIndex = 0;
  int loopCounter = 0;
  int loopStartSample = 0;
  int loopEndSample = 0;
  int totalSampleCount = 0;
  bool isPlaying = false;
  bool isLooping = false;
};