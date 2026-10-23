#ifdef WITH_RIVE_SCRIPTING_WASM

#include "rive/wasm/wasm_processors.hpp"
#include "rive/async/compare_exchange.hpp"
#include "rive/wasm/wasm_shared.hpp"

#ifdef WITH_RIVE_AUDIO
#include "rive/audio/audio_engine.hpp"
#endif

#ifndef __EMSCRIPTEN__
#include "wasm_export.h"
#else
#include <emscripten/em_js.h>
#include <emscripten/emscripten.h>
#endif

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace rive;

namespace
{
constexpr uint32_t kProcessorStack = 256 * 1024;

#if defined(WITH_RIVE_AUDIO) && !defined(__EMSCRIPTEN__)
class ProcessorRenderer : public AudioRenderer
{
public:
  explicit ProcessorRenderer(WasmProcessor* processor) : m_processor(processor)
  {}
  void render(float* out, uint32_t frames, uint64_t startFrame) override
  {
    m_processor->render(out, frames, startFrame);
  }

private:
  WasmProcessor* m_processor;
};
#endif

#ifndef __EMSCRIPTEN__
// Bounded inits get a thread no long job delays, leaked as it may outlive
// static destructors.
JobPool& initPool()
{
  static JobPool* pool = new JobPool(1);
  return *pool;
}
#endif

struct OfflineRender
{
  bool on = false;
  ProcessorFormat format;
  uint64_t frame = 0;
};

#if RIVE_PROCESSOR_AUDIO()
std::mutex& offlineMutex()
{
  static std::mutex mutex;
  return mutex;
}

OfflineRender& offlineSlot()
{
  static OfflineRender render;
  return render;
}

OfflineRender offlineState()
{
  std::lock_guard<std::mutex> lock(offlineMutex());
  return offlineSlot();
}
#else
OfflineRender offlineState() { return {}; }
#endif
} // namespace

WasmProcessor::WasmProcessor(
    rcp<WasmLaneModule> module,
    uint32_t entry,
    std::vector<uint8_t> input,
    ProcessorFormat format,
    uint32_t hardBlocks)
    : WasmLane(LaneKind::processor, std::move(module), entry, std::move(input))
    , m_format(format)
    , m_hardBlocks(hardBlocks)
{}

WasmProcessor::~WasmProcessor()
{
  stop();
  endInstance();
}

#if defined(__EMSCRIPTEN__) && RIVE_PROCESSOR_AUDIO()
static void stopRealtime(uint32_t id);
#endif

bool WasmProcessor::settle(LaneState state)
{
  LaneState now = m_state.load();
  while (now < LaneState::done)
  {
    if (compareExchangeWeak(m_state, now, state))
    {
      return true;
    }
  }
  return false;
}

void WasmProcessor::stop()
{
  settle(LaneState::stopped);
#if defined(WITH_RIVE_AUDIO) && !defined(__EMSCRIPTEN__)
  std::lock_guard<std::mutex> lock(m_nodeMutex);
  if (m_node != nullptr)
  {
    m_engine->detachRenderer(m_node);
    m_node = nullptr;
    // Nothing renders it again, and its watchdog slot keeps the watchdog
    // ticking.
    endInstance();
  }
  m_engine = nullptr;
#endif
#if defined(__EMSCRIPTEN__) && RIVE_PROCESSOR_AUDIO()
  if (m_audioId != 0)
  {
    stopRealtime(m_audioId);
    m_audioId = 0;
  }
#endif
}

