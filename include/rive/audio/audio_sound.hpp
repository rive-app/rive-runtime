#ifdef WITH_RIVE_AUDIO
#ifndef _RIVE_AUDIO_SOUND_HPP_
#define _RIVE_AUDIO_SOUND_HPP_

#include "miniaudio.h"
#include "rive/refcnt.hpp"
#include "rive/audio/audio_source.hpp"

namespace rive
{
class AudioEngine;
class Artboard;
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
    AudioSound(AudioEngine* engine, rcp<AudioSource> source, Artboard* artboard);
    ma_decoder* decoder() { return &m_decoder; }
    ma_audio_buffer* buffer() { return &m_buffer; }
    ma_sound* sound() { return &m_sound; }
    void dispose();

    ma_decoder m_decoder;
    ma_audio_buffer m_buffer;
    ma_sound m_sound;
    rcp<AudioSource> m_source;

    // This is storage used by the AudioEngine.
    bool m_isDisposed;
    rcp<AudioSound> m_nextPlaying;
    rcp<AudioSound> m_prevPlaying;
    AudioEngine* m_engine;
    Artboard* m_artboard;
};
} // namespace rive

#endif
#endif