#pragma once

// Pure audio helpers (no Arduino): voice upsampling, voice file choice, MP3 tag
// skipping and the "tipped over" watch.
// Used by audio/Audio, main.cpp and the native tests (test/test_logic).

#include <stddef.h>
#include <stdint.h>

namespace logic {

// The voice files (upstream data/, MP3) are 22 050 Hz mono; the I2S runs at
// 44 100 Hz for the gunshot and the test tones. Upsamples 16-bit mono PCM by an
// integer factor with linear interpolation, one output sample at a time (the
// caller fills DMA blocks that do not line up with the MP3 frames). The last
// input sample is kept from one frame to the next, so they join without a
// click; a voice starts from 0.
class Upsampler {
public:
  void Reset() {
    previous = 0;
    input = nullptr;
    count = 0;
    position = 0;
    step = 1;
  }
  // Next block of input (one decoded frame); it must stay valid until Next()
  // returns false.
  void Load(const int16_t *samples, size_t sampleCount, uint8_t upsampleFactor) {
    input = samples;
    count = sampleCount;
    position = 0;
    step = 1;
    factor = upsampleFactor < 1 ? 1 : upsampleFactor;
  }
  // Next output sample; false once the block is used up.
  bool Next(int16_t &out) {
    if (position >= count) {
      return false;
    }
    int32_t target = input[position];
    out = (int16_t)(previous + (target - previous) * step / factor);
    if (step < factor) {
      step++;
    } else {
      previous = target;
      position++;
      step = 1;
    }
    return true;
  }

private:
  int32_t previous = 0;
  const int16_t *input = nullptr;
  size_t count = 0;
  size_t position = 0;
  uint8_t step = 1;
  uint8_t factor = 1;
};

// Integer upsampling factor to reach outputRate from a file's sample rate
// (22 050 -> 2 for 44 100); 1 when the rate is unknown or already high enough.
inline uint8_t UpsampleFactor(uint32_t inputRate, uint32_t outputRate) {
  if (inputRate == 0 || inputRate >= outputRate) {
    return 1;
  }
  uint32_t factor = (outputRate + inputRate / 2) / inputRate;
  return factor > 8 ? 8 : (uint8_t)factor;
}

// Which of a folder's `count` files to play: randomValue (any, e.g. esp_random)
// picks one, never `last` again when there is a choice. -1 if the folder is empty.
inline int PickVoiceFile(int count, int last, uint32_t randomValue) {
  if (count <= 0) {
    return -1;
  }
  if (count == 1) {
    return 0;
  }
  if (last < 0 || last >= count) {
    return (int)(randomValue % (uint32_t)count);
  }
  // One of the count - 1 others, evenly.
  int pick = (int)(randomValue % (uint32_t)(count - 1));
  return pick >= last ? pick + 1 : pick;
}

// Bytes taken by an ID3v2 tag at the start of an MP3 file (header, the four
// "syncsafe" size bytes of 7 bits, and the footer if flagged), 0 if `header`
// (the first 10 bytes of the file) is not one. Skipping it means a picture or
// text in the tag can never be mistaken for an MP3 frame.
inline uint32_t Id3v2Size(const uint8_t *header, size_t length) {
  if (length < 10 || header[0] != 'I' || header[1] != 'D' || header[2] != '3') {
    return 0;
  }
  if ((header[6] | header[7] | header[8] | header[9]) & 0x80) {
    return 0; // not syncsafe: not a valid tag
  }
  uint32_t size = ((uint32_t)header[6] << 21) | ((uint32_t)header[7] << 14) |
                  ((uint32_t)header[8] << 7) | header[9];
  return 10 + size + ((header[5] & 0x10) ? 10 : 0);
}

// "Tipped over" voice line: true once when the turret stops being upright for
// TIP_CONFIRM_MS after having been upright for at least UPRIGHT_ARM_MS. Never
// fires while the check is impossible (IMU missing, ImuUpAxis 0, no reading).
const uint32_t TIP_CONFIRM_MS = 500;
const uint32_t UPRIGHT_ARM_MS = 2000;

class TipWatch {
public:
  bool Update(bool checkable, bool upright, uint32_t now) {
    if (!checkable) {
      armed = false;
      uprightTiming = false;
      tippedTiming = false;
      return false;
    }
    if (upright) {
      tippedTiming = false;
      if (!uprightTiming) {
        uprightTiming = true;
        uprightSince = now;
      }
      if (now - uprightSince >= UPRIGHT_ARM_MS) {
        armed = true;
      }
      return false;
    }
    uprightTiming = false;
    if (!armed) {
      return false;
    }
    if (!tippedTiming) {
      tippedTiming = true;
      tippedSince = now;
    }
    if (now - tippedSince >= TIP_CONFIRM_MS) {
      armed = false; // once per fall: upright again for UPRIGHT_ARM_MS re-arms
      return true;
    }
    return false;
  }

private:
  bool armed = false;
  bool uprightTiming = false;
  bool tippedTiming = false;
  uint32_t uprightSince = 0;
  uint32_t tippedSince = 0;
};

} // namespace logic
