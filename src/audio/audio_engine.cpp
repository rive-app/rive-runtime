#ifdef WITH_RIVE_AUDIO
#include "rive/math/simd.hpp"
#ifdef __APPLE__
#include <TargetConditionals.h>
#if TARGET_IPHONE_SIMULATOR || TARGET_OS_MACCATALYST || TARGET_OS_IPHONE
// Don't define MINIAUDIO_IMPLEMENTATION ON iOS
#elif TARGET_OS_MAC
#define MINIAUDIO_IMPLEMENTATION
#else
#error "Unknown Apple platform"
#endif
#else
#define MINIAUDIO_IMPLEMENTATION
#endif
#include "miniaudio.h"

#include "rive/audio/audio_engine.hpp"
#include "rive/audio/audio_sound.hpp"
#include "rive/audio/audio_source.hpp"

#include <algorithm>

using namespace rive;

void AudioEngine::SoundCompleted(void* pUserData, ma_sound* pSound)
{
    AudioSound* audioSound = (AudioSound*)pUserData;
    auto engine = audioSound->m_engine;
    engine->soundCompleted(ref_rcp(audioSound));
}

void AudioEngine::unlinkSound(rcp<AudioSound> sound)
{
    auto next = sound->m_nextPlaying;
    auto prev = sound->m_prevPlaying;
    if (next != nullptr)
    {
        next->m_prevPlaying = prev;
    }
    if (prev != nullptr)
    {
        prev->m_nextPlaying = next;
    }

    if (m_playingSoundsHead == sound)
    {
        m_playingSoundsHead = next;
    }

    sound->m_nextPlaying = nullptr;
    sound->m_prevPlaying = nullptr;
}

void AudioEngine::soundCompleted(rcp<AudioSound> sound)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_completedSounds.push_back(sound);
    unlinkSound(sound);
}

#ifdef WITH_RIVE_AUDIO_TOOLS
namespace rive
{
class LevelsNode
{
public:
    ma_node_base base;
    AudioEngine* engine;
    static void measureLevels(ma_node* pNode,
                              const float** ppFramesIn,
                              ma_uint32* pFrameCountIn,
                              float** ppFramesOut,
                              ma_uint32* pFrameCountOut)
    {
        const float* frames = ppFramesIn[0];

        ma_uint32 frameCount = pFrameCountIn[0];

        static_cast<LevelsNode*>(pNode)->engine->measureLevels(frames, (uint32_t)frameCount);
    }
};
} // namespace rive

void AudioEngine::measureLevels(const float* frames, uint32_t frameCount)
{
    uint32_t channelCount = channels();

    for (uint32_t i = 0; i < frameCount; i++)
    {
        for (uint32_t c = 0; c < channelCount; c++)
        {
            float sample = *frames++;
            m_levels[c] = std::max(m_levels[c], sample);
        }
    }
}

static ma_node_vtable measure_levels_vtable = {LevelsNode::measureLevels,
                                               nullptr,
                                               1,
                                               1,
                                               MA_NODE_FLAG_PASSTHROUGH};

void AudioEngine::initLevelMonitor()
{
    if (m_levelMonitor == nullptr)
    {
        m_levelMonitor = new LevelsNode();
        m_levelMonitor->engine = this;

        ma_node_config nodeConfig = ma_node_config_init();
        nodeConfig.vtable = &measure_levels_vtable;
        uint32_t channelCount = channels();
        nodeConfig.pInputChannels = &channelCount;
        nodeConfig.pOutputChannels = &channelCount;
        m_levels.resize(channelCount);

        auto graph = ma_engine_get_node_graph(m_engine);
        if (ma_node_init(graph, &nodeConfig, nullptr, &m_levelMonitor->base) != MA_SUCCESS)
        {
            delete m_levelMonitor;
            m_levelMonitor = nullptr;
            return;
        }
        if (ma_node_attach_output_bus(&m_levelMonitor->base,
                                      0,
                                      ma_node_graph_get_endpoint(graph),
                                      0) != MA_SUCCESS)
        {
            ma_node_uninit(&m_levelMonitor->base, nullptr);
            delete m_levelMonitor;
            m_levelMonitor = nullptr;
            return;
        }
    }
}

