#ifdef WITH_RIVE_AUDIO
#ifndef _RIVE_AUDIO_ENGINE_HPP_
#define _RIVE_AUDIO_ENGINE_HPP_

#include "rive/refcnt.hpp"
#include "rive/span.hpp"
#include <vector>
#include <stdio.h>
#include <cstdint>

typedef struct ma_engine ma_engine;
typedef struct ma_sound ma_sound;
typedef struct ma_device ma_device;

namespace rive
{
class AudioSound;
class AudioSource;

class AudioEngine : public RefCnt<AudioEngine>
{
    friend class AudioSound;
    friend class AudioSource;

public:
    static const uint32_t defaultNumChannels = 2;
    static const uint32_t defaultSampleRate = 48000;

    static rcp<AudioEngine> Make(uint32_t numChannels, uint32_t sampleRate);

    ma_device* device() { return m_device; }
    ma_engine* engine() { return m_engine; }

    uint32_t channels() const;
    uint32_t sampleRate() const;
    uint64_t timeInFrames();

    ~AudioEngine();
    rcp<AudioSound> play(rcp<AudioSource> source,
                         uint64_t startTime,
                         uint64_t endTime,
                         uint64_t soundStartTime);

    static rcp<AudioEngine> RuntimeEngine();

#ifdef EXTERNAL_RIVE_AUDIO_ENGINE
    bool readAudioFrames(float* frames, uint64_t numFrames, uint64_t* framesRead = nullptr);
#endif

private:
    AudioEngine(ma_engine* engine);
    ma_device* m_device;
    ma_engine* m_engine;

    std::vector<rcp<AudioSound>> m_completedSounds;

    void completeSound(rcp<AudioSound> sound);
    void purgeCompletedSounds();
    static void SoundCompleted(void* pUserData, ma_sound* pSound);
};
} // namespace rive

#endif
#endif