#if RIVE_PROCESSOR_AUDIO()
void WasmProcessor::run()
{
  WasmLane* outerLane = WasmLane::setCurrent(this);
  bool ok = m_state.load() == LaneState::starting && startInstance();
  WasmLane::setCurrent(outerLane);
  releaseTrappedRegion();
#if defined(WITH_RIVE_AUDIO) && !defined(__EMSCRIPTEN__)
  std::lock_guard<std::mutex> lock(m_nodeMutex);
  // Joined before it reads as running, so a running processor is in the
  // mix. It renders silence until then.
  if (ok && m_engine != nullptr && m_state.load() == LaneState::starting)
  {
    m_renderer = std::make_unique<ProcessorRenderer>(this);
    m_node = m_engine->attachRenderer(m_renderer.get());
    if (m_node == nullptr)
    {
      fail("processor could not join the audio mix");
      ok = false;
    }
  }
#endif
  LaneState state = LaneState::starting;
  if (compareExchangeStrong(
          m_state,
          state,
          ok ? LaneState::running : LaneState::failed) &&
      ok)
  {
    return;
  }
  // A failed init or a stop that landed during it never renders.
#if defined(WITH_RIVE_AUDIO) && !defined(__EMSCRIPTEN__)
  if (m_node != nullptr)
  {
    m_engine->detachRenderer(m_node);
    m_node = nullptr;
  }
#endif
  endInstance();
}

void WasmProcessor::render(float* out, uint32_t frames, uint64_t startFrame)
{
  uint32_t channels = m_format.channels;
  uint32_t done = 0;
  WasmLane* outerLane = WasmLane::setCurrent(this);
  while (done < frames && m_state.load() == LaneState::running)
  {
    uint32_t block = std::min(frames - done, m_format.maxFrames);
    // A native may fail it without a trap, as a refused shared read does.
    if (!process(out + (size_t)done * channels, block, startFrame + done) ||
        hasFailure())
    {
      settle(LaneState::failed);
      break;
    }
    done += block;
  }
  WasmLane::setCurrent(outerLane);
  if (done < frames)
  {
    releaseTrappedRegion();
  }
  memset(
      out + (size_t)done * channels,
      0,
      (size_t)(frames - done) * channels * sizeof(float));
}
#else
// Every start here fails, so no processor runs.
void WasmProcessor::run() {}
#endif

// --- the WAMR backend --------------------------------------------------------

#ifndef __EMSCRIPTEN__
bool WasmProcessor::startInstance()
{
  ensureLaneThreadEnv();
  if (!instantiate(kProcessorStack))
  {
    return false;
  }
  // Registered here, so the audio thread never takes the watchdog's lock.
  m_budget = std::make_unique<WasmCallBudget>(m_execEnv);
  auto init = [&](auto&& step) {
    m_budget->arm(lane_layout::kProcessorInitMs);
    // Disarming clears an interrupt, so a cancel between steps ends here.
    if (cancelRequested())
    {
      interruptCall();
    }
    bool ok = step();
    bool fired = m_budget->disarm();
    if (!ok)
    {
      failTrap(fired ? lane_layout::kProcessorInitOverran : "");
    }
    return ok;
  };
  // (input, length, sampleRate f64, maxFrames, channels): the f64 takes
  // two cells.
  uint32_t argv[6] = {};
  if (!init([&] { return runStart(argv); }))
  {
    return false;
  }
  uint32_t at = argv[0];
  double rate = (double)m_format.sampleRate;
  memcpy(&argv[2], &rate, sizeof(rate));
  argv[4] = m_format.maxFrames;
  argv[5] = m_format.channels;
  if (!init(
          [&] { return call(m_module->entryNames[m_entry].c_str(), 6, argv); }))
  {
    return false;
  }
  argv[0] = at;
  if (!init([&] { return call(lane_layout::kLaneFreeExport, 1, argv); }))
  {
    return false;
  }
  // (frames, startFrame f64) answering the block's address.
  m_processFn = lookup(lane_layout::kProcessorProcessExport, 3, 3);
  m_collectFn = lookup(lane_layout::kFrameMinorExport, 0, 1, true);
  return m_processFn != nullptr && !hasFailure();
}

