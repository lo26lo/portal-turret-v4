#include "board/Log.h"
#include "Audio.h"

#include <LittleFS.h>
#include <esp_heap_caps.h>

#define I2S_PORT I2S_NUM_0
#define SAMPLE_RATE 44100

namespace {
// Folders of the voice lines on LittleFS, in the order of enum Voice.
const char *const VOICE_FOLDERS[(int)Voice::Count] = {
    "/01_activate", "/02_collision", "/04_shutdown", "/05_pickup",
    "/06_retire",   "/07_search",    "/08_tipped",   "/09",
};
const char *const VOICE_NAMES[(int)Voice::Count] = {
    "activate", "collision", "shutdown", "pickup", "retire", "search", "tipped", "alarm",
};
// The MP3 decoder takes ~24 KB in 8 blocks, the largest 8.7 KB (measured).
// libhelix's allocator waits forever when malloc fails (the loop watchdog
// would then reset the turret), so a voice is refused below these margins.
const size_t VOICE_HEAP_NEEDED = 40 * 1024;
const size_t VOICE_BLOCK_NEEDED = 12 * 1024;

size_t ReadFile(void *context, uint8_t *buffer, size_t size) {
  return static_cast<File *>(context)->read(buffer, size);
}
} // namespace

Audio::Audio()
    : // sizeof on the array itself (not a pointer): 16-bit samples, 2 bytes each
      ShootAudio(samples, sizeof(samples) / 2, 1537, 10102) {}

void Audio::Initialize(Settings &settingsIn, bool reducedModeIn) {
  settings = &settingsIn;
  reducedMode = reducedModeIn;
  amp.Initialize();
  CountVoiceFiles();

  // Mono in, but the driver runs the default RIGHT_LEFT format: the DMA holds
  // 16-bit stereo frames (the voice lines are written to it directly).
  auto cfg = i2s.defaultConfig(TX_MODE);
  cfg.port_no = I2S_PORT;
  cfg.pin_bck = PIN_BCLK;
  cfg.pin_ws = PIN_LRCLK;
  cfg.pin_data = PIN_DIN;
  cfg.pin_mck = -1;
  cfg.sample_rate = SAMPLE_RATE;
  cfg.bits_per_sample = 16;
  cfg.channels = 1;
  cfg.buffer_count = DMA_BUFFER_COUNT;
  cfg.buffer_size = DMA_BUFFER_FRAMES;
  cfg.use_apll = false;
  cfg.auto_clear = true; // DMA sends zeros when there is nothing to play
  cfg.fixed_mclk = 0;
  i2sRunning = i2s.begin(cfg);
  if (!i2sRunning) {
    Log.println("Audio: I2S start failed, amp left in shutdown");
    return;
  }

  ApplySettings();
  amp.Start(); // SD low, gain, 10 ms, SD high
}

void Audio::ApplySettings() {
  if (settings == nullptr) {
    return;
  }
  amp.SetGain(reducedMode ? 9 : settings->GetInt(SettingId::AmpGain));
  volume = settings->GetInt(SettingId::Volume);
  if (!settings->GetBool(SettingId::Voices)) {
    StopVoice();
  }
}

void Audio::End() {
  StopVoice();
  amp.Shutdown(); // never stop LRCLK while the amp is running
  if (i2sRunning) {
    i2s.end();
    i2sRunning = false;
  }
}

void Audio::PlayTone(uint32_t hz, uint32_t ms) {
  tonePhase = 0;
  tonePhaseStep = 2.0f * PI * hz / SAMPLE_RATE;
  tonePhaseStepDelta = 0;
  toneSamplesLeft = (uint64_t)SAMPLE_RATE * ms / 1000;
}

void Audio::PlayChirp(uint32_t startHz, uint32_t endHz, uint32_t ms) {
  PlayTone(startHz, ms);
  if (toneSamplesLeft > 0) {
    float endStep = 2.0f * PI * endHz / SAMPLE_RATE;
    tonePhaseStepDelta = (endStep - tonePhaseStep) / toneSamplesLeft;
  }
}

