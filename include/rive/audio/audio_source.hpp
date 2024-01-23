#ifdef WITH_RIVE_AUDIO
#ifndef _RIVE_AUDIO_SOURCE_HPP_
#define _RIVE_AUDIO_SOURCE_HPP_

#include "miniaudio.h"
#include "rive/refcnt.hpp"
#include "rive/span.hpp"
#include "rive/simple_array.hpp"
#include "rive/audio/audio_format.hpp"

namespace rive
{
class AudioEngine;
class AudioReader;
class AudioSound;
class AudioSource : public RefCnt<AudioSource>
{
public:
    // Makes an audio source that does not own it's backing bytes.
    AudioSource(rive::Span<uint8_t> fileBytes);

    // Makes an audio source whose backing bytes will be deleted when the
    // AudioSource deletes.
    AudioSource(rive::SimpleArray<uint8_t> fileBytes);

    // Makes a buffered audio source whose backing bytes will be deleted when
    // the AudioSource deletes.
    AudioSource(rive::Span<float> samples, uint32_t numChannels, uint32_t sampleRate);

    rcp<AudioReader> makeReader(uint32_t numChannels, uint32_t sampleRate);

    uint32_t channels();
    uint32_t sampleRate();
    AudioFormat format() const;
    const rive::Span<uint8_t> bytes() const { return m_fileBytes; }

    const rive::Span<float> bufferedSamples() const;
    bool isBuffered() const { return m_isBuffered; }

private:
    bool m_isBuffered;
    uint32_t m_channels;
    uint32_t m_sampleRate;
    rive::Span<uint8_t> m_fileBytes;
    rive::SimpleArray<uint8_t> m_ownedBytes;
};
} // namespace rive

#endif
#endif