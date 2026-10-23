#pragma once

#include "Amp.h"
#include "Arduino.h"
#include "AudioLoop.h"
#include "AudioTools.h"
#include "GunShotAudio.h"
#include "Mp3FrameDecoder.h"
#include "driver/i2s.h"
#include "logic/AudioLogic.h"
#include "pins.h"
#include "settings/Settings.h"
#include <FS.h>

// Turret voice lines: the MP3 files of the upstream project (its data/ folder),
// one folder per situation on LittleFS (pio run -t uploadfs). Values index
// Audio::VOICE_FOLDERS and the console names.
enum class Voice : uint8_t {
  Activate,  // /01_activate: deploying on a target
  Collision, // /02_collision
  Shutdown,  // /04_shutdown
  Pickup,    // /05_pickup
  Retire,    // /06_retire: folding back
  Search,    // /07_search
  Tipped,    // /08_tipped: knocked over
  Alarm,     // /09
  Count
};

class Audio {
public:
  Audio();
  // Boot step 9: I2S clocks first, then the amp leaves shutdown (no pop).
  // Reduced mode (D5) forces 9 dB. Also counts the voice files (LittleFS is
  // mounted at step 4).
  void Initialize(Settings &settings, bool reducedMode);
  // Reads AmpGain and Volume again (after a change from the web page or console).
  void ApplySettings();
  // Volume in percent, RAM only (the rotary encoder saves it later, once).
  void SetVolume(int32_t percent) { volume = constrain(percent, 0, 100); }
  int32_t GetVolume() const { return volume; }
  void Update(ulong deltaTime);
  // Shutdown sequence (plan §3.3): voice stopped, amp in shutdown, then I2S stopped.
  void End();
  // Test tone (console / web page): sine at `hz` for `ms`, at the current volume.
  void PlayTone(uint32_t hz, uint32_t ms);
  // L2 (lab aid): frequency sweeping linearly from startHz to endHz over ms.
  void PlayChirp(uint32_t startHz, uint32_t endHz, uint32_t ms);
  void StopTone() { toneSamplesLeft = 0; }
  // Gunshot, test tone or voice line in progress.
  bool IsPlaying() { return ShootAudio.IsPlaying() || toneSamplesLeft > 0 || voicePlaying; }

  // A random file of the voice's folder (never the same one twice in a row).
  // Refused (false) when the setting Voices is off, the I2S is not running,
  // the folder is empty or missing (uploadfs not done), the MP3 decoder cannot
  // get its memory, or the gunshot or a test tone is playing: those always
  // take over and cut a voice.
  bool PlayVoice(Voice voice);
  // What the DMA already holds (up to ~90 ms) still plays out.
  void StopVoice();
  bool IsVoicePlaying() const { return voicePlaying; }
  uint8_t VoiceFileCount(Voice voice) const { return voiceCount[(int)voice]; }
  const char *CurrentVoiceFile() const { return voiceName; }
  static const char *VoiceName(Voice voice);
  static bool VoiceFromName(const char *name, Voice &voice);

  AudioLoop ShootAudio;
  Amp amp;

private:
  // I2S DMA: 8 buffers of 512 stereo frames (2048 bytes, 11.6 ms at 44.1 kHz).
  static const int DMA_BUFFER_COUNT = 8;
  static const int DMA_BUFFER_FRAMES = 512;

  void ApplyVolume(uint8_t *buffer, int len);
  void FillTone(uint8_t *buffer, int len);
  void CountVoiceFiles();
  void UpdateVoice();
  bool FillVoiceBlock();
  bool NextVoiceFrame();

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

  // Voice lines: MP3 file -> decoder, one frame at a time (mono) -> upsampled
  // to the I2S rate -> one DMA buffer's worth of stereo frames.
  File voiceFile;
  Mp3FrameDecoder voiceDecoder; // ~25 KB of heap, only while a voice plays
  bool voicePlaying = false;
  bool voiceFileDone = false; // no frame left: the last block is padded with silence
  char voiceName[40] = "";
  uint8_t voiceCount[(int)Voice::Count] = {};
  int16_t voiceLast[(int)Voice::Count] = {}; // index of the last file played, -1 none
  logic::Upsampler upsampler;                // reads the decoder's current frame
  int16_t voiceBlock[DMA_BUFFER_FRAMES * 2]; // L, R, L, R...
  size_t voiceBlockFrames = 0; // frames filled
  size_t voiceBlockSent = 0;   // bytes already taken by the I2S driver
};
