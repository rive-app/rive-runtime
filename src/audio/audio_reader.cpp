#ifdef WITH_RIVE_AUDIO
#include "rive/audio/audio_reader.hpp"
#include "rive/audio/audio_source.hpp"
#include "rive/audio/audio_engine.hpp"

using namespace rive;

AudioReader::AudioReader(rcp<AudioSource> audioSource, uint32_t channels) :
    m_source(std::move(audioSource)), m_channels(channels), m_decoder({})
{}

AudioReader::~AudioReader() { ma_decoder_uninit(&m_decoder); }

uint32_t AudioReader::channels() const { return m_channels; }
uint32_t AudioReader::sampleRate() const { return m_decoder.outputSampleRate; }
ma_decoder* AudioReader::decoder() { return &m_decoder; }

uint64_t AudioReader::lengthInFrames()
{
    ma_uint64 length;

    ma_result result = ma_data_source_get_length_in_pcm_frames(&m_decoder, &length);
    if (result != MA_SUCCESS)
    {
        fprintf(stderr,
                "AudioReader::lengthInFrames - audioSourceLength failed to determine length\n");
        return 0;
    }
    return (uint64_t)length;
}

Span<float> AudioReader::read(uint64_t frameCount)
{
    m_readBuffer.resize(frameCount * m_channels);

    ma_uint64 framesRead;
    if (ma_data_source_read_pcm_frames(&m_decoder,
                                       m_readBuffer.data(),
                                       (ma_uint64)frameCount,
                                       &framesRead) != MA_SUCCESS)
    {
        return Span<float>(nullptr, 0);
    }
    return Span<float>(m_readBuffer.data(), framesRead * m_channels);
}
#endif