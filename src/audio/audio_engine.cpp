#ifdef WITH_RIVE_AUDIO
#include "rive/math/simd.hpp"
#include "miniaudio.h"

#include "rive/audio/audio_engine.hpp"
#include "rive/audio/audio_sound.hpp"
#include "rive/audio/audio_source.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>

using namespace rive;

static std::atomic<uint32_t> s_bufferFrames{0};

void AudioEngine::SetBufferFrames(uint32_t frames) { s_bufferFrames = frames; }

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
  static void measureLevels(
      ma_node* pNode,
      const float** ppFramesIn,
      ma_uint32* pFrameCountIn,
      float** ppFramesOut,
      ma_uint32* pFrameCountOut)
  {
    const float* frames = ppFramesIn[0];

    ma_uint32 frameCount = pFrameCountIn[0];

    static_cast<LevelsNode*>(pNode)->engine->measureLevels(
        frames,
        (uint32_t)frameCount);
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

static ma_node_vtable measure_levels_vtable =
    {LevelsNode::measureLevels, nullptr, 1, 1, MA_NODE_FLAG_PASSTHROUGH};

void AudioEngine::initLevelMonitor()
{
  if (m_levelMonitor == nullptr)
  {
    auto* monitor = new LevelsNode();
    monitor->engine = this;

    ma_node_config nodeConfig = ma_node_config_init();
    nodeConfig.vtable = &measure_levels_vtable;
    uint32_t channelCount = channels();
    nodeConfig.pInputChannels = &channelCount;
    nodeConfig.pOutputChannels = &channelCount;
    m_levels.resize(channelCount);

    auto graph = ma_engine_get_node_graph(m_engine);
    if (ma_node_init(graph, &nodeConfig, nullptr, &monitor->base) != MA_SUCCESS)
    {
      delete monitor;
      return;
    }
    if (!attachToMix(&monitor->base))
    {
      ma_node_uninit(&monitor->base, nullptr);
      delete monitor;
      return;
    }
    m_levelMonitor = monitor;
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

#ifndef MA_NO_DEVICE_IO
// A callback that outlasts a block mixes two in the time of one, so blocks are
// fitted to the period the device chose.
static bool fitBlocksToDevice(ma_engine_config& config, const ma_device& device)
{
  uint32_t period = device.playback.internalPeriodSizeInFrames;
  uint32_t rate = device.playback.internalSampleRate;
  if (rate == device.sampleRate)
  {
    if (period == config.periodSizeInFrames)
    {
      return false;
    }
    config.periodSizeInFrames = period;
    return true;
  }
  // Only an engine given a rate gets here. Frames last different times at two
  // rates, so both get a time: the one asked for, or the period if shorter.
  auto milliseconds = [](uint64_t frames, uint64_t perSecond) {
    return (frames * 1000 + perSecond - 1) / perSecond;
  };
  config.periodSizeInMilliseconds = (uint32_t)std::min(
      milliseconds(config.periodSizeInFrames, device.sampleRate),
      milliseconds(period, rate));
  config.periodSizeInFrames = 0;
  return true;
}
#endif

rcp<AudioEngine> AudioEngine::Make(uint32_t numChannels, uint32_t sampleRate)
{
// I _think_ MA_NO_DEVICE_IO is defined when building for Unity; otherwise, it
// seems to pass "standard" building When defined, pContext is unavailable,
// which causes build errors when building Unity for iOS. - David
#if (TARGET_IPHONE_SIMULATOR || TARGET_OS_MACCATALYST || TARGET_OS_IPHONE) &&  \
    !defined(MA_NO_DEVICE_IO)
  // Used for configuration only, and isn't referenced past the usage of
  // ma_context_init; thus, can be locally scoped. Uses the "logical" defaults
  // from miniaudio, and updates only what we need. This should automatically
  // set available backends in priority order based on the target it's built
  // for, which in the case of Apple is Core Audio first.
  ma_context_config contextConfig = ma_context_config_init();

  // By setting the core audio session to none, miniaudio will not
  // - set a (new) category on the shared audio session
  // - set any (new) options when setting a new category
  // This means that the shared AVAudioSession instance will be respected
  // when audio is played; the developer will have to set up the shared
  // AVAudioSession instance explicitly for their own set up.
  // This does not touch whether the session is made (in)active.
  contextConfig.coreaudio.sessionCategory = ma_ios_session_category_none;

  // We only need to initialize space for the context if we're targeting Apple
  // platforms
  ma_context* context = (ma_context*)malloc(sizeof(ma_context));

  if (ma_context_init(NULL, 0, &contextConfig, context) != MA_SUCCESS)
  {
    free(context);
    context = nullptr;
  }
#else
  ma_context* context = nullptr;
#endif

  ma_engine_config engineConfig = ma_engine_config_init();
  engineConfig.channels = numChannels;

#if (TARGET_IPHONE_SIMULATOR || TARGET_OS_MACCATALYST || TARGET_OS_IPHONE) &&  \
    !defined(MA_NO_DEVICE_IO)
  if (context != nullptr)
  {
    engineConfig.pContext = context;
  }
#endif

#ifdef EXTERNAL_RIVE_AUDIO_ENGINE
  engineConfig.noDevice = MA_TRUE;
#else
  // CI VMs can stall for minutes opening a CoreAudio device.
  if (std::getenv("RIVE_NO_AUDIO_DEVICE") != nullptr)
  {
    engineConfig.noDevice = MA_TRUE;
  }
#endif
  engineConfig.sampleRate = sampleRate == 0 && engineConfig.noDevice
                                ? defaultSampleRate
                                : sampleRate;
  uint32_t buffer = s_bufferFrames;
  // Without a fixed block miniaudio can start a scheduled sound a read late.
  // A host reading partial blocks hears sounds played now up to a block late.
  engineConfig.periodSizeInFrames = buffer == 0 && engineConfig.noDevice
                                        ? 128
                                        : buffer;

  ma_engine* engine = new ma_engine();
  ma_result result = ma_engine_init(&engineConfig, engine);
#ifndef MA_NO_DEVICE_IO
  ma_device* device = result == MA_SUCCESS ? ma_engine_get_device(engine)
                                           : nullptr;
  if (device != nullptr && engineConfig.periodSizeInFrames != 0 &&
      fitBlocksToDevice(engineConfig, *device))
  {
    ma_engine_uninit(engine);
    result = ma_engine_init(&engineConfig, engine);
  }
#endif

  if (result != MA_SUCCESS)
  {
#if (TARGET_IPHONE_SIMULATOR || TARGET_OS_MACCATALYST || TARGET_OS_IPHONE) &&  \
    !defined(MA_NO_DEVICE_IO)
    if (context != nullptr)
    {
      ma_context_uninit(context);
      free(context);
      context = nullptr;
    }
#endif
    fprintf(stderr, "AudioEngine::Make - failed to init engine\n");
    delete engine;
    return nullptr;
  }

  return rcp<AudioEngine>(new AudioEngine(engine, context));
}

uint32_t AudioEngine::channels() const
{
  return ma_engine_get_channels(m_engine);
}
uint32_t AudioEngine::sampleRate() const
{
  return ma_engine_get_sample_rate(m_engine);
}
uint32_t AudioEngine::bufferFrames() const
{
  return ma_engine_get_node_graph(m_engine)->processingSizeInFrames;
}

AudioEngine::AudioEngine(ma_engine* engine, ma_context* context)
    : m_device(ma_engine_get_device(engine))
    , m_engine(engine)
    , m_context(context)
{}

rcp<AudioSound> AudioEngine::internalPlaySound(
    rcp<AudioSource> source,
    uint64_t duration,
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

  rcp<AudioSound> audioSound = rcp<AudioSound>(
      new AudioSound(this, source, artboard));
  if (source->isBuffered())
  {
    rive::Span<float> samples = source->bufferedSamples();
    ma_uint64 sizeInFrames = samples.size() / source->channels();
    if (endTime != 0)
    {
      float durationSeconds = duration / (float)sampleRate();
      ma_uint64 clippedFrames = (ma_uint64)std::round(
          durationSeconds * source->sampleRate());
      if (clippedFrames < sizeInFrames)
      {
        sizeInFrames = clippedFrames;
      }
    }
    ma_audio_buffer_config config = ma_audio_buffer_config_init(
        ma_format_f32,
        source->channels(),
        sizeInFrames,
        (const void*)samples.data(),
        nullptr);
    if (ma_audio_buffer_init(&config, audioSound->buffer()) != MA_SUCCESS)
    {
      fprintf(
          stderr,
          "AudioSource::play - Failed to initialize audio buffer.\n");
      return nullptr;
    }
    if (ma_sound_init_from_data_source(
            m_engine,
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
    // We wrapped the miniaudio decoder with a custom data source "Clipped
    // Decoder" which lets us ensure that the end callback for the sound is
    // called when we reach the end of the clip. This won't happen when
    // using ma_sound_set_stop_time_in_pcm_frames(audioSound->sound(),
    // endTime); as this keeps the sound playing/ready to fade back in.
    auto clip = audioSound->clippedDecoder();
    ma_decoder_config config = ma_decoder_config_init(
        ma_format_f32,
        channels(),
        sampleRate());
    auto sourceBytes = source->bytes();
    if (ma_decoder_init_memory(
            sourceBytes.data(),
            sourceBytes.size(),
            &config,
            &clip->decoder) != MA_SUCCESS)
    {
      fprintf(stderr, "AudioSource::play - Failed to initialize decoder.\n");
      return nullptr;
    }
    clip->frameCursor = 0;
    clip->endFrame = endTime == 0 ? std::numeric_limits<uint64_t>::max()
                                  : duration;
    ma_data_source_config baseConfig = ma_data_source_config_init();
    baseConfig.vtable = &g_ma_end_clipped_decoder_vtable;
    if (ma_data_source_init(&baseConfig, &clip->base) != MA_SUCCESS)
    {
      return nullptr;
    }

    if (ma_sound_init_from_data_source(
            m_engine,
            audioSound->clippedDecoder(),
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

  ma_sound_set_end_callback(
      audioSound->sound(),
      SoundCompleted,
      audioSound.get());
  return audioSound;
}

rcp<AudioSound> AudioEngine::initializeAudioSound(rcp<AudioSound> audioSound)
{
#ifdef WITH_RIVE_AUDIO_TOOLS
  if (m_levelMonitor != nullptr)
  {
    ma_node_attach_output_bus(audioSound->sound(), 0, m_levelMonitor, 0);
  }
#endif
  if (ma_sound_start(audioSound->sound()) != MA_SUCCESS)
  {
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

rcp<AudioSound> AudioEngine::playSeconds(
    rcp<AudioSource> source,
    float startTime,
    uint64_t endTime,
    uint64_t soundStartTime,
    Artboard* artboard)
{

  if (endTime != 0 && startTime >= (float)endTime)
  {
    // Requested to stop sound before start.
    return nullptr;
  }
  auto audioSound = internalPlaySound(
      source,
      (int)(soundStartTime + endTime - startTime),
      endTime,
      soundStartTime,
      artboard);
  if (audioSound == nullptr)
  {
    return nullptr;
  }
  if (startTime != 0)
  {
    ma_sound_set_start_time_in_milliseconds(
        audioSound->sound(),
        (ma_uint64)std::round(startTime * 1000.0));
  }
  return initializeAudioSound(audioSound);
}

rcp<AudioSound> AudioEngine::play(
    rcp<AudioSource> source,
    uint64_t startTime,
    uint64_t endTime,
    uint64_t soundStartTime,
    Artboard* artboard)
{
  if (endTime != 0 && startTime >= endTime)
  {
    // Requested to stop sound before start.
    return nullptr;
  }
  auto audioSound = internalPlaySound(
      source,
      soundStartTime + endTime - startTime,
      endTime,
      soundStartTime,
      artboard);

  if (audioSound == nullptr)
  {
    return nullptr;
  }
  if (startTime != 0)
  {
    ma_sound_set_start_time_in_pcm_frames(audioSound->sound(), startTime);
  }
  return initializeAudioSound(audioSound);
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

rcp<AudioSound> AudioEngine::playingSoundsHead() { return m_playingSoundsHead; }

#endif

void AudioEngine::stop(Artboard* artboard)
{
  std::unique_lock<std::mutex> lock(m_mutex);
  auto sound = m_playingSoundsHead;
  while (sound != nullptr)
  {
    auto next = sound->m_nextPlaying;
    if (sound->m_artboard == artboard || artboard == nullptr)
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

#if (TARGET_IPHONE_SIMULATOR || TARGET_OS_MACCATALYST || TARGET_OS_IPHONE) &&  \
    !defined(MA_NO_DEVICE_IO)
  // m_context is only set when Core Audio is available
  if (m_context != nullptr)
  {
    ma_context_uninit(m_context);
    free(m_context);
    m_context = nullptr;
  }
#endif

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

float AudioEngine::timeInSeconds()
{
  return ma_engine_get_time_in_milliseconds(m_engine) / 1000.0f;
}

static rcp<AudioEngine> m_runtimeAudioEngine;

rcp<AudioEngine> AudioEngine::MakeAndStore(
    uint32_t numChannels,
    uint32_t sampleRate)
{
  auto audioEngine = Make(numChannels, sampleRate);
#ifdef WITH_RIVE_TOOLS
  if (m_runtimeAudioEngine)
  {
    m_runtimeAudioEngine->stop(nullptr);
  }
#endif
  m_runtimeAudioEngine = audioEngine;
  return m_runtimeAudioEngine;
}

rcp<AudioEngine> AudioEngine::RuntimeEngine(bool makeWhenNecessary)
{
  if (!makeWhenNecessary)
  {
    return m_runtimeAudioEngine;
  }
  else if (m_runtimeAudioEngine == nullptr)
  {
    // A browser's audio context runs at the rate it is given, so blocks fit.
#ifdef __EMSCRIPTEN__
    uint32_t sampleRate = defaultSampleRate;
#else
    uint32_t sampleRate = 0;
#endif
    m_runtimeAudioEngine = AudioEngine::Make(defaultNumChannels, sampleRate);
  }
  return m_runtimeAudioEngine;
}

void AudioEngine::ReleaseRuntimeEngine() { m_runtimeAudioEngine = nullptr; }

namespace rive
{
class AudioRendererNode
{
public:
  ma_node_base base;
  AudioRenderer* renderer;
  // The engine frame its next block starts at. A renderer attached during a
  // mix first plays in the next one, so the clock is read at its first block.
  bool started = false;
  uint64_t nextFrame = 0;

  static void process(
      ma_node* node,
      const float**,
      ma_uint32*,
      float** out,
      ma_uint32* frames)
  {
    auto* self = static_cast<AudioRendererNode*>(node);
    if (!self->started)
    {
      self->nextFrame = ma_node_graph_get_time(ma_node_get_node_graph(node));
      self->started = true;
    }
    self->renderer->render(out[0], *frames, self->nextFrame);
    self->nextFrame += *frames;
  }
};
} // namespace rive

bool AudioEngine::attachToMix(ma_node_base* node)
{
  ma_node* mix = ma_node_graph_get_endpoint(ma_engine_get_node_graph(m_engine));
#ifdef WITH_RIVE_AUDIO_TOOLS
  // Editor meters read what passes through the monitor.
  if (m_levelMonitor != nullptr)
  {
    mix = &m_levelMonitor->base;
  }
#endif
  return ma_node_attach_output_bus(node, 0, mix, 0) == MA_SUCCESS;
}

static ma_node_vtable renderer_vtable =
    {AudioRendererNode::process, nullptr, 0, 1, 0};

AudioRendererNode* AudioEngine::attachRenderer(AudioRenderer* renderer)
{
  auto graph = ma_engine_get_node_graph(m_engine);
  auto* node = new AudioRendererNode();
  node->renderer = renderer;
  ma_node_config config = ma_node_config_init();
  config.vtable = &renderer_vtable;
  uint32_t channelCount = channels();
  config.pOutputChannels = &channelCount;
  if (ma_node_init(graph, &config, nullptr, &node->base) != MA_SUCCESS)
  {
    delete node;
    return nullptr;
  }
  if (!attachToMix(&node->base))
  {
    ma_node_uninit(&node->base, nullptr);
    delete node;
    return nullptr;
  }
  return node;
}

void AudioEngine::detachRenderer(AudioRendererNode* node)
{
  if (node != nullptr)
  {
    ma_node_uninit(&node->base, nullptr);
    delete node;
  }
}

#if defined(EXTERNAL_RIVE_AUDIO_ENGINE) || defined(WITH_RIVE_TOOLS)
bool AudioEngine::readAudioFrames(
    float* frames,
    uint64_t numFrames,
    uint64_t* framesRead)
{
  return ma_engine_read_pcm_frames(
             m_engine,
             (void*)frames,
             (ma_uint64)numFrames,
             (ma_uint64*)framesRead) == MA_SUCCESS;
}
#endif

#ifdef EXTERNAL_RIVE_AUDIO_ENGINE
bool AudioEngine::sumAudioFrames(float* frames, uint64_t numFrames)
{
  size_t numChannels = (size_t)channels();
  size_t count = (size_t)numFrames * numChannels;

  if (m_readFrames.size() < count)
  {
    m_readFrames.resize(count);
  }
  ma_uint64 framesRead = 0;
  if (ma_engine_read_pcm_frames(
          m_engine,
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