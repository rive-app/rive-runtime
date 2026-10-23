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
typedef struct ma_context ma_context;

namespace rive
{
class AudioSound;
class AudioSource;
class LevelsNode;
class AudioRendererNode;
class Artboard;

/// A source the engine pulls blocks from on the audio thread and mixes with
/// its sounds.
class AudioRenderer
{
public:
  /// Writes frames of interleaved output starting at startFrame on the
  /// engine's clock.
  virtual void render(float* out, uint32_t frames, uint64_t startFrame) = 0;
  virtual ~AudioRenderer() = default;
};

class AudioEngine : public RefCnt<AudioEngine>
{
  friend class AudioSound;
  friend class AudioSource;
  friend class LevelsNode;

public:
  static const uint32_t defaultNumChannels = 2;
  static const uint32_t defaultSampleRate = 48000;

  /// A rate of 0 takes the device's own, or defaultSampleRate with no device.
  static rcp<AudioEngine> Make(uint32_t numChannels, uint32_t sampleRate);

  static rcp<AudioEngine> MakeAndStore(
      uint32_t numChannels,
      uint32_t sampleRate);

  /// The device buffer in frames engines open with, 0 for the default. Set
  /// after an engine opens, it applies the next time one is made.
  static void SetBufferFrames(uint32_t frames);

  ma_device* device() { return m_device; }
  ma_engine* engine() { return m_engine; }

  uint32_t channels() const;
  uint32_t sampleRate() const;
  /// The frames the engine mixes at a time. A device that rounds or clamps
  /// the buffer set, or runs at another rate, makes this differ from it.
  uint32_t bufferFrames() const;
  uint64_t timeInFrames();
  float timeInSeconds();

  ~AudioEngine();
  rcp<AudioSound> play(
      rcp<AudioSource> source,
      uint64_t startTime,
      uint64_t endTime,
      uint64_t soundStartTime,
      Artboard* artboard = nullptr);
  rcp<AudioSound> playSeconds(
      rcp<AudioSource> source,
      float startTime,
      uint64_t endTime,
      uint64_t soundStartTime,
      Artboard* artboard = nullptr);

  static rcp<AudioEngine> RuntimeEngine(bool makeWhenNecessary = true);
  static void ReleaseRuntimeEngine();

  /// Mixes renderer in until detachRenderer, which the renderer outlives.
  AudioRendererNode* attachRenderer(AudioRenderer* renderer);
  /// Returns once the audio thread has left the renderer.
  void detachRenderer(AudioRendererNode* node);

#if defined(EXTERNAL_RIVE_AUDIO_ENGINE) || defined(WITH_RIVE_TOOLS)
  bool readAudioFrames(
      float* frames,
      uint64_t numFrames,
      uint64_t* framesRead = nullptr);
#endif
#ifdef EXTERNAL_RIVE_AUDIO_ENGINE
  bool sumAudioFrames(float* frames, uint64_t numFrames);
#endif

#ifdef WITH_RIVE_AUDIO_TOOLS
  void initLevelMonitor();
  void levels(Span<float> levels);
  float level(uint32_t channel);
#endif

  void start();
  void stop();
  void stop(Artboard* artboard);

#ifdef TESTING
  size_t playingSoundCount();
  rcp<AudioSound> playingSoundsHead();
#endif
private:
  AudioEngine(ma_engine* engine, ma_context* context);
  ma_device* m_device;
  ma_engine* m_engine;
  ma_context* m_context;
  std::mutex m_mutex;
  rcp<AudioSound> internalPlaySound(
      rcp<AudioSource> source,
      uint64_t duration,
      uint64_t endTime,
      uint64_t soundStartTime,
      Artboard* artboard = nullptr);

  void soundCompleted(rcp<AudioSound> sound);
  /// Mixes node into the output, through the level monitor when there is
  /// one.
  bool attachToMix(ma_node_base* node);
  void unlinkSound(rcp<AudioSound> sound);
  rcp<AudioSound> initializeAudioSound(rcp<AudioSound>);

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