#include "AudioLoop.h"

AudioLoop::AudioLoop(const uint8_t *samplesIn, int sampleCountIn, int loopStartSampleIn,
                     int loopEndSampleIn) {
  loopStartSample = loopStartSampleIn;
  loopEndSample = loopEndSampleIn;
  samples = samplesIn;
  totalSampleCount = sampleCountIn;
}

void AudioLoop::Begin() {
  isPlaying = true;
  isLooping = false;
  loopCounter = 0;
  sampleReadIndex = 0;
}

bool AudioLoop::IsPlaying() { return isPlaying; }

void AudioLoop::Stop() { isLooping = false; }

void AudioLoop::Read(uint8_t *buffer, int len) {
  for (int i = 0; i < len; i += 2) {
    if (!isPlaying) {
      buffer[i + 0] = 0;
      buffer[i + 1] = 0;
    } else {
      int byteReadIndex = sampleReadIndex * 2;
      buffer[i + 0] = samples[byteReadIndex + 0];
      buffer[i + 1] = samples[byteReadIndex + 1];

      sampleReadIndex++;

      if (!isLooping && loopCounter == 0 &&
          sampleReadIndex >= loopStartSample) {
        isLooping = true;
      }

      if (isLooping && sampleReadIndex >= loopEndSample) {
        sampleReadIndex -= loopEndSample - loopStartSample;
        loopCounter++;
      }

      if (sampleReadIndex >= totalSampleCount) {
        isPlaying = false;
      }
    }
  }
}