void Audio::FillTone(uint8_t *buffer, int len) {
  const float amplitude = 12000.0f;
  for (int i = 0; i + 1 < len; i += 2) {
    int16_t sample = 0;
    if (toneSamplesLeft > 0) {
      sample = (int16_t)(amplitude * sinf(tonePhase));
      tonePhase += tonePhaseStep;
      tonePhaseStep += tonePhaseStepDelta;
      if (tonePhase > 2.0f * PI) {
        tonePhase -= 2.0f * PI;
      }
      toneSamplesLeft--;
    }
    buffer[i] = sample & 0xFF;
    buffer[i + 1] = (sample >> 8) & 0xFF;
  }
}

void Audio::Update(ulong deltaTime) {
  if (voicePlaying && (ShootAudio.IsPlaying() || toneSamplesLeft > 0)) {
    StopVoice(); // the gunshot and the test tones take over
  }
  if (i2sRunning && voicePlaying) {
    UpdateVoice();
    return;
  }
  // availableForWrite() is a constant 3072 bytes (6 x 512) with this driver:
  // 1536 samples, i.e. exactly 3 DMA buffers once doubled to stereo, so these
  // blocking writes keep the driver aligned on its buffers (see UpdateVoice).
  if (i2sRunning && !ShootAudio.IsPlaying() && toneSamplesLeft > 0) {
    int bytesToWrite = min(i2s.availableForWrite(), (int)sizeof(sampleBuffer)) & ~1;
    if (bytesToWrite > 0) {
      FillTone(sampleBuffer, bytesToWrite);
      ApplyVolume(sampleBuffer, bytesToWrite);
      i2s.write(sampleBuffer, bytesToWrite);
    }
    return;
  }
  if (i2sRunning && ShootAudio.IsPlaying()) {
    // availableForWrite() can exceed sampleBuffer; keep an even count (16-bit samples)
    int bytesToWrite = min(i2s.availableForWrite(), (int)sizeof(sampleBuffer)) & ~1;
    if (bytesToWrite <= 0) {
      return;
    }
    ShootAudio.Read(sampleBuffer, bytesToWrite);
    ApplyVolume(sampleBuffer, bytesToWrite);
    i2s.write(sampleBuffer, bytesToWrite);
  }
}

// ---------------------------------------------------------------- voice lines

const char *Audio::VoiceName(Voice voice) {
  return (int)voice < (int)Voice::Count ? VOICE_NAMES[(int)voice] : "?";
}

bool Audio::VoiceFromName(const char *name, Voice &voice) {
  for (int i = 0; i < (int)Voice::Count; i++) {
    if (strcasecmp(name, VOICE_NAMES[i]) == 0) {
      voice = (Voice)i;
      return true;
    }
  }
  return false;
}

namespace {
bool IsMp3(File &file) {
  String name = file.name();
  name.toLowerCase();
  return !file.isDirectory() && name.endsWith(".mp3");
}
} // namespace

void Audio::CountVoiceFiles() {
  int total = 0;
  for (int i = 0; i < (int)Voice::Count; i++) {
    voiceCount[i] = 0;
    voiceLast[i] = -1;
    File dir = LittleFS.open(VOICE_FOLDERS[i]);
    if (!dir || !dir.isDirectory()) {
      continue;
    }
    for (File file = dir.openNextFile(); file; file = dir.openNextFile()) {
      if (IsMp3(file) && voiceCount[i] < 255) {
        voiceCount[i]++;
      }
    }
    total += voiceCount[i];
  }
  Log.printf("Audio: %d voice lines on LittleFS%s\n", total, total ? "" : " (pio run -t uploadfs)");
}

bool Audio::PlayVoice(Voice voice) {
  int index = (int)voice;
  if (index >= (int)Voice::Count || !i2sRunning || settings == nullptr ||
      !settings->GetBool(SettingId::Voices) || ShootAudio.IsPlaying() || toneSamplesLeft > 0) {
    return false;
  }
  int pick = logic::PickVoiceFile(voiceCount[index], voiceLast[index], esp_random());
  if (pick < 0) {
    return false;
  }
  StopVoice();
  File dir = LittleFS.open(VOICE_FOLDERS[index]);
  int seen = 0;
  for (File file = dir.openNextFile(); file; file = dir.openNextFile()) {
    if (IsMp3(file) && seen++ == pick) {
      voiceFile = file;
      break;
    }
  }
  if (!voiceFile) {
    return false;
  }
  if (ESP.getFreeHeap() < VOICE_HEAP_NEEDED ||
      heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) < VOICE_BLOCK_NEEDED ||
      !voiceDecoder.Begin(ReadFile, &voiceFile)) {
    Log.println("Audio: not enough memory for the MP3 decoder");
    voiceFile.close();
    return false;
  }
  voiceLast[index] = pick;
  snprintf(voiceName, sizeof(voiceName), "%s/%s", VOICE_FOLDERS[index], voiceFile.name());
  voiceBlockFrames = 0;
  voiceBlockSent = 0;
  voiceFileDone = false;
  upsampler.Reset();
  voicePlaying = true;
  Log.printf("Voice %s: %s\n", VOICE_NAMES[index], voiceName);
  return true;
}

