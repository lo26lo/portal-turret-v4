#pragma once

#include <stddef.h>
#include <stdint.h>

#include "libhelix-mp3/mp3dec.h"

// MP3 -> 16-bit mono PCM, one frame per call, with the C API of the Helix
// decoder (libhelix). Its C++ wrappers buffer ~1 KB of input and only decode
// it all at the end (or never); here the caller decodes exactly what it is
// about to play, down to the last frame of the file. No Arduino here: the
// bytes come from a callback, so the same code also runs on a computer.
class Mp3FrameDecoder {
public:
  // Up to `size` bytes into buffer; returns the count, 0 at the end of the file.
  typedef size_t (*ReadFn)(void *context, uint8_t *buffer, size_t size);

  ~Mp3FrameDecoder() { End(); }

  // Allocates the Helix decoder (~25 KB of heap, freed by End) and skips an
  // ID3v2 tag. Check the free memory first: libhelix's allocator never
  // returns null, it waits forever when malloc fails.
  bool Begin(ReadFn read, void *context);
  void End();

  // Next frame, stereo folded to mono: `count` samples at `sampleRate`, valid
  // until the next call. False at the end of the file. Damaged frames are
  // skipped; a file of garbage ends early.
  bool Next(const int16_t *&samples, int &count, int &sampleRate);

  // Frames that gave no sound since Begin (damaged, or the bit reservoir of
  // the first frames still filling).
  int SkippedFrames() const { return skipped; }

private:
  void Fill();
  void Consume(size_t bytes);

  HMP3Decoder decoder = nullptr;
  ReadFn read = nullptr;
  void *context = nullptr;
  uint8_t in[2048]; // a frame of any MP3 fits (Helix MAINBUF_SIZE: 1940)
  size_t inLength = 0;
  int16_t pcm[MAX_NCHAN * MAX_NGRAN * MAX_NSAMP];
  int skipped = 0;
};
