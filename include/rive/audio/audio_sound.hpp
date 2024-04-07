#ifdef WITH_RIVE_AUDIO
#ifndef _RIVE_AUDIO_SOUND_HPP_
#define _RIVE_AUDIO_SOUND_HPP_

#include "miniaudio.h"
#include "rive/refcnt.hpp"

namespace rive
{
class AudioEngine;
class AudioSound : public RefCnt<AudioSound>
{
    friend class AudioEngine;

public:
    bool seek(uint64_t timeInFrames);
    ~AudioSound();
    void stop(uint64_t fadeTimeInFrames = 0);
    float volume();
    void volume(float value);
    bool completed() const;

private:
    AudioSound(AudioEngine* engine);
    ma_decoder* decoder() { return &m_decoder; }
    ma_audio_buffer* buffer() { return &m_buffer; }
    ma_sound* sound() { return &m_sound; }
    void dispose();

    ma_decoder m_decoder;
    ma_audio_buffer m_buffer;
    ma_sound m_sound;

    // This is storage used by the AudioEngine.
    bool m_isDisposed;
    rcp<AudioSound> m_nextPlaying;
    rcp<AudioSound> m_prevPlaying;
    AudioEngine* m_engine;
};
} // namespace rive

#endif
#endif