bool WasmProcessor::process(float* out, uint32_t frames, uint64_t startFrame)
{
  // The thread pulling audio can change, as when a device reroutes.
  ensureLaneThreadEnv();
  auto env = (wasm_exec_env_t)m_execEnv;
  uint32_t budgetMs = lane_layout::processorBlockBudgetMs(
      m_hardBlocks,
      frames,
      (double)m_format.sampleRate,
      !m_warm);
  auto inst = (wasm_module_inst_t)m_instance;
  wasm_val_t args[2];
  args[0].kind = WASM_I32;
  args[0].of.i32 = (int32_t)frames;
  args[1].kind = WASM_F64;
  args[1].of.f64 = (double)startFrame;
  wasm_val_t result;
  size_t bytes = (size_t)frames * m_format.channels * sizeof(float);
  if (budgetMs != 0)
  {
    m_budget->arm(budgetMs);
  }
  m_warm = true;
  bool ok = wasm_runtime_call_wasm_a(
                env,
                (wasm_function_inst_t)m_processFn,
                1,
                &result,
                2,
                args) &&
            wasm_runtime_validate_app_addr(inst, result.of.i32, bytes);
  if (ok)
  {
    memcpy(out, wasm_runtime_addr_app_to_native(inst, result.of.i32), bytes);
    // Back at the host no frame is live, so the block's garbage goes.
    uint32_t argv[1] = {0};
    ok = m_collectFn == nullptr || wasm_runtime_call_wasm(
                                       env,
                                       (wasm_function_inst_t)m_collectFn,
                                       0,
                                       argv);
  }
  bool fired = m_budget->disarm();
  if (!ok)
  {
    // Built in place, as this may be the audio thread. An interpreter's
    // tier says what to pass for compiled code.
    char overran[512] = "";
    if (fired)
    {
      snprintf(
          overran,
          sizeof(overran),
          "%s on %s",
          lane_layout::kProcessorOverran,
          m_module->tier.c_str());
    }
    failTrap(overran);
    return false;
  }
  return true;
}

void WasmProcessor::endInstance()
{
  m_budget = nullptr;
  teardown();
}
#else
// --- the browser backend -----------------------------------------------------

// clang-format off
EM_JS_DEPS(riveWebProcessors, "$stringToNewUTF8");

EM_JS(char*, riveWebAudioRefusal, (), {
    const refusal = Module['riveScripting']['audioRefusal']();
    return refusal === null ? 0 : stringToNewUTF8(refusal);
});

#if RIVE_PROCESSOR_AUDIO()
// The starts and the render answer null, or a malloc'd error.
EM_JS(char*, riveWebProcessorStart,
      (uint32_t lane, uint32_t moduleId, const uint8_t* module,
       uint32_t moduleSize, uint32_t entry, const uint8_t* input,
       uint32_t inputSize, uint32_t sampleRate, uint32_t maxFrames,
       uint32_t channels, double sharedOwner, uint32_t* idPtr), {
    const result = Module['riveScripting']['processorStart'](
        lane,
        moduleId,
        () => HEAPU8.slice(module, module + moduleSize),
        entry,
        HEAPU8.slice(input, input + inputSize),
        {sampleRate, maxFrames, channels},
        sharedOwner);
    if (!result.ok) {
        return stringToNewUTF8(result.error);
    }
    HEAPU32[idPtr >> 2] = result.id;
    return 0;
});

EM_JS(char*, riveWebProcessorRender,
      (uint32_t id, uint32_t frames, double startFrame, float* out), {
    const error = Module['riveScripting']['processorRender'](
        id, frames, startFrame, (block) => HEAPF32.set(block, out >> 2));
    return error === null ? 0 : stringToNewUTF8(error);
});

EM_JS(void, riveWebProcessorEnd, (uint32_t id), {
    Module['riveScripting']['processorEnd'](id);
});

EM_JS(double, riveWebAudioSampleRate, (), {
    return Module['riveScripting']['audioSampleRate']();
});

