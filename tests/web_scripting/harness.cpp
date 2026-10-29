// Imports a riv the way a web host does: import, let the page prepare the
// script modules, start the scripts, then run an artboard for a few frames.
#include "rive/animation/state_machine_instance.hpp"
#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include "rive/renderer/ore/cmd/ore_deferred_context.hpp"
#include "rive/scroll_event.hpp"
#include "rive/wasm/wasm_lane_policy.hpp"
#include "rive/wasm/wasm_processors.hpp"
#include "rive/wasm/wasm_scripting_vm.hpp"
#include "utils/no_op_factory.hpp"
#include "utils/no_op_renderer.hpp"

#include <emscripten/emscripten.h>
#include <emscripten/stack.h>

using namespace rive;

static uint32_t byteSum(Span<const uint8_t> bytes)
{
  uint32_t sum = 0;
  for (uint8_t byte : bytes)
  {
    sum += byte;
  }
  return sum;
}

// Records what scripts send the GPU, as a host that replays it later does.
struct RecordingFactory : NoOpFactory
{
  ore::cmd::DeferredOreContext recorder{ore::ReplayCaps{}};
  size_t buffersPrinted = 0;

  ore::Context* ore() override { return &recorder; }

  // Prints each buffer made or updated since the last call.
  void printBuffers()
  {
    const auto& stream = recorder.stream();
    ore::cmd::OreCommandReader reader(
        stream.commandBytes(),
        stream.blobBytes());
    ore::cmd::CommandType type;
    size_t seen = 0;
    while (reader.next(type))
    {
      if (type == ore::cmd::CommandType::makeBuffer)
      {
        reader.read<ore::cmd::MakeResourcePOD>();
        auto desc = reader.read<ore::cmd::BufferDescPOD>();
        if (seen++ >= buffersPrinted)
        {
          printf(
              "gpu buffer of %u bytes%s sum %u\n",
              desc.size,
              desc.immutable ? " immutable" : "",
              byteSum(reader.blobAt(desc.data.offset, desc.data.size)));
        }
      }
      else if (type == ore::cmd::CommandType::bufferUpdate)
      {
        auto update = reader.read<ore::cmd::BufferUpdatePOD>();
        if (seen++ >= buffersPrinted)
        {
          printf(
              "gpu upload at %u of %u bytes sum %u\n",
              update.offset,
              update.bytes.size,
              byteSum(reader.blobAt(update.bytes.offset, update.bytes.size)));
        }
      }
      else
      {
        reader.skip(ore::cmd::orePayloadSizeOf(type));
      }
    }
    buffersPrinted = seen;
  }
};
static RecordingFactory s_factory;
static rcp<File> s_file;
static bool s_scroll = false;
static bool s_errorSink = false;
static std::unique_ptr<ArtboardInstance> s_artboard;
static std::unique_ptr<StateMachineInstance> s_machine;
static rcp<ViewModelInstance> s_data;

static void advanceFrames(int frames)
{
  NoOpRenderer renderer;
  for (int i = 0; i < frames; i++)
  {
    if (s_machine != nullptr)
    {
      s_machine->advanceAndApply(1.0f / 60.0f);
    }
    else
    {
      s_artboard->advance(1.0f / 60.0f);
    }
    s_artboard->draw(&renderer);
    s_file->frameBoundary();
    s_factory.printBuffers();
  }
}

