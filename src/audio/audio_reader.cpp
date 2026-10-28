#ifdef WITH_RIVE_AUDIO
#include "rive/audio/audio_reader.hpp"
#include "rive/audio/audio_source.hpp"
#include "rive/audio/audio_engine.hpp"
#include <algorithm>

using namespace rive;

static constexpr uint64_t kInputFrames = 4096;

AudioReader::AudioReader(rcp<AudioSource> audioSource)
    : m_source(std::move(audioSource))
    , m_decoder({})
    , m_converter({})
{}

AudioReader::~AudioReader()
{
  if (m_converting)
  {
    ma_data_converter_uninit(&m_converter, nullptr);
  }
  if (m_decoding)
  {
    ma_decoder_uninit(&m_decoder);
  }
}

uint32_t AudioReader::channels() const { return m_converter.channelsOut; }
uint32_t AudioReader::sampleRate() const { return m_converter.sampleRateOut; }

bool AudioReader::init(uint32_t channels, uint32_t sampleRate)
{
  uint32_t inChannels = m_decoding ? m_decoder.outputChannels
                                   : m_source->channels();
  uint32_t inRate = m_decoding ? m_decoder.outputSampleRate
                               : m_source->sampleRate();
  ma_data_converter_config config = ma_data_converter_config_init(
      ma_format_f32,
      ma_format_f32,
      inChannels,
      channels != 0 ? channels : inChannels,
      inRate,
      sampleRate != 0 ? sampleRate : inRate);
  // As a resampling decoder filters, rather than the converter's default.
  config.resampling.linear.lpfOrder = ma_decoder_config_init(
                                          ma_format_f32,
                                          0,
                                          0)
                                          .resampling.linear.lpfOrder;
  m_converting = ma_data_converter_init(&config, nullptr, &m_converter) ==
                 MA_SUCCESS;
  return m_converting;
}

bool AudioReader::inputLength(ma_uint64& length)
{
  if (!m_decoding)
  {
    length = m_source->bufferedSamples().size() / m_converter.channelsIn;
    return true;
  }
  return ma_data_source_get_length_in_pcm_frames(&m_decoder, &length) ==
         MA_SUCCESS;
}

uint64_t AudioReader::lengthInFrames()
{
  ma_uint64 length = 0;
  if (!inputLength(length))
  {
    fprintf(
        stderr,
        "AudioReader::lengthInFrames - audioSourceLength failed to "
        "determine length\n");
    return 0;
  }
  ma_uint64 expected = 0;
  ma_data_converter_get_expected_output_frame_count(
      &m_converter,
      length,
      &expected);
  return (uint64_t)expected;
}

Span<float> AudioReader::read(uint64_t frameCount)
{
  m_readBuffer.resize(frameCount * channels());

  uint64_t framesRead;
  if (!readInto(m_readBuffer.data(), frameCount, framesRead))
  {
    return Span<float>(nullptr, 0);
  }
  return Span<float>(m_readBuffer.data(), framesRead * channels());
}

bool AudioReader::nextInput()
{
  uint32_t inChannels = m_converter.channelsIn;
  if (!m_decoding)
  {
    auto samples = m_source->bufferedSamples();
    uint64_t total = samples.size() / inChannels;
    m_pending = samples.data() + m_inputFrame * inChannels;
    m_pendingFrames = total - m_inputFrame;
    m_inputFrame = total;
    return true;
  }
  m_input.resize(kInputFrames * inChannels);
  ma_uint64 read = 0;
  ma_result result = ma_data_source_read_pcm_frames(
      &m_decoder,
      m_input.data(),
      kInputFrames,
      &read);
  m_pending = m_input.data();
  m_pendingFrames = read;
  return result == MA_SUCCESS || result == MA_AT_END;
}

bool AudioReader::readInto(
    float* out,
    uint64_t frameCount,
    uint64_t& framesRead)
{
  framesRead = 0;
  while (framesRead < frameCount)
  {
    if (m_pendingFrames == 0)
    {
      if (!nextInput())
      {
        return false;
      }
      if (m_pendingFrames == 0)
      {
        return true;
      }
    }
    ma_uint64 in = m_pendingFrames;
    ma_uint64 made = frameCount - framesRead;
    if (ma_data_converter_process_pcm_frames(
            &m_converter,
            m_pending,
            &in,
            out + framesRead * channels(),
            &made) != MA_SUCCESS)
    {
      return false;
    }
    m_pending += in * m_converter.channelsIn;
    m_pendingFrames -= in;
    framesRead += made;
    if (in == 0 && made == 0)
    {
      return true;
    }
  }
  return true;
}

bool AudioReader::seekInput(uint64_t& frame)
{
  m_pendingFrames = 0;
  if (!m_decoding)
  {
    ma_uint64 length = 0;
    inputLength(length);
    m_inputFrame = frame = std::min<uint64_t>(frame, length);
    return true;
  }
  // Clamped only once it fails, as a decoder's length can cost a pass.
  if (ma_decoder_seek_to_pcm_frame(&m_decoder, frame) == MA_SUCCESS)
  {
    return true;
  }
  ma_uint64 length = 0;
  if (!inputLength(length) || frame < length ||
      ma_decoder_seek_to_pcm_frame(&m_decoder, length) != MA_SUCCESS)
  {
    return false;
  }
  frame = length;
  return true;
}

bool AudioReader::seek(uint64_t frame, uint64_t* landed)
{
  // In double, as frame times the source's rate can pass 64 bits.
  double rateIn = m_converter.sampleRateIn;
  double at = (double)frame * rateIn / sampleRate();
  uint64_t input = at < 1.8e19 ? (uint64_t)at : UINT64_MAX;
  if (!seekInput(input) || ma_data_converter_reset(&m_converter) != MA_SUCCESS)
  {
    return false;
  }
  if (landed != nullptr)
  {
    *landed = std::min(
        frame,
        (uint64_t)((double)input * sampleRate() / rateIn));
  }
  return true;
}

bool AudioReader::readBlock(
    uint64_t maxFrames,
    std::vector<float>& out,
    bool& done)
{
  uint32_t frameSamples = channels();
  if (out.empty())
  {
    out.reserve((size_t)std::min<uint64_t>(
        (std::min(lengthInFrames(), maxFrames) + kInputFrames) * frameSamples,
        SIZE_MAX / sizeof(float)));
  }
  size_t at = out.size();
  out.resize(at + kInputFrames * frameSamples);
  uint64_t read = 0;
  if (!readInto(out.data() + at, kInputFrames, read))
  {
    return false;
  }
  out.resize(at + read * frameSamples);
  // Sized by what decodes, as an MP3's length is only an estimate.
  done = read == 0 || out.size() / frameSamples > maxFrames;
  return true;
}

bool AudioReader::readAll(uint64_t maxFrames, std::vector<float>& out)
{
  out.clear();
  bool done = false;
  while (!done)
  {
    if (!readBlock(maxFrames, out, done))
    {
      out.clear();
      return false;
    }
  }
  return true;
}

bool AudioReader::rewind()
{
  uint64_t start = 0;
  return seekInput(start);
}
#endif
