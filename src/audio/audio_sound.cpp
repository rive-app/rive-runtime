#ifdef WITH_RIVE_AUDIO
#include "rive/audio/audio_sound.hpp"
#include "rive/audio/audio_engine.hpp"
#include "rive/audio/audio_reader.hpp"
#include "rive/audio/audio_source.hpp"

using namespace rive;

AudioSound::AudioSound(rcp<AudioEngine> engine) :
    m_engine(std::move(engine)), m_decoder({}), m_buffer({}), m_sound({})
{}

AudioSound::~AudioSound()
{
    ma_sound_uninit(&m_sound);
    ma_decoder_uninit(&m_decoder);
    ma_audio_buffer_uninit(&m_buffer);
}

void AudioSound::stop(uint64_t fadeTimeInFrames)
{
    if (fadeTimeInFrames == 0)
    {
        ma_sound_stop(&m_sound);
    }
    else
    {
        ma_sound_stop_with_fade_in_pcm_frames(&m_sound, fadeTimeInFrames);
    }
}

void AudioSound::complete()
{
    auto sound = rcp<AudioSound>(this);
    sound->ref();
    m_engine->completeSound(sound);
}

bool AudioSound::seek(uint64_t timeInFrames)
{
    return ma_sound_seek_to_pcm_frame(&m_sound, (ma_uint64)timeInFrames) == MA_SUCCESS;
}

#endif