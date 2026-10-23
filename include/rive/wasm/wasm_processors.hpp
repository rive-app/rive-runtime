#ifndef _RIVE_WASM_PROCESSORS_HPP_
#define _RIVE_WASM_PROCESSORS_HPP_

#ifdef WITH_RIVE_SCRIPTING_WASM

#include "rive/async/job_pool.hpp"
#include "rive/async/threading.hpp"
#include "rive/wasm/wasm_lane.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <vector>

// A worklet plays processors only from a shared heap, and only tools render
// them offline, so other web builds can only refuse them.
#if !RIVE_ONE_THREAD() || defined(WITH_RIVE_TOOLS)
#define RIVE_PROCESSOR_AUDIO() 1
#else
#define RIVE_PROCESSOR_AUDIO() 0
#endif

namespace rive
{
class AudioEngine;
class AudioRenderer;
class AudioRendererNode;

/// What a processor renders: interleaved frames at a sample rate, in blocks
/// of at most maxFrames.
struct ProcessorFormat
{
  uint32_t sampleRate = 48000;
  uint32_t channels = 2;
  uint32_t maxFrames = 1024;
};

/// A script's AudioProcessor: init runs on a thread of its own, then each
/// block on the audio thread, where overrunning hardBlocks of audio silences
/// only it.
class WasmProcessor : public JobTask, public WasmLane
{
public:
  WasmProcessor(
      rcp<WasmLaneModule> module,
      uint32_t entry,
      std::vector<uint8_t> input,
      ProcessorFormat format,
      uint32_t hardBlocks);
  ~WasmProcessor() override;

#if RIVE_PROCESSOR_AUDIO()
  /// Writes interleaved frames from startFrame on the engine's clock;
  /// silence before init has run and after a stop or a failure.
  void render(float* out, uint32_t frames, uint64_t startFrame);
#endif
  /// Leaves the mix; a render already running finishes its block.
  void stop() override;

  LaneState state() const override { return m_state.load(); }
  const ProcessorFormat& format() const { return m_format; }

  // JobTask: instantiates the module and runs init, which nothing joins.
  void run() override;

  static constexpr uint32_t kEventLog = lane_layout::kProcessorEventLog;

private:
  friend class WasmProcessors;
  bool startInstance();
#if RIVE_PROCESSOR_AUDIO()
  bool process(float* out, uint32_t frames, uint64_t startFrame);
#endif
  void endInstance();
  /// Moves a starting or running processor to state; a failed or stopped
  /// one stays as it is. False when it stayed.
  bool settle(LaneState state);

  ProcessorFormat m_format;
  uint32_t m_hardBlocks;
  std::atomic<LaneState> m_state{LaneState::starting};
#ifndef __EMSCRIPTEN__
  // A cancel ends an init still running.
  void interrupt() override { interruptCall(); }
  std::unique_ptr<WasmCallBudget> m_budget;
  bool m_warm = false;
  void* m_processFn = nullptr;
  void* m_collectFn = nullptr;
#elif RIVE_PROCESSOR_AUDIO()
  bool startOnPage();
  // The page runner's id for this processor's instance.
  uint32_t m_webInstance = 0;

public:
  /// Hands the processor to the page's audio worklet, which runs it in
  /// real time and reports back through event(); false when it refused.
  bool startRealtime();
  /// The worklet's word on the processor: the state it reached with its
  /// text, or kEventLog with a console line.
  void event(uint32_t kind, const char* text);

private:
  // The worklet's id for it, once handed over.
  uint32_t m_audioId = 0;
#endif
#if defined(WITH_RIVE_AUDIO) && !defined(__EMSCRIPTEN__)
  // The engine whose mix it joins after init. Attaching and detaching take
  // their own lock, since detaching waits for the audio thread.
  rcp<AudioEngine> m_engine;
  std::unique_ptr<AudioRenderer> m_renderer;
  std::mutex m_nodeMutex;
  AudioRendererNode* m_node = nullptr;
#endif
};

/// A VM's processors: the module carried in its main module's "rive.audio"
/// section, and every processor the VM started.
class WasmProcessors : public WasmLaneSet<WasmProcessor>
{
public:
  using WasmLaneSet::WasmLaneSet;
  /// Stops every processor.
  ~WasmProcessors();

  /// Starts a processor that joins the runtime engine's mix after init; null
  /// for an entry the module lacks, failed when nothing would play it.
  rcp<WasmProcessor> start(
      uint32_t entry,
      std::vector<uint8_t> input,
      uint32_t hardBlocks);
#if RIVE_PROCESSOR_AUDIO()
  /// The host pulls blocks through render() itself, in format, so
  /// processors run without an audio device or a block budget.
  static void setRenderOffline(
      bool offline,
      const ProcessorFormat& format = ProcessorFormat());
  static bool renderOffline();
  /// The frames the host has pulled so far in an offline render, which the
  /// main lane's audio clock reads. Each render starts at 0.
  static void setOfflineFrame(uint64_t frame);
  /// The offline render's frame and rate; false unless rendering offline.
  static bool offlineClock(uint64_t& frame, uint32_t& sampleRate);
#else
  static bool offlineClock(uint64_t&, uint32_t&) { return false; }
#endif
  /// Whether a processor's init has yet to run.
  bool starting() const;
};

} // namespace rive

#endif
#endif