EM_JS(char*, riveWebAudioStart,
      (uint32_t owner, uint32_t moduleId, const uint8_t* module,
       uint32_t moduleSize, uint32_t entry, const uint8_t* input,
       uint32_t inputSize, uint32_t sampleRate, uint32_t maxFrames,
       uint32_t channels, uint32_t hardBlocks, double sharedOwner,
       uint32_t* idPtr), {
    const result = Module['riveScripting']['audioStart'](
        owner,
        moduleId,
        () => HEAPU8.slice(module, module + moduleSize),
        entry,
        HEAPU8.slice(input, input + inputSize),
        {sampleRate, maxFrames, channels},
        hardBlocks,
        sharedOwner);
    if (!result.ok) {
        return stringToNewUTF8(result.error);
    }
    HEAPU32[idPtr >> 2] = result.id;
    return 0;
});

EM_JS(void, riveWebAudioStop, (uint32_t id), {
    Module['riveScripting']['audioStop'](id);
});
#endif
// clang-format on

// A page's instance belongs to the thread that made it, so a processor
// starts on the thread that renders it, as an AudioWorklet's would.
bool WasmProcessor::startInstance() { return true; }

#if RIVE_PROCESSOR_AUDIO()
static void stopRealtime(uint32_t id) { riveWebAudioStop(id); }

bool WasmProcessor::startRealtime()
{
  std::vector<uint8_t> input = takeInput();
  char* error = riveWebAudioStart(
      (uint32_t)(uintptr_t)this,
      m_module->id,
      m_module->bytes.data(),
      (uint32_t)m_module->bytes.size(),
      m_entry,
      input.data(),
      (uint32_t)input.size(),
      m_format.sampleRate,
      m_format.maxFrames,
      m_format.channels,
      m_hardBlocks,
      (double)sharedOwner(),
      &m_audioId);
  if (error != nullptr)
  {
    failTaken(error);
    m_state.store(LaneState::failed);
    return false;
  }
  return true;
}

void WasmProcessor::event(uint32_t kind, const char* text)
{
  if (kind == kEventLog)
  {
    log(text);
    return;
  }
  if ((LaneState)kind == LaneState::failed)
  {
    fail(text);
  }
  settle((LaneState)kind);
}

extern "C" EMSCRIPTEN_KEEPALIVE void rive_web_processor_event(
    uint32_t processor,
    uint32_t kind,
    const char* text)
{
  ((WasmProcessor*)(uintptr_t)processor)->event(kind, text);
}

bool WasmProcessor::startOnPage()
{
  if (!riveWebAttach())
  {
    fail("no script runner on this thread");
    return false;
  }
  std::vector<uint8_t> input = takeInput();
  char* error = riveWebProcessorStart(
      (uint32_t)(uintptr_t)(WasmLane*)this,
      m_module->id,
      m_module->bytes.data(),
      (uint32_t)m_module->bytes.size(),
      m_entry,
      input.data(),
      (uint32_t)input.size(),
      m_format.sampleRate,
      m_format.maxFrames,
      m_format.channels,
      (double)sharedOwner(),
      &m_webInstance);
  if (error != nullptr)
  {
    failTaken(error);
    return false;
  }
  return true;
}

// Only an offline render pulls blocks here, and as natively those have no
// limit.
bool WasmProcessor::process(float* out, uint32_t frames, uint64_t startFrame)
{
  if (m_webInstance == 0 && !startOnPage())
  {
    return false;
  }
  char* error = riveWebProcessorRender(
      m_webInstance,
      frames,
      (double)startFrame,
      out);
  if (error != nullptr)
  {
    failTaken(error);
    return false;
  }
  return true;
}
#endif

void WasmProcessor::endInstance()
{
#if RIVE_PROCESSOR_AUDIO()
  if (m_webInstance != 0)
  {
    riveWebProcessorEnd(m_webInstance);
    m_webInstance = 0;
  }
#endif
}
#endif

// --- the set -----------------------------------------------------------------

WasmProcessors::~WasmProcessors()
{
  for (auto& processor : m_lanes)
  {
#ifndef __EMSCRIPTEN__
    // Asked only for a queued init, so teardown never makes the pool.
    if (processor->state() == LaneState::starting)
    {
      initPool().cancel(processor.get());
    }
#endif
    processor->stop();
  }
}

