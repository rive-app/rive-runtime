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

using namespace rive;

void AudioEngine::SoundCompleted(void* pUserData, ma_sound* pSound)
{
    AudioSound* audioSound = (AudioSound*)pUserData;
    audioSound->complete();
}

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
                                  uint64_t soundStartTime)
{
    purgeCompletedSounds();

    rive::rcp<rive::AudioEngine> rcEngine = rive::rcp<rive::AudioEngine>(this);
    rcEngine->ref();
    rcp<AudioSound> audioSound = rcp<AudioSound>(new AudioSound(rcEngine));
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

    // one extra ref for sound as we're waiting for playback to complete.
    audioSound->ref();

    ma_sound_set_end_callback(audioSound->sound(), SoundCompleted, audioSound.get());

    if (startTime != 0)
    {
        ma_sound_set_start_time_in_pcm_frames(audioSound->sound(), startTime);
    }
    if (endTime != 0)
    {
        ma_sound_set_stop_time_in_pcm_frames(audioSound->sound(), endTime);
    }
    if (ma_sound_start(audioSound->sound()) != MA_SUCCESS)
    {
        fprintf(stderr, "AudioSource::play - failed to start sound\n");
        return nullptr;
    }

    return audioSound;
}

void AudioEngine::completeSound(rcp<AudioSound> sound) { m_completedSounds.push_back(sound); }

void AudioEngine::purgeCompletedSounds()
{
    for (auto sound : m_completedSounds)
    {
        sound->unref();
    }
    m_completedSounds.clear();
}

AudioEngine::~AudioEngine()
{
    ma_engine_uninit(m_engine);
    delete m_engine;
}

uint64_t AudioEngine::timeInFrames()
{
    return (uint64_t)ma_engine_get_time_in_pcm_frames(m_engine);
}

rcp<AudioEngine> AudioEngine::RuntimeEngine()
{
    static rcp<AudioEngine> engine = AudioEngine::Make(defaultNumChannels, defaultSampleRate);
    return engine;
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