extern "C"
{
  EMSCRIPTEN_KEEPALIVE int harness_import(const uint8_t* bytes, uint32_t size)
  {
    s_file = File::import(Span<const uint8_t>(bytes, size), &s_factory);
    if (s_file != nullptr && s_errorSink)
    {
      for (auto& vm : s_file->wasmVMs())
      {
        vm->onError([](const std::string& line) {
          printf("error sink: %s\n", line.c_str());
        });
      }
    }
    return s_file != nullptr;
  }

  // Gives the files imported after it an error sink, as rive-cli does.
  EMSCRIPTEN_KEEPALIVE void harness_error_sink() { s_errorSink = true; }

  // Writes a string at appAddr through the module memory seam and reads it
  // back as a host op would.
  EMSCRIPTEN_KEEPALIVE void harness_seam(uint32_t appAddr)
  {
    WasmScriptingVM* vm = s_file->wasmScriptingVM();
    bool wrote = vm != nullptr && vm->copyToModule(appAddr, "seam", 5);
    const char* text = wrote ? vm->resolveModuleString(appAddr) : nullptr;
    printf("seam %s\n", text != nullptr ? text : "failed");
  }

  // Returns how many script VMs are running.
  EMSCRIPTEN_KEEPALIVE int harness_run(int frames)
  {
    s_file->startScripts();
    printf("script vms kept: %zu\n", s_file->wasmVMs().size());
    int running = 0;
    for (auto& vm : s_file->wasmVMs())
    {
      running += vm->valid() ? 1 : 0;
    }
    // The machine points into the artboard, so it goes first.
    s_machine = nullptr;
    s_artboard = s_file->artboardDefault();
    if (s_artboard == nullptr)
    {
      return -1;
    }
    s_machine = s_artboard->defaultStateMachine();
    s_data = s_file->createDefaultViewModelInstance(s_artboard.get());
    if (s_machine != nullptr && s_data != nullptr)
    {
      s_machine->bindViewModelInstance(s_data);
    }
    advanceFrames(frames);
    printf("artboard drew %d frames\n", frames);
    if (s_scroll && s_machine != nullptr)
    {
      ScrollEvent event;
      event.delta = Vec2D(0.0f, -30.0f);
      event.phase = ScrollPhase::update;
      event.precise = true;
      HitResult hit = s_machine
                          ->pointerScroll(Vec2D(50.0f, 60.0f), event, 1.5f, 2);
      printf("scroll %s\n", hit != HitResult::none ? "claimed" : "declined");
    }
    int alive = 0;
    for (auto& vm : s_file->wasmVMs())
    {
      alive += vm->valid() ? 1 : 0;
      if (!vm->lastTrap().message.empty())
      {
        printf("last trap: %s\n", vm->lastTrap().message.c_str());
      }
    }
    printf("script vms alive after the frames: %d\n", alive);
    return running;
  }

  // Runs frames with only this much stack left, as deeply nested script
  // and host calls would.
  EMSCRIPTEN_KEEPALIVE int harness_run_low_on_stack(int frames, int left)
  {
    if (emscripten_stack_get_free() <= (size_t)left)
    {
      return harness_run(frames);
    }
    volatile char pad[1024];
    pad[0] = 0;
    return harness_run_low_on_stack(frames, left) + pad[0];
  }

  // More frames on the artboard harness_run started, for a page that lets
  // time pass between them.
  EMSCRIPTEN_KEEPALIVE void harness_frames(int frames)
  {
    if (s_artboard != nullptr)
    {
      advanceFrames(frames);
    }
  }

  // Renders blocks of 128 frames from frame 0 through each script audio
  // processor, as an audio thread would, and prints what came out, with
  // an FNV-1a hash of its bytes.
  EMSCRIPTEN_KEEPALIVE void harness_audio(int blocks)
  {
    int index = 0;
    for (auto& vm : s_file->wasmVMs())
    {
      WasmProcessors* processors = vm->processors();
      if (processors == nullptr)
      {
        continue;
      }
      for (auto& processor : processors->lanes())
      {
        uint32_t channels = processor->format().channels;
        std::vector<float> block(128 * channels);
        double left = 0;
        double right = 0;
        uint32_t hash = 2166136261u;
        for (int i = 0; i < blocks; i++)
        {
          processor->render(block.data(), 128, (uint64_t)i * 128);
          for (size_t f = 0; f < 128; f++)
          {
            left += block[f * channels];
            right += block[f * channels + channels - 1];
          }
          auto bytes = (const uint8_t*)block.data();
          for (size_t b = 0; b < block.size() * sizeof(float); b++)
          {
            hash = (hash ^ bytes[b]) * 16777619u;
          }
        }
        printf(
            "processor %d state %u sums %.2f %.2f hash %08x\n",
            index++,
            (uint32_t)processor->state(),
            left,
            right,
            hash);
      }
    }
  }

  // Renders blocks of 128 frames through each processor between frames, as
  // an offline host would, moving the clock the main lane reads past them.
  EMSCRIPTEN_KEEPALIVE void harness_audio_pump(int blocks)
  {
    uint64_t frame;
    uint32_t rate;
    WasmProcessors::offlineClock(frame, rate);
    std::vector<float> block;
    for (auto& vm : s_file->wasmVMs())
    {
      if (WasmProcessors* processors = vm->processors())
      {
        for (auto& processor : processors->lanes())
        {
          block.resize(128 * processor->format().channels);
          for (int i = 0; i < blocks; i++)
          {
            processor->render(block.data(), 128, frame + (uint64_t)i * 128);
          }
        }
      }
    }
    WasmProcessors::setOfflineFrame(frame + (uint64_t)blocks * 128);
  }

  // Whether the artboard still needs advances for script work.
  EMSCRIPTEN_KEEPALIVE int harness_pending()
  {
    return s_artboard != nullptr && s_artboard->hasPendingAsyncWork();
  }

  // Sends one scroll over the artboard after the frames.
  EMSCRIPTEN_KEEPALIVE void harness_scroll() { s_scroll = true; }

  // Starts only modules the page can bound, as an editor does.
  EMSCRIPTEN_KEEPALIVE void harness_require_bounds()
  {
    WasmScriptingVM::requireBounds(true);
  }

  // The harness pulls processor blocks itself, so they start without a
  // worklet.
  EMSCRIPTEN_KEEPALIVE void harness_render_audio_offline()
  {
    WasmProcessors::setRenderOffline(true);
  }

  // A loaded machine can hold a block past the default limit, which would
  // drop a processor no run means to drop.
  EMSCRIPTEN_KEEPALIVE void harness_audio_hard_blocks(uint32_t blocks)
  {
    ScriptingLimits limits = ScriptingLimits::get();
    limits.audioHardBlocks = blocks;
    ScriptingLimits::set(limits);
  }

  // Before the pool first runs.
  EMSCRIPTEN_KEEPALIVE void harness_job_threads(uint32_t threads)
  {
    ScriptingLimits limits = ScriptingLimits::get();
    limits.jobThreads = threads;
    ScriptingLimits::set(limits);
  }

  // Before the file loads; 0 runs its calls unbounded.
  EMSCRIPTEN_KEEPALIVE void harness_call_budget(uint32_t ms)
  {
    ScriptingLimits limits = ScriptingLimits::get();
    limits.mainCallBudgetMs = ms;
    ScriptingLimits::set(limits);
  }

  EMSCRIPTEN_KEEPALIVE void harness_parallel_safe()
  {
    ScriptingLimits limits = ScriptingLimits::get();
    limits.parallelSafeMode = true;
    ScriptingLimits::set(limits);
  }

  EMSCRIPTEN_KEEPALIVE uintptr_t harness_stack()
  {
    return emscripten_stack_get_current();
  }

  EMSCRIPTEN_KEEPALIVE void harness_release()
  {
    s_machine = nullptr;
    s_artboard = nullptr;
    s_data = nullptr;
    s_file = nullptr;
  }
}