void AudioEngine::levels(Span<float> levels)
{
    int size = std::min((int)m_levels.size(), (int)levels.size());
    for (int i = 0; i < size; i++)
    {
        levels[i] = m_levels[i];
        m_levels[i] = 0.0f;
    }
}

float AudioEngine::level(uint32_t channel)
{
    if (channel < m_levels.size())
    {
        float value = m_levels[channel];
        m_levels[channel] = 0.0f;
        return value;
    }
    return 0.0f;
}
#endif

void AudioEngine::start() { ma_engine_start(m_engine); }
void AudioEngine::stop() { ma_engine_stop(m_engine); }

rcp<AudioEngine> AudioEngine::Make(uint32_t numChannels, uint32_t sampleRate)
{
    ma_engine_config engineConfig = ma_engine_config_init();
    engineConfig.channels = numChannels;
    engineConfig.sampleRate = sampleRate;

#ifdef EXTERNAL_RIVE_AUDIO_ENGINE
    engineConfig.noDevice = MA_TRUE;
#endif

    ma_engine* engine = new ma_engine();

    if (ma_engine_init(&engineConfig, engine) != MA_SUCCESS)
    {
        fprintf(stderr, "AudioEngine::Make - failed to init engine\n");
        delete engine;
        return nullptr;
    }

    return rcp<AudioEngine>(new AudioEngine(engine));
}

uint32_t AudioEngine::channels() const { return ma_engine_get_channels(m_engine); }
uint32_t AudioEngine::sampleRate() const { return ma_engine_get_sample_rate(m_engine); }

AudioEngine::AudioEngine(ma_engine* engine) :
    m_device(ma_engine_get_device(engine)), m_engine(engine)
{}

rcp<AudioSound> AudioEngine::play(rcp<AudioSource> source,
                                  uint64_t startTime,
                                  uint64_t endTime,
                                  uint64_t soundStartTime,
                                  Artboard* artboard)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    // We have to dispose completed sounds out of the completed callback. So we
    // do it on next play or at destruct.
    for (auto sound : m_completedSounds)
    {
        sound->dispose();
    }
    m_completedSounds.clear();

    rcp<AudioSound> audioSound = rcp<AudioSound>(new AudioSound(this, source, artboard));
    if (source->isBuffered())
    {
        rive::Span<float> samples = source->bufferedSamples();
        ma_audio_buffer_config config =
            ma_audio_buffer_config_init(ma_format_f32,
                                        source->channels(),
                                        samples.size() / source->channels(),
                                        (const void*)samples.data(),
                                        nullptr);
        if (ma_audio_buffer_init(&config, audioSound->buffer()) != MA_SUCCESS)
        {
            fprintf(stderr, "AudioSource::play - Failed to initialize audio buffer.\n");
            return nullptr;
        }
        if (ma_sound_init_from_data_source(m_engine,
                                           audioSound->buffer(),
                                           MA_SOUND_FLAG_NO_PITCH | MA_SOUND_FLAG_NO_SPATIALIZATION,
                                           nullptr,
                                           audioSound->sound()) != MA_SUCCESS)
        {
            return nullptr;
        }
    }
    else
    {
        ma_decoder_config config = ma_decoder_config_init(ma_format_f32, channels(), sampleRate());
        auto sourceBytes = source->bytes();
        if (ma_decoder_init_memory(sourceBytes.data(),
                                   sourceBytes.size(),
                                   &config,
                                   audioSound->decoder()) != MA_SUCCESS)
        {
            fprintf(stderr, "AudioSource::play - Failed to initialize decoder.\n");
            return nullptr;
        }
        if (ma_sound_init_from_data_source(m_engine,
                                           &audioSound->m_decoder,
                                           MA_SOUND_FLAG_NO_PITCH | MA_SOUND_FLAG_NO_SPATIALIZATION,
                                           nullptr,
                                           audioSound->sound()) != MA_SUCCESS)
        {
            return nullptr;
        }
    }

    if (soundStartTime != 0)
    {
        audioSound->seek(soundStartTime);
    }

    ma_sound_set_end_callback(audioSound->sound(), SoundCompleted, audioSound.get());

    if (startTime != 0)
    {
        ma_sound_set_start_time_in_pcm_frames(audioSound->sound(), startTime);
    }
    if (endTime != 0)
    {
        ma_sound_set_stop_time_in_pcm_frames(audioSound->sound(), endTime);
    }
