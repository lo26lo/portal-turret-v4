#include "board/Log.h"
#include "Audio.h"

#define I2S_PORT I2S_NUM_0
#define SAMPLE_RATE 44100

Audio::Audio()
    : source("/", ".mp3"), player(source, i2s, decoder),
      // sizeof on the array itself (not a pointer): 16-bit samples, 2 bytes each
      ShootAudio(samples, sizeof(samples) / 2, 1537, 10102) {}

void Audio::Initialize(Settings &settingsIn, bool reducedModeIn) {
  settings = &settingsIn;
  reducedMode = reducedModeIn;
  amp.Initialize();

  source.selectStream("/fire.mp3");

  auto cfg = i2s.defaultConfig(TX_MODE);
  cfg.pin_bck = PIN_BCLK;
  cfg.pin_ws = PIN_LRCLK;
  cfg.pin_data = PIN_DIN;
  cfg.pin_mck = -1;
  cfg.sample_rate = SAMPLE_RATE;
  cfg.bits_per_sample = 16;
  cfg.channels = 1;
  cfg.buffer_count = 8;
  cfg.buffer_size = 512;
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
}

void Audio::End() {
  amp.Shutdown(); // never stop LRCLK while the amp is running
  if (i2sRunning) {
    i2s.end();
    i2sRunning = false;
  }
}

void Audio::PlayTone(uint32_t hz, uint32_t ms) {
  tonePhase = 0;
  tonePhaseStep = 2.0f * PI * hz / SAMPLE_RATE;
  toneSamplesLeft = (uint64_t)SAMPLE_RATE * ms / 1000;
}

void Audio::FillTone(uint8_t *buffer, int len) {
  const float amplitude = 12000.0f;
  for (int i = 0; i + 1 < len; i += 2) {
    int16_t sample = 0;
    if (toneSamplesLeft > 0) {
      sample = (int16_t)(amplitude * sinf(tonePhase));
      tonePhase += tonePhaseStep;
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
