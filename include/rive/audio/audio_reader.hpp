#ifdef WITH_RIVE_AUDIO
#ifndef _RIVE_AUDIO_READER_HPP_
#define _RIVE_AUDIO_READER_HPP_

#include "miniaudio.h"
#include "rive/refcnt.hpp"
#include "rive/span.hpp"
#include <vector>

namespace rive
{
class AudioSource;
class AudioReader : public RefCnt<AudioReader>
{
  friend class AudioSource;

public:
  uint64_t lengthInFrames();
  Span<float> read(uint64_t frameCount);
  // Decodes up to frameCount frames into out, setting framesRead; false on a
  // decoder error, true at the end of the stream.
  bool readInto(float* out, uint64_t frameCount, uint64_t& framesRead);
  // Moves to frame at the reader's own rate, clamped to the end, setting
  // landed to where it lands; false when the source cannot.
  bool seek(uint64_t frame, uint64_t* landed = nullptr);
  // Back to the start without resetting the resampler, so a loop is seamless.
  bool rewind();
  // A block of the source onto out, done once it ends or out passes
  // maxFrames; false on a decoder error.
  bool readBlock(uint64_t maxFrames, std::vector<float>& out, bool& done);
  // The rest of the source into out, as readBlock does.
  bool readAll(uint64_t maxFrames, std::vector<float>& out);
  ~AudioReader();
  uint32_t channels() const;
  uint32_t sampleRate() const;

private:
  AudioReader(rcp<AudioSource> audioSource);
  // Converts from the decoder, or a buffered source's own samples.
  bool init(uint32_t channels, uint32_t sampleRate);
  // The source's length in its own frames; false when it cannot tell.
  bool inputLength(ma_uint64& length);
  // Sets frame to where it lands.
  bool seekInput(uint64_t& frame);
  // The next run of source frames, none at its end; false on an error.
  bool nextInput();

  rcp<AudioSource> m_source;
  bool m_decoding = false;
  bool m_converting = false;
  ma_decoder m_decoder;
  ma_data_converter m_converter;
  // A buffered source's next frame.
  uint64_t m_inputFrame = 0;
  std::vector<float> m_input;
  // Source frames the converter has not taken yet.
  const float* m_pending = nullptr;
  uint64_t m_pendingFrames = 0;
  std::vector<float> m_readBuffer;
};

} // namespace rive

#endif
#endif
