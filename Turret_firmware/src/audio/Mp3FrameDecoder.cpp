#include "Mp3FrameDecoder.h"

#include <string.h>

#include "logic/AudioLogic.h"

namespace {
// Decode attempts for one frame: each failure skips at least one byte.
const int DECODE_ATTEMPTS = 32;
} // namespace

bool Mp3FrameDecoder::Begin(ReadFn readFn, void *readContext) {
  End();
  decoder = MP3InitDecoder();
  if (decoder == nullptr) {
    return false;
  }
  read = readFn;
  context = readContext;
  skipped = 0;
  // ID3v2 tag (title, artist, sometimes a picture): skipped whole, so that
  // nothing in it can pass for a frame.
  Fill();
  uint32_t tag = logic::Id3v2Size(in, inLength);
  while (tag > 0 && inLength > 0) {
    size_t drop = tag < inLength ? tag : inLength;
    Consume(drop);
    tag -= drop;
    Fill();
  }
  return true;
}

void Mp3FrameDecoder::End() {
  if (decoder != nullptr) {
    MP3FreeDecoder(decoder);
    decoder = nullptr;
  }
  read = nullptr;
  context = nullptr;
  inLength = 0;
}

bool Mp3FrameDecoder::Next(const int16_t *&samples, int &count, int &sampleRate) {
  if (decoder == nullptr) {
    return false;
  }
  for (int attempt = 0; attempt < DECODE_ATTEMPTS; attempt++) {
    Fill(); // `in` full: the frame at its start is then complete
    if (inLength < 4) {
      return false; // end of the file: not even a frame header left
    }
    int offset = MP3FindSyncWord(in, (int)inLength);
    if (offset != 0) {
      // Bytes before the next frame are dropped (all of them if there is
      // none, but the last: it may be the first half of a sync word).
      Consume(offset > 0 ? (size_t)offset : inLength - 1);
      continue;
    }
    unsigned char *position = in;
    int left = (int)inLength;
    int err = MP3Decode(decoder, &position, &left, pcm, 0);
    if (err == ERR_MP3_INDATA_UNDERFLOW) {
      if (inLength < sizeof(in)) {
        return false; // truncated last frame
      }
      Consume(1); // longer than any real frame: a false sync word
      skipped++;
      continue;
    }
    // Frame used, or bad: skip what the decoder read (at least the sync word).
    size_t used = position - in;
    Consume(used > 0 ? used : 1);
    if (err != ERR_MP3_NONE) {
      skipped++; // ERR_MP3_MAINDATA_UNDERFLOW: bit reservoir still filling
      continue;
    }
    MP3FrameInfo info;
    MP3GetLastFrameInfo(decoder, &info);
    count = info.outputSamps;
    if (info.nChans == 2) {
      count /= 2;
      for (int i = 0; i < count; i++) {
        pcm[i] = (int16_t)(((int32_t)pcm[2 * i] + pcm[2 * i + 1]) / 2);
      }
    }
    if (count > 0) {
      samples = pcm;
      sampleRate = info.samprate;
      return true;
    }
  }
  return false;
}

void Mp3FrameDecoder::Fill() {
  while (read != nullptr && inLength < sizeof(in)) {
    size_t got = read(context, in + inLength, sizeof(in) - inLength);
    if (got == 0) {
      return;
    }
    inLength += got;
  }
}

void Mp3FrameDecoder::Consume(size_t bytes) {
  if (bytes >= inLength) {
    inLength = 0;
    return;
  }
  memmove(in, in + bytes, inLength - bytes);
  inLength -= bytes;
}