#ifdef WITH_RIVE_AUDIO_TOOLS
    if (m_levelMonitor != nullptr)
    {
        ma_node_attach_output_bus(audioSound->sound(), 0, m_levelMonitor, 0);
    }
#endif
    if (ma_sound_start(audioSound->sound()) != MA_SUCCESS)
    {
        fprintf(stderr, "AudioSource::play - failed to start sound\n");
        return nullptr;
    }

    if (m_playingSoundsHead != nullptr)
    {
        m_playingSoundsHead->m_prevPlaying = audioSound;
    }
    audioSound->m_nextPlaying = m_playingSoundsHead;
    m_playingSoundsHead = audioSound;

    return audioSound;
}

#ifdef TESTING
size_t AudioEngine::playingSoundCount()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    size_t count = 0;
    auto sound = m_playingSoundsHead;
    while (sound != nullptr)
    {
        count++;
        sound = sound->m_nextPlaying;
    }

    return count;
}
#endif

void AudioEngine::stop(Artboard* artboard)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    auto sound = m_playingSoundsHead;
    while (sound != nullptr)
    {
        auto next = sound->m_nextPlaying;
        if (sound->m_artboard == artboard)
        {
            sound->stop();
            m_completedSounds.push_back(sound);
            unlinkSound(sound);
        }
        sound = next;
    }
}

AudioEngine::~AudioEngine()
{
    auto sound = m_playingSoundsHead;
    while (sound != nullptr)
    {
        sound->dispose();

        auto next = sound->m_nextPlaying;
        sound->m_nextPlaying = nullptr;
        sound->m_prevPlaying = nullptr;
        sound = next;
    }

    for (auto sound : m_completedSounds)
    {
        sound->dispose();
    }
    m_completedSounds.clear();

    ma_engine_uninit(m_engine);
    delete m_engine;

#ifdef WITH_RIVE_AUDIO_TOOLS
    if (m_levelMonitor != nullptr)
    {
        ma_node_uninit(&m_levelMonitor->base, nullptr);
        delete m_levelMonitor;
    }

#endif
}

uint64_t AudioEngine::timeInFrames()
{
    return (uint64_t)ma_engine_get_time_in_pcm_frames(m_engine);
}

static rcp<AudioEngine> m_runtimeAudioEngine;
rcp<AudioEngine> AudioEngine::RuntimeEngine(bool makeWhenNecessary)
{
    if (!makeWhenNecessary)
    {
        return m_runtimeAudioEngine;
    }
    else if (m_runtimeAudioEngine == nullptr)
    {
        m_runtimeAudioEngine = AudioEngine::Make(defaultNumChannels, defaultSampleRate);
    }
    return m_runtimeAudioEngine;
}

#ifdef EXTERNAL_RIVE_AUDIO_ENGINE
bool AudioEngine::readAudioFrames(float* frames, uint64_t numFrames, uint64_t* framesRead)
{
    return ma_engine_read_pcm_frames(m_engine,
                                     (void*)frames,
                                     (ma_uint64)numFrames,
                                     (ma_uint64*)framesRead) == MA_SUCCESS;
}
bool AudioEngine::sumAudioFrames(float* frames, uint64_t numFrames)
{
    size_t numChannels = (size_t)channels();
    size_t count = (size_t)numFrames * numChannels;

    if (m_readFrames.size() < count)
    {
        m_readFrames.resize(count);
    }
    ma_uint64 framesRead = 0;
    if (ma_engine_read_pcm_frames(m_engine,
                                  (void*)m_readFrames.data(),
                                  (ma_uint64)numFrames,
                                  &framesRead) != MA_SUCCESS)
    {
        return false;
    }

    count = framesRead * numChannels;

    const size_t alignedCount = count - count % 4;
    float* src = m_readFrames.data();
    float* dst = frames;
    float* srcEnd = src + alignedCount;

    while (src != srcEnd)
    {
        float4 sum = simd::load4f(src) + simd::load4f(dst);
        simd::store(dst, sum);

        src += 4;
        dst += 4;
    }
    for (size_t i = alignedCount; i < count; i++)
    {
        frames[i] += m_readFrames[i];
    }
    return true;
}
#endif

#endif