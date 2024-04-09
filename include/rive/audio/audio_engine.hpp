#ifdef WITH_RIVE_AUDIO
#ifndef _RIVE_AUDIO_ENGINE_HPP_
#define _RIVE_AUDIO_ENGINE_HPP_

#include "rive/refcnt.hpp"
#include "rive/span.hpp"
#include <vector>
#include <stdio.h>
#include <cstdint>
#include <mutex>

typedef struct ma_engine ma_engine;
typedef struct ma_sound ma_sound;
typedef struct ma_device ma_device;
typedef struct ma_node_base ma_node_base;

namespace rive
{
class AudioSound;
class AudioSource;
class LevelsNode;
class AudioEngine : public RefCnt<AudioEngine>
{
    friend class AudioSound;
    friend class AudioSource;
    friend class LevelsNode;

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
    bool sumAudioFrames(float* frames, uint64_t numFrames);
#endif

#ifdef WITH_RIVE_AUDIO_TOOLS
    void initLevelMonitor();
    void levels(Span<float> levels);
    float level(uint32_t channel);
#endif

private:
    AudioEngine(ma_engine* engine);
    ma_device* m_device;
    ma_engine* m_engine;
    std::mutex m_mutex;

    void soundCompleted(rcp<AudioSound> sound);

    std::vector<rcp<AudioSound>> m_completedSounds;
    rcp<AudioSound> m_playingSoundsHead;
    static void SoundCompleted(void* pUserData, ma_sound* pSound);

#ifdef WITH_RIVE_AUDIO_TOOLS
    void measureLevels(const float* frames, uint32_t frameCount);
    std::vector<float> m_levels;
    LevelsNode* m_levelMonitor = nullptr;
#endif
#ifdef EXTERNAL_RIVE_AUDIO_ENGINE
    std::vector<float> m_readFrames;
#endif
};
} // namespace rive

#endif
#endif