bool WasmProcessors::starting() const
{
#if RIVE_PROCESSOR_AUDIO()
  return std::any_of(
      m_lanes.begin(),
      m_lanes.end(),
      [](const rcp<WasmProcessor>& processor) {
        return processor->state() == LaneState::starting;
      });
#else
  // Each start fails before it returns.
  return false;
#endif
}

rcp<WasmProcessor> WasmProcessors::start(
    uint32_t entry,
    std::vector<uint8_t> input,
    uint32_t hardBlocks)
{
  if (entry >= entryCount())
  {
    return nullptr;
  }
  OfflineRender offlineRender = offlineState();
  bool offline = offlineRender.on;
  ProcessorFormat format;
  if (offline)
  {
    format = offlineRender.format;
    // Nothing plays it live, so no block of it can run late.
    hardBlocks = 0;
  }
  std::string refusal = "real time audio has no audio engine";
#ifdef __EMSCRIPTEN__
  // A refused worklet is final: the page's own engine has no block limit.
  if (!offline && riveWebAttach())
  {
    char* why = riveWebAudioRefusal();
#if RIVE_PROCESSOR_AUDIO()
    // The page's worklet runs it, so init happens there.
    if (why == nullptr)
    {
      format.sampleRate = (uint32_t)riveWebAudioSampleRate();
      format.maxFrames = 128;
      auto processor = add(make_rcp<WasmProcessor>(
          m_module,
          entry,
          std::move(input),
          format,
          hardBlocks));
      processor->startRealtime();
      return processor;
    }
#endif
    if (why != nullptr)
    {
      refusal = why;
      free(why);
    }
  }
#elif defined(WITH_RIVE_AUDIO)
  // Resolved here, on the VM's thread, since init runs on the pool.
  rcp<AudioEngine> engine = offline ? nullptr : AudioEngine::RuntimeEngine();
  if (engine != nullptr)
  {
    format.sampleRate = engine->sampleRate();
    format.channels = engine->channels();
  }
#endif
  auto processor = make_rcp<WasmProcessor>(
      m_module,
      entry,
      std::move(input),
      format,
      hardBlocks);
#if defined(WITH_RIVE_AUDIO) && !defined(__EMSCRIPTEN__)
  bool device = engine != nullptr;
  processor->m_engine = std::move(engine);
#else
  bool device = false;
#endif
  add(processor);
  // Nothing would pull its blocks, so it would only seem to play.
  if (!device && !offline)
  {
    processor->fail(refusal);
    processor->settle(LaneState::failed);
    return processor;
  }
#ifdef __EMSCRIPTEN__
  // The page instantiates it at its first block, so init costs nothing.
  processor->run();
#else
  initPool().submit(processor);
#endif
  return processor;
}

#if RIVE_PROCESSOR_AUDIO()
void WasmProcessors::setRenderOffline(
    bool offline,
    const ProcessorFormat& format)
{
  std::lock_guard<std::mutex> lock(offlineMutex());
  OfflineRender& render = offlineSlot();
  render.on = offline;
  // A block of no frames would never finish a render.
  render.format = {
      std::max<uint32_t>(format.sampleRate, 1),
      std::max<uint32_t>(format.channels, 1),
      std::max<uint32_t>(format.maxFrames, 1)};
  render.frame = 0;
}

bool WasmProcessors::renderOffline() { return offlineState().on; }

void WasmProcessors::setOfflineFrame(uint64_t frame)
{
  std::lock_guard<std::mutex> lock(offlineMutex());
  offlineSlot().frame = frame;
}

bool WasmProcessors::offlineClock(uint64_t& frame, uint32_t& sampleRate)
{
  OfflineRender render = offlineState();
  frame = render.frame;
  sampleRate = render.format.sampleRate;
  return render.on;
}
#endif

#endif
