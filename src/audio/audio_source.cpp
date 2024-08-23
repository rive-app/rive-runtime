#include "rive/audio/audio_source.hpp"
#ifdef WITH_RIVE_AUDIO
#include "rive/audio/audio_engine.hpp"
#include "rive/audio/audio_sound.hpp"
#include "rive/audio/audio_reader.hpp"
#include "rive/audio/audio_reader.hpp"
#endif

using namespace rive;

#ifdef WITH_RIVE_AUDIO
AudioSource::AudioSource(rive::Span<float> samples, uint32_t numChannels, uint32_t sampleRate) :
    m_isBuffered(true),
    m_channels(numChannels),
    m_sampleRate(sampleRate),
    m_ownedBytes((uint8_t*)samples.data(), samples.size() * sizeof(float))
{
    assert(numChannels != 0);
    assert(sampleRate != 0);
}

AudioSource::AudioSource(rive::Span<uint8_t> fileBytes) :
    m_isBuffered(false), m_channels(0), m_sampleRate(0), m_fileBytes(fileBytes)
{}

AudioSource::AudioSource(rive::SimpleArray<uint8_t> fileBytes) :
    m_isBuffered(false),
    m_channels(0),
    m_sampleRate(0),
    m_fileBytes(fileBytes.data(), fileBytes.size()),
    m_ownedBytes(std::move(fileBytes))
{}

const rive::Span<float> AudioSource::bufferedSamples() const
{
    assert(m_isBuffered);
    return rive::Span<float>((float*)m_ownedBytes.data(), m_ownedBytes.size() / sizeof(float));
}

class AudioSourceDecoder
{
public:
    AudioSourceDecoder(rive::Span<uint8_t> fileBytes) : m_decoder({})
    {
        ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
        if (ma_decoder_init_memory(fileBytes.data(), fileBytes.size(), &config, &m_decoder) !=
            MA_SUCCESS)
        {
            fprintf(stderr, "AudioSourceDecoder - Failed to initialize decoder.\n");
        }
    }

    ~AudioSourceDecoder() { ma_decoder_uninit(&m_decoder); }

    uint32_t channels() { return (uint32_t)m_decoder.outputChannels; }

    uint32_t sampleRate() { return (uint32_t)m_decoder.outputSampleRate; }

private:
    ma_decoder m_decoder;
};

uint32_t AudioSource::channels()
{
    if (m_channels != 0)
    {
        return m_channels;
    }
    AudioSourceDecoder audioDecoder(m_fileBytes);
    return m_channels = audioDecoder.channels();
}

uint32_t AudioSource::sampleRate()
{
    if (m_sampleRate != 0)
    {
        return m_sampleRate;
    }
    AudioSourceDecoder audioDecoder(m_fileBytes);
    return m_sampleRate = audioDecoder.sampleRate();
}

AudioFormat AudioSource::format() const
{
    if (m_isBuffered)
    {
        return AudioFormat::buffered;
    }
    ma_decoder decoder;
    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
    if (ma_decoder_init_memory(m_fileBytes.data(), m_fileBytes.size(), &config, &decoder) !=
        MA_SUCCESS)
    {
        fprintf(stderr, "AudioSource::format - Failed to initialize decoder.\n");
        return AudioFormat::unknown;
    }
    ma_encoding_format encodingFormat;

    AudioFormat format = AudioFormat::unknown;
    if (ma_decoder_get_encoding_format(&decoder, &encodingFormat) == MA_SUCCESS)
    {
        switch (encodingFormat)
        {
            case ma_encoding_format_mp3:
                format = AudioFormat::mp3;
                break;
            case ma_encoding_format_wav:
                format = AudioFormat::wav;
                break;
            case ma_encoding_format_vorbis:
                format = AudioFormat::vorbis;
                break;
            case ma_encoding_format_flac:
                format = AudioFormat::flac;
                break;
            default:
                format = AudioFormat::unknown;
                break;
        }
    }

    ma_decoder_uninit(&decoder);

    return format;
}

rcp<AudioReader> AudioSource::makeReader(uint32_t numChannels, uint32_t sampleRate)
{
    if (m_isBuffered)
    {
        return nullptr;
    }

    rive::rcp<rive::AudioSource> rcSource = rive::rcp<rive::AudioSource>(this);
    rcSource->ref();
    auto reader = rcp<AudioReader>(new AudioReader(rcSource, numChannels));

    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, numChannels, sampleRate);

    if (ma_decoder_init_memory(m_fileBytes.data(),
                               m_fileBytes.size(),
                               &config,
                               reader->decoder()) != MA_SUCCESS)
    {
        fprintf(stderr, "AudioSource::makeReader - Failed to initialize decoder.\n");
        return nullptr;
    }

    return reader;
}
#else
AudioSource::AudioSource(rive::Span<uint8_t> fileBytes) {}
AudioSource::AudioSource(rive::SimpleArray<uint8_t> fileBytes) {}
AudioSource::AudioSource(rive::Span<float> samples, uint32_t numChannels, uint32_t sampleRate) {}
uint32_t AudioSource::channels() { return 0; }
uint32_t AudioSource::sampleRate() { return 0; }
AudioFormat AudioSource::format() const { return AudioFormat::unknown; }
const rive::Span<float> AudioSource::bufferedSamples() const
{
    return rive::Span<float>(nullptr, 0);
}
#endif