void Audio::StopVoice() {
  if (voicePlaying && i2sRunning && voiceBlockSent > 0) {
    // Rest of a block the driver took in part (it takes whole DMA buffers, so
    // this is only a safety net): it stays aligned on its buffers.
    size_t written = 0;
    i2s_write(I2S_PORT, (const uint8_t *)voiceBlock + voiceBlockSent, sizeof(voiceBlock) - voiceBlockSent,
              &written, portMAX_DELAY);
  }
  voicePlaying = false;
  voiceDecoder.End();
  if (voiceFile) {
    voiceFile.close();
  }
  voiceBlockFrames = 0;
  voiceBlockSent = 0;
  voiceFileDone = false;
}

// Keeps the DMA full without ever waiting (ticks_to_wait 0), one whole DMA
// buffer (2048 bytes) at a time. The tone and gunshot writes are whole buffers
// too (see Update), so the driver is always aligned on its buffers: a sound
// never ends in a half-written buffer whose other half would replay old
// samples. Up to ~90 ms ahead, so a loop() slowed down by the OLED or the web
// server does not starve the speaker; the decoding work follows what was played.
void Audio::UpdateVoice() {
  for (int i = 0; i < DMA_BUFFER_COUNT; i++) {
    if (!FillVoiceBlock()) {
      StopVoice(); // played to the end; the DMA sends zeros after it (auto_clear)
      return;
    }
    size_t written = 0;
    i2s_write(I2S_PORT, (const uint8_t *)voiceBlock + voiceBlockSent, sizeof(voiceBlock) - voiceBlockSent,
              &written, 0);
    voiceBlockSent += written;
    if (voiceBlockSent < sizeof(voiceBlock)) {
      return; // the DMA is full: the rest at the next loop
    }
    voiceBlockFrames = 0;
    voiceBlockSent = 0;
  }
}

// Completes the block with the upsampled samples (volume applied, same on both
// channels), decoding frames as needed; after the last frame, silence. False
// when there is nothing left to play.
bool Audio::FillVoiceBlock() {
  while (voiceBlockFrames < DMA_BUFFER_FRAMES) {
    int16_t sample;
    if (upsampler.Next(sample)) {
      if (volume < 100) {
        sample = (int16_t)((int32_t)sample * volume / 100);
      }
      voiceBlock[2 * voiceBlockFrames] = sample;
      voiceBlock[2 * voiceBlockFrames + 1] = sample;
      voiceBlockFrames++;
    } else if (!NextVoiceFrame()) {
      if (voiceBlockFrames == 0) {
        return false;
      }
      memset(voiceBlock + 2 * voiceBlockFrames, 0, (DMA_BUFFER_FRAMES - voiceBlockFrames) * 2 * sizeof(int16_t));
      voiceBlockFrames = DMA_BUFFER_FRAMES;
    }
  }
  return true;
}

// Next frame of the file to the upsampler; false (and stays so) at the end.
bool Audio::NextVoiceFrame() {
  const int16_t *samples;
  int count;
  int rate;
  if (voiceFileDone || !voiceDecoder.Next(samples, count, rate)) {
    voiceFileDone = true;
    return false;
  }
  upsampler.Load(samples, count, logic::UpsampleFactor(rate, SAMPLE_RATE));
  return true;
}

// Software volume on 16-bit little-endian samples.
void Audio::ApplyVolume(uint8_t *buffer, int len) {
  if (volume >= 100) {
    return;
  }
  for (int i = 0; i + 1 < len; i += 2) {
    int16_t sample = (int16_t)(buffer[i] | (buffer[i + 1] << 8));
    sample = (int16_t)((int32_t)sample * volume / 100);
    buffer[i] = sample & 0xFF;
    buffer[i + 1] = (sample >> 8) & 0xFF;
  }
}
