#pragma once

#include "Amp.h"
#include "Arduino.h"
#include "AudioLoop.h"
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecMP3Helix.h"
#include "AudioTools/Disk/AudioSourceLittleFS.h"
#include "GunShotAudio.h"
#include "driver/i2s.h"
#include "pins.h"
#include "settings/Settings.h"

class Audio {
public:
  Audio();
  // Boot step 9: I2S clocks first, then the amp leaves shutdown (no pop).
  // Reduced mode (D5) forces 9 dB.
  void Initialize(Settings &settings, bool reducedMode);
  // Reads AmpGain and Volume again (after a change from the web page or console).
  void ApplySettings();
  void Update(ulong deltaTime);
  // Shutdown sequence (plan §3.3): amp in shutdown, then I2S stopped.
  void End();
  // Test tone (console / web page): sine at `hz` for `ms`, at the current volume.
  void PlayTone(uint32_t hz, uint32_t ms);
  // L2 (lab aid): frequency sweeping linearly from startHz to endHz over ms.
  void PlayChirp(uint32_t startHz, uint32_t endHz, uint32_t ms);
  void StopTone() { toneSamplesLeft = 0; }
  // Gunshot or test tone in progress.
  bool IsPlaying() { return ShootAudio.IsPlaying() || toneSamplesLeft > 0; }

  AudioLoop ShootAudio;
  Amp amp;

private:
  void ApplyVolume(uint8_t *buffer, int len);
  void FillTone(uint8_t *buffer, int len);

  Settings *settings = nullptr;
  bool reducedMode = false;
  bool i2sRunning = false;
  int32_t volume = 100; // percent
  uint32_t toneSamplesLeft = 0;
  float tonePhase = 0;
  float tonePhaseStep = 0;
  float tonePhaseStepDelta = 0; // per sample, for PlayChirp
  int sampleReadIndex = 0;
  int loopCounter = 0;
  bool isLooping = false;
  uint8_t sampleBuffer[4096];
  I2SStream i2s;
  AudioSourceLittleFS source;
  MP3DecoderHelix decoder;
  AudioPlayer player;
};
