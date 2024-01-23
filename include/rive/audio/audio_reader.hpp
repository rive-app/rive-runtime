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
    ~AudioReader();
    uint32_t channels() const;
    uint32_t sampleRate() const;

private:
    AudioReader(rcp<AudioSource> audioSource, uint32_t channels);
    ma_decoder* decoder();

    rcp<AudioSource> m_source;
    uint32_t m_channels;
    ma_decoder m_decoder;
    std::vector<float> m_readBuffer;
};

} // namespace rive

#endif
#endif