#ifdef WITH_RIVE_AUDIO
#include "rive/audio/audio_sound.hpp"
#include "rive/audio/audio_engine.hpp"
#include "rive/audio/audio_reader.hpp"
#include "rive/audio/audio_source.hpp"

using namespace rive;

AudioSound::AudioSound(AudioEngine* engine, rcp<AudioSource> source, Artboard* artboard) :
    m_decoder({}),
    m_buffer({}),
    m_sound({}),
    m_source(std::move(source)),
    m_isDisposed(false),
    m_engine(engine),
    m_artboard(artboard)
{}

void AudioSound::dispose()
{
    if (m_isDisposed)
    {
        return;
    }
    m_isDisposed = true;
    ma_sound_uninit(&m_sound);
    ma_decoder_uninit(&m_decoder.decoder);
    ma_audio_buffer_uninit(&m_buffer);
}

float AudioSound::volume() { return ma_sound_get_volume(&m_sound); }
void AudioSound::volume(float value) { ma_sound_set_volume(&m_sound, value); }

bool AudioSound::completed() const
{
    if (m_isDisposed)
    {
        return true;
    }
    return (bool)ma_sound_at_end(&m_sound);
}

AudioSound::~AudioSound() { dispose(); }

void AudioSound::stop(uint64_t fadeTimeInFrames)
{
    if (m_isDisposed)
    {
        return;
    }
    if (fadeTimeInFrames == 0)
    {
        ma_sound_stop(&m_sound);
    }
    else
    {
        ma_sound_stop_with_fade_in_pcm_frames(&m_sound, fadeTimeInFrames);
    }
}

bool AudioSound::seek(uint64_t timeInFrames)
{
    if (m_isDisposed)
    {
        return false;
    }
    return ma_sound_seek_to_pcm_frame(&m_sound, (ma_uint64)timeInFrames) == MA_SUCCESS;
}

#endif