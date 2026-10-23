#ifdef WITH_RIVE_SCRIPTING_WASM

#include "rive/wasm/wasm_scripting_vm.hpp"
#include "rive/async/compare_exchange.hpp"
#include "rive/async/timeouts.hpp"
#include "rive/wasm/wasm_jobs.hpp"
#include "rive/wasm/wasm_lane.hpp"
#include "rive/wasm/wasm_shared.hpp"
#include "rive/wasm/wasm_workers.hpp"
#include "rive/wasm/wasm_processors.hpp"
#ifndef __EMSCRIPTEN__
#include "rive/wasm/aot_artifact.hpp"
#include "rive/wasm/module_tier_ladder.hpp"
#include "rive/wasm/prelinked_aot.hpp"
#else
#include <emscripten/emscripten.h>
#include <emscripten/threading.h>
#endif
#include <mutex>
#include <thread>
#include <unordered_map>
#ifdef WITH_RIVE_TEXT
#include "rive/text/raw_text.hpp"
#include "rive/text/cursor.hpp"
#include "rive/text/font_hb.hpp"
#endif

#include "rive/animation/linear_animation.hpp"
#include "rive/animation/linear_animation_instance.hpp"
#include "rive/animation/listener_invocation.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/input/focus_manager.hpp"
#include "rive/artboard.hpp"
#include "rive/assets/blob_asset.hpp"
#include "rive/custom_property_boolean.hpp"
#include "rive/custom_property_color.hpp"
#include "rive/assets/manifest_asset.hpp"
#include "rive/custom_property.hpp"
#include "rive/custom_property_number.hpp"
#include "rive/custom_property_string.hpp"
#include "rive/drawable.hpp"
#ifdef WITH_RIVE_AUDIO
#include "rive/assets/audio_asset.hpp"
#include "rive/audio/audio_engine.hpp"
#include "rive/audio/audio_sound.hpp"
#include "rive/audio/audio_source.hpp"
#endif
#include "rive/bones/root_bone.hpp"
#include "rive/constraints/constraint.hpp"
#include "rive/container_component.hpp"
#include "rive/math/transform_components.hpp"
#include "rive/node.hpp"
#include "rive/shapes/path.hpp"
#include "rive/transform_component.hpp"
#include "rive/viewmodel/viewmodel_instance_symbol_list_index.hpp"
#include "rive/wasm/artboard_wire.hpp"
#include "rive/assets/font_asset.hpp"
#include "rive/assets/image_asset.hpp"
#include "rive/assets/shader_asset.hpp"
#include "rive/async/work_pool.hpp"
#include "rive/data_bind/data_context.hpp"
#include "rive/data_bind/data_values/data_value.hpp"
#include "rive/data_bind/data_values/data_value_boolean.hpp"
#include "rive/data_bind/data_values/data_value_color.hpp"
#include "rive/data_bind/data_values/data_value_number.hpp"
#include "rive/data_bind/data_values/data_value_string.hpp"
#include "rive/factory.hpp"
#include "rive/file.hpp"
#include "rive/shapes/paint/feather.hpp"
#include "rive/shapes/paint/fill.hpp"
#include "rive/shapes/paint/image_sampler.hpp"
#include "rive/shapes/paint/shape_paint.hpp"
#include "rive/shapes/paint/solid_color.hpp"
#include "rive/shapes/paint/stroke.hpp"
#include "rive/math/raw_path.hpp"
#include "rive/math/contour_measure.hpp"
#include "rive/math/path_measure.hpp"
#include "rive/renderer.hpp"
#include "rive/scripted/decoded_file.hpp"
#include "rive/scripted/scripted_object.hpp"
#include "rive/scripted/transition_child_ref.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/viewmodel_instance_boolean.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "rive/viewmodel/viewmodel_instance_string.hpp"
#include "rive/viewmodel/data_enum.hpp"
#include "rive/viewmodel/data_enum_value.hpp"
#include "rive/viewmodel/viewmodel_instance_color.hpp"
#include "rive/viewmodel/viewmodel_instance_enum.hpp"
#include "rive/viewmodel/viewmodel_property_enum.hpp"
#include "rive/viewmodel/viewmodel_instance_trigger.hpp"
#include "rive/viewmodel/viewmodel_instance_list.hpp"
#include "rive/viewmodel/viewmodel_instance_list_item.hpp"
#include "rive/viewmodel/viewmodel_instance_viewmodel.hpp"
#include "rive/viewmodel/viewmodel_instance_asset_blob.hpp"
#include "rive/viewmodel/viewmodel_instance_asset_font.hpp"
#include "rive/viewmodel/viewmodel_instance_asset_image.hpp"
#include "rive/wasm/data_convert_wire.hpp"
#include "rive/wasm/gamepad_wire.hpp"
#include "rive/wasm/listener_wire.hpp"
#include "rive/wasm/path_effect_wire.hpp"
#include "rive/wasm/net_wire.hpp"
#include "rive/bindable_artboard.hpp"
#include "rive/viewmodel/viewmodel_instance_artboard.hpp"
#ifdef WITH_RIVE_SCRIPTNET
#include "rive/scriptnet/net.hpp"
#endif

#ifdef __EMSCRIPTEN__
#include "rive/async/browser_image_decode.hpp"
#elif defined(RIVE_DECODERS)
#include "rive/decoders/bitmap_decoder.hpp"

#include <chrono>
#include <sys/stat.h>
#endif

#ifdef RIVE_CANVAS
#include "rive/renderer/render_context.hpp"
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/cmd/deferred_canvas_host.hpp"
#include "rive/renderer/rive_render_image.hpp"
#endif
#if defined(RIVE_CANVAS) && defined(RIVE_ORE)
#include "rive/renderer/ore/ore_context.hpp"
#include "rive/renderer/ore/ore_deferred_bind_groups.hpp"
#include "rive/renderer/ore/ore_draw_bindings.hpp"
#include "rive/renderer/ore/ore_script_guards.hpp"
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_texture.hpp"
#include "rive/renderer/ore/ore_sampler.hpp"
#include "rive/renderer/ore/ore_shader_module.hpp"
#include "rive/renderer/ore/ore_shader_asset_modules.hpp"
#include "rive/renderer/ore/ore_bind_group_layout.hpp"
#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/renderer/ore/cmd/ore_deferred_render_pass.hpp"
#include "rive/renderer/cmd/deferred_render_resource.hpp"

#include <algorithm>
#include <vector>
#include <atomic>
#include <unordered_map>
#include <condition_variable>
#include <thread>
#include <mutex>
#include <cmath>
#include <cassert>
#endif

#ifndef __EMSCRIPTEN__
#include "wasm_export.h"
#endif
#include "wasm_utf16.hpp"

#include <chrono>
#include <cstdarg>
#include <cstddef>
#include <string.h>
#include <time.h>

using namespace rive;

namespace
{

constexpr char kLongjmpTag[] = "rive-longjmp";

#ifndef __EMSCRIPTEN__
WasmScriptingVM* vmFromEnv(wasm_exec_env_t env)
{
  return static_cast<WasmScriptingVM*>(wasm_runtime_get_user_data(env));
}

// A lane has no VM, so its lines go to stderr.
void reportScriptError(WasmScriptingVM* vm, const std::string& line)
{
  if (vm != nullptr)
  {
    vm->reportError(line);
  }
  else
  {
    fprintf(stderr, "%s\n", line.c_str());
  }
}
#endif

std::string budgetExceededLine(int ms)
{
  return "script error: execution exceeded " + std::to_string(ms) +
         " millisecond timeout";
}

// Module start runs inside wasm_runtime_instantiate, before an exec env
// exists to carry the vm, so natives called from there resolve none. rasc
// runs every script's top level there, and its output is the one thing a
// host still needs; the sink for that window is parked here.
thread_local const std::function<void(const char*, size_t)>*
    s_bootPrint = nullptr;
// The VM whose module is starting, for the natives its top level reaches.
thread_local WasmScriptingVM* s_booting = nullptr;
std::atomic<bool> s_requireBounds{false};
thread_local WasmScriptingVM::BootHook s_bootHook;

} // namespace

// The VMs one thread's polls reach. A VM may be destroyed on another thread,
// so it leaves its list under the lock a poll holds while it delivers.
struct rive::VMPollList
{
#if RIVE_ONE_THREAD()
  // One thread runs every poll and VM, and its lists outlive them all.
  using Ref = VMPollList*;
  struct Lock
  {
    explicit Lock(const VMPollList&) {}
  };
#else
  using Ref = std::shared_ptr<VMPollList>;
  struct Lock : std::lock_guard<std::recursive_mutex>
  {
    explicit Lock(const VMPollList& list) : lock_guard(*list.mutex) {}
  };

  // A thread's lists share it, as delivering either one can join the other.
  std::shared_ptr<std::recursive_mutex> mutex;

  explicit VMPollList(std::shared_ptr<std::recursive_mutex> threadMutex)
      : mutex(std::move(threadMutex))
  {}
#endif
  std::vector<WasmScriptingVM*> vms;

  // Joins list, set to the thread's own the first time.
  static void join(Ref& list, const Ref& thread, WasmScriptingVM* vm)
  {
    if (list == nullptr)
    {
      list = thread;
    }
    Lock lock(*list);
    if (std::find(list->vms.begin(), list->vms.end(), vm) == list->vms.end())
    {
      list->vms.push_back(vm);
    }
  }

  static void leave(const Ref& list, WasmScriptingVM* vm)
  {
    if (list != nullptr)
    {
      Lock lock(*list);
      list->vms.erase(
          std::remove(list->vms.begin(), list->vms.end(), vm),
          list->vms.end());
    }
  }

  // A delivery runs script, which can destroy another VM on the list.
  template <typename Deliver> void deliver(Deliver&& each)
  {
    Lock lock(*this);
    for (WasmScriptingVM* vm : std::vector<WasmScriptingVM*>(vms))
    {
      if (std::find(vms.begin(), vms.end(), vm) != vms.end())
      {
        each(vm);
      }
    }
  }
};

namespace
{
#if RIVE_ONE_THREAD()
// Never destroyed, so they need no static constructor or destructor.
[[clang::no_destroy]] VMPollList s_holdingList;
[[clang::no_destroy]] VMPollList s_laneList;
VMPollList* const t_holdingVMs = &s_holdingList;
VMPollList* const t_laneVMs = &s_laneList;
#else
thread_local std::shared_ptr<std::recursive_mutex>
    t_pollMutex = std::make_shared<std::recursive_mutex>();
// The VMs holding outcomes, so a poll reaches them without walking every
// artboard that might own one.
thread_local std::shared_ptr<VMPollList>
    t_holdingVMs = std::make_shared<VMPollList>(t_pollMutex);
// The VMs with lanes started on this thread, so a poll reaches their output.
thread_local std::shared_ptr<VMPollList>
    t_laneVMs = std::make_shared<VMPollList>(t_pollMutex);
#endif
constexpr size_t kLaneResultBytesPerPoll = 8u << 20;
} // namespace

struct rive::WasmScriptingVMNatives
{
  static void print(WasmScriptingVM* vm, const char* data, size_t size);
  static int startFuelMs(const WasmScriptingVM* vm);
  static const char* startRefusal(const WasmScriptingVM* vm);
#ifndef __EMSCRIPTEN__
  static WasmScriptingVM* adoptBooting(wasm_exec_env_t env);
#else
  static void booting(WasmScriptingVM* vm);
#endif
  static std::unordered_map<std::string, int64_t>& consoleTimers(
      WasmScriptingVM* vm);
};

#ifndef __EMSCRIPTEN__
// Loaded modules are immutable and shared: translating one costs about 11ms,
// instantiating it microseconds, and files of one content load the same bytes.
struct SharedWasmModule
{
  // The buffer WAMR loads from and references until the module unloads.
  std::vector<uint8_t> bytes;
  wasm_module_t module = nullptr;
  // Artifact-backed entries hand every later VM their real tier; without
  // this a cache hit reports interp while running compiled code.
  WasmScriptingVM::ExecutionTier tier = WasmScriptingVM::ExecutionTier::interp;
  // No device run covers unloading prelinked code.
  bool pinned = false;
  // Orders the idle ones, so the least recently leased unloads first.
  uint64_t lastLeased = 0;

  ~SharedWasmModule()
  {
    if (module != nullptr)
    {
      wasm_runtime_unload(module);
    }
  }
};

using SharedModuleCache = std::
    unordered_map<uint64_t, std::shared_ptr<SharedWasmModule>>;

static SharedModuleCache& sharedModuleCache()
{
  // Never destroyed, so nothing unloads at exit and threads still booting
  // at exit never see it torn down.
  static auto* cache = new SharedModuleCache();
  return *cache;
}

// Hosts boot modules on more than one thread, such as a build thread and
// a render thread.
static std::mutex& sharedModuleCacheMutex()
{
  static std::mutex mutex;
  return mutex;
}

// Leases start only under the cache mutex, so under it an entry nothing else
// holds is idle and stays idle.
static std::shared_ptr<void> leaseLocked(
    const std::shared_ptr<SharedWasmModule>& entry)
{
  static uint64_t leases = 0;
  entry->lastLeased = ++leases;
  return std::shared_ptr<void>(entry, entry->module);
}

static std::shared_ptr<void> leaseCachedModule(
    uint64_t key,
    WasmScriptingVM::ExecutionTier& tier)
{
  std::lock_guard<std::mutex> lock(sharedModuleCacheMutex());
  auto cached = sharedModuleCache().find(key);
  if (cached == sharedModuleCache().end())
  {
    return nullptr;
  }
  tier = cached->second->tier;
  return leaseLocked(cached->second);
}

static void evictIdleModulesLocked(
    std::vector<std::shared_ptr<SharedWasmModule>>& evicted)
{
  SharedModuleCache& cache = sharedModuleCache();
  std::vector<std::pair<uint64_t, uint64_t>> idle;
  for (const auto& [key, entry] : cache)
  {
    if (entry.use_count() == 1 && !entry->pinned)
    {
      idle.emplace_back(entry->lastLeased, key);
    }
  }
  if (idle.size() <= WasmScriptingVM::kIdleModules)
  {
    return;
  }
  std::sort(idle.begin(), idle.end());
  idle.resize(idle.size() - WasmScriptingVM::kIdleModules);
  for (const auto& [lastLeased, key] : idle)
  {
    auto found = cache.find(key);
    evicted.push_back(std::move(found->second));
    cache.erase(found);
  }
}

// Compiled modules cache apart from interpreted ones of the same bytes, and
// each species apart from the others, since the module carries its tier.
static uint64_t compiledModuleKey(uint64_t key, TierSpecies species)
{
  key ^= 0x9e3779b97f4a7c15ull;
  if (species == TierSpecies::hw)
  {
    // The hw bounds flag lives on the module.
    key ^= 0xc2b2ae3d27d4eb4full;
  }
  if (species == TierSpecies::o0)
  {
    key ^= 0x27d4eb2f165667c5ull;
  }
  return key;
}

struct WasmScriptingVM::WamrState
{
  // Keeps module loaded until the destructor has ended the instance.
  std::shared_ptr<void> lease;
  wasm_module_t module = nullptr;
  wasm_module_inst_t instance = nullptr;
  wasm_exec_env_t execEnv = nullptr;

  // Export names reach callModule as literals, so the pointer is the key;
  // the owned copy lets debug builds catch a caller that reuses a buffer.
  struct ExportSlot
  {
    const char* key = nullptr;
    std::string name;
    wasm_function_inst_t function = nullptr;
  };
  ExportSlot exportCache[64];

  // The module's __riveCall globals: state of the call in progress, which
  // a trap unwinds without restoring.
  static constexpr uint32_t kMaxCallState = 8;
  uint32_t* callState[kMaxCallState] = {};
  uint32_t callStateCount = 0;

  // Puts the call state back when a call into the module ends.
  class CallStateScope
  {
  public:
    explicit CallStateScope(const WamrState& state) : m_state(state)
    {
      for (uint32_t i = 0; i < state.callStateCount; i++)
      {
        m_saved[i] = *state.callState[i];
      }
    }

    ~CallStateScope()
    {
      for (uint32_t i = 0; i < m_state.callStateCount; i++)
      {
        *m_state.callState[i] = m_saved[i];
      }
    }

  private:
    const WamrState& m_state;
    uint32_t m_saved[kMaxCallState];
  };

  void findCallState()
  {
    int32_t count = wasm_runtime_get_export_count(module);
    for (int32_t i = 0; i < count; i++)
    {
      wasm_export_t entry;
      wasm_runtime_get_export_type(module, i, &entry);
      wasm_global_inst_t global;
      if (entry.kind != WASM_IMPORT_EXPORT_KIND_GLOBAL ||
          strncmp(entry.name, "__riveCall", 10) != 0 ||
          !wasm_runtime_get_export_global_inst(instance, entry.name, &global) ||
          global.kind != WASM_I32 || !global.is_mutable)
      {
        continue;
      }
      if (callStateCount == kMaxCallState)
      {
        fprintf(
            stderr,
            "wasm module has more call state than the host "
            "restores; a trap may leave %s stale\n",
            entry.name);
        continue;
      }
      callState[callStateCount++] = (uint32_t*)global.global_data;
    }
  }

  wasm_function_inst_t lookupExport(const char* name)
  {
    ExportSlot& slot = exportCache
        [(reinterpret_cast<uintptr_t>(name) * 0x9E3779B97F4A7C15ull) >> 58];
    if (slot.key != name)
    {
      wasm_function_inst_t function = wasm_runtime_lookup_function(
          instance,
          name);
      if (function == nullptr)
      {
        return nullptr;
      }
      slot = {name, name, function};
    }
    assert(slot.name == name);
    return slot.function;
  }

  ~WamrState()
  {
    if (execEnv != nullptr)
    {
      wasm_runtime_destroy_exec_env(execEnv);
    }
    if (instance != nullptr)
    {
      wasm_runtime_deinstantiate(instance);
    }
  }
};
#endif

#if defined(RIVE_CANVAS) && defined(RIVE_ORE)
namespace
{
ore::Context* gpuOreContext(WasmScriptingVM* vm);
}
#endif
#ifdef RIVE_CANVAS
namespace
{
void canvasEndFrameImpl(WasmScriptingVM* vm, uint32_t canvas);
}
#endif

uint32_t WasmScriptingVM::callModule(
    const char* name,
    uint32_t argc,
    uint32_t* argv)
{
  uint32_t result = 0;
  callModuleChecked(name, argc, argv, &result);
  return result;
}

struct WasmScriptingVM::BudgetSlot
{
  std::atomic<int64_t> deadline{0};
  // Set when the watchdog interrupted the last budgeted call.
  std::atomic<bool> fired{false};
#ifndef __EMSCRIPTEN__
  wasm_exec_env_t env = nullptr;
#endif
};

namespace
{
int64_t steadyNanos()
{
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}
} // namespace

// The watchdog interrupts WAMR; the browser's engine has no such hook, so
// there a call runs unbounded.
#ifndef __EMSCRIPTEN__
namespace
{
// One thread bounds every VM's calls, so modules need no fuel counters: it
// polls each VM's deadline and, on an overrun, sets the call's terminate
// flag so the interpreter or AOT code traps at its next loop back-edge.
// Calls only store and swap an atomic, so the script thread never waits.
constexpr int64_t kFiring = -1;

class ScriptWatchdog
{
public:
  // Leaked, with a detached thread, so no static destructor joins it or
  // frees the mutex a VM in another static still uses.
  static ScriptWatchdog& instance()
  {
    static ScriptWatchdog* watchdog = new ScriptWatchdog();
    return *watchdog;
  }

  // A polled slot is armed from any thread, the audio thread's included,
  // so while one exists the watchdog ticks and arming makes no syscall.
  void add(WasmScriptingVM::BudgetSlot* slot, bool polled = false)
  {
    m_polled.fetch_add(polled ? 1 : 0);
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_started)
    {
      m_started = true;
      std::thread([this] { run(); }).detach();
    }
    m_slots.push_back(slot);
    // A watchdog waiting without a deadline starts ticking.
    if (polled)
    {
      m_wake.wake();
    }
  }

  void remove(WasmScriptingVM::BudgetSlot* slot, bool polled = false)
  {
    // An arm skipped its wake while slots were polled, so the last one
    // going rescans before the watchdog may sleep without a deadline.
    bool lastPolled = polled && m_polled.fetch_sub(1) == 1;
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_slots.erase(
          std::remove(m_slots.begin(), m_slots.end(), slot),
          m_slots.end());
    }
    if (lastPolled)
    {
      m_wake.wake();
    }
  }

  // Calls arm without any lock, so the audio thread may: only an arm that
  // finds the thread idle, or asleep until a later deadline, wakes it.
  void armed(int64_t deadline)
  {
    if (m_polled.load() != 0)
    {
      return;
    }
    if (m_idle.load() || deadline < m_waitingUntil.load())
    {
      m_wake.wake();
    }
  }

private:
  void run()
  {
    while (true)
    {
      // Read before the scan, so an arm racing it either shows up in
      // the scan or moves the word and ends the wait.
      uint32_t seen = m_wake.value();
      m_idle.store(true);
      int64_t earliest = 0;
      {
        std::lock_guard<std::mutex> lock(m_mutex);
        int64_t now = steadyNanos();
        for (WasmScriptingVM::BudgetSlot* slot : m_slots)
        {
          int64_t deadline = slot->deadline.load();
          if (deadline > 0 && now >= deadline &&
              slot->deadline.compare_exchange_strong(deadline, kFiring))
          {
            slot->fired.store(true);
            wasm_runtime_rive_interrupt(slot->env);
            slot->deadline.store(0);
          }
          else if (deadline > 0 && (earliest == 0 || deadline < earliest))
          {
            earliest = deadline;
          }
        }
      }
      if (m_polled.load() != 0)
      {
        int64_t tick = steadyNanos() + kPollNanos;
        earliest = earliest == 0 ? tick : std::min(earliest, tick);
      }
      if (earliest == 0)
      {
        m_waitingUntil.store(INT64_MAX);
        m_wake.wait(seen, std::chrono::steady_clock::time_point::max());
        continue;
      }
      // Stays idle until the deadline is published, so an arm in
      // between still wakes it.
      m_waitingUntil.store(earliest);
      m_idle.store(false);
      m_wake.wait(
          seen,
          std::chrono::steady_clock::time_point(
              std::chrono::duration_cast<std::chrono::steady_clock::duration>(
                  std::chrono::nanoseconds(earliest))));
    }
  }

  // How often the watchdog checks while a polled slot exists.
  static constexpr int64_t kPollNanos = 2000000;
  std::atomic<int32_t> m_polled{0};
  std::mutex m_mutex;
  WakeWord m_wake;
  std::vector<WasmScriptingVM::BudgetSlot*> m_slots;
  std::atomic<bool> m_idle{false};
  std::atomic<int64_t> m_waitingUntil{INT64_MAX};
  bool m_started = false;
};

// Swaps the slot's deadline out for none, waiting out a firing so its
// interrupt never lands on a later call; answers the deadline it took.
int64_t takeDeadline(WasmScriptingVM::BudgetSlot& slot)
{
  int64_t deadline = slot.deadline.load();
  while (deadline == kFiring ||
         !slot.deadline.compare_exchange_weak(deadline, 0))
  {
    deadline = slot.deadline.load();
  }
  return deadline;
}
} // namespace

struct WasmCallBudget::State
{
  WasmScriptingVM::BudgetSlot budget;
};

WasmCallBudget::WasmCallBudget(void* execEnv)
    : m_state(std::make_unique<State>())
{
  m_state->budget.env = (wasm_exec_env_t)execEnv;
  ScriptWatchdog::instance().add(&m_state->budget, true);
}

WasmCallBudget::~WasmCallBudget()
{
  disarm();
  ScriptWatchdog::instance().remove(&m_state->budget, true);
}

#if defined(WITH_RIVE_TOOLS) || defined(TESTING)
static std::atomic<uint32_t> s_interruptDelayMs{0};

void WasmCallBudget::delayInterrupts(uint32_t ms) { s_interruptDelayMs = ms; }
#else
static constexpr uint32_t s_interruptDelayMs = 0;
#endif

void WasmCallBudget::arm(uint32_t timeoutMs)
{
  m_state->budget.fired.store(false);
  int64_t deadline = steadyNanos() +
                     ((int64_t)timeoutMs + s_interruptDelayMs) * 1000000;
  m_state->budget.deadline.store(deadline);
  ScriptWatchdog::instance().armed(deadline);
}

bool WasmCallBudget::disarm()
{
  WasmScriptingVM::BudgetSlot& slot = m_state->budget;
  takeDeadline(slot);
  bool fired = slot.fired.load();
  wasm_runtime_rive_clear_interrupt(slot.env);
  return fired;
}

#endif

namespace
{
// Module calls live on this thread, any VM's.
thread_local uint32_t t_moduleCalls = 0;
using Clock = std::chrono::steady_clock;
} // namespace

// What the module call on this thread has left of its budget, infinity
// when nothing bounds it.
static double callSecondsLeft()
{
  Clock::time_point ends = callDeadline();
  if (ends == Clock::time_point::max())
  {
    return INFINITY;
  }
  return std::max(
      0.0,
      std::chrono::duration<double>(ends - Clock::now()).count());
}

// What a wait may spend of its call's budget, keeping a little for the
// script to act once the wait ends.
static double joinableSeconds()
{
  constexpr double kJoinerReserveSeconds = 0.02;
  return callSecondsLeft() - kJoinerReserveSeconds;
}

WasmScriptingVM::WaitScope::WaitScope(WasmScriptingVM* vm)
    : m_vm(vm)
    , m_usable(joinableSeconds())
    , m_start(Clock::now())
{
#ifndef __EMSCRIPTEN__
  // A deadline already fired stays fired.
  if (vm != nullptr && vm->m_budgetSlot != nullptr)
  {
    m_paused = takeDeadline(*vm->m_budgetSlot);
  }
#endif
}

WasmScriptingVM::WaitScope::~WaitScope()
{
  // A wait begun with nothing usable gives no time back, so retries cannot
  // move the call's end.
  double over = std::isinf(m_usable) || m_usable <= 0
                    ? 0
                    : std::chrono::duration<double>(Clock::now() - m_start)
                              .count() -
                          m_usable;
  auto shift = std::chrono::duration_cast<Clock::duration>(
      std::chrono::duration<double>(std::max(0.0, over)));
#ifndef __EMSCRIPTEN__
  if (m_paused > 0)
  {
    int64_t deadline = m_paused +
                       std::chrono::duration_cast<std::chrono::nanoseconds>(
                           shift)
                           .count();
    m_vm->m_budgetSlot->deadline.store(deadline);
    ScriptWatchdog::instance().armed(deadline);
  }
#endif
  if (over <= 0 || m_vm == nullptr)
  {
    return;
  }
  Clock::time_point deadline = callDeadline();
  if (deadline != Clock::time_point::max())
  {
    restoreCallDeadline(deadline + shift);
  }
  // A metered module times its own budget, so it moves its deadline on,
  // rounded up so it never falls behind the host's.
  if (m_vm->m_fuelShift && m_vm->m_fuelBudget)
  {
    uint32_t args[1] = {(uint32_t)std::ceil(over * 1000)};
    m_vm->callModule("__riveFuelShift", 1, args);
  }
}

bool WasmScriptingVM::inModuleCallOnThread() { return t_moduleCalls != 0; }

WasmScriptingVM::ScriptCallScope::ScriptCallScope(
    WasmScriptingVM* vm,
    bool budgeted)
    : m_vm(vm)
{
  t_moduleCalls++;
  if (vm->m_debugHooks != nullptr)
  {
    vm->m_debugHooks->onCallBegin(*vm);
  }
  // The outermost scope sweeps what any nested call left open.
  bool outermost = vm->m_callDepth++ == 0;
  if (outermost)
  {
    // Another VM's call on this thread may hold a region of its own.
    m_outerRegion = stashHeldRegion(&m_outerRegionId);
    // Paths that consume their own traps never reach reportTrap to clear
    // it.
    vm->m_budgetRaised = false;
    vm->m_thrownRaised = false;
  }
#ifndef __EMSCRIPTEN__
  // Script a runtime service calls back into arms its own.
  if (budgeted && vm->m_hostBudget && !vm->m_budgetArmed && vm->boundsCalls())
  {
    vm->armWatchdog(vm->m_state->execEnv);
    m_budgeted = true;
  }
#endif
  m_deadlineArmed = budgeted && vm->armDeadline();
  if (!outermost)
  {
    return;
  }
#if defined(RIVE_CANVAS) && defined(RIVE_ORE)
  ore::Context* oreContext = gpuOreContext(vm);
  m_passToken = oreContext != nullptr ? oreContext->nextRenderPassToken() : 0;
#endif
#ifdef RIVE_CANVAS
  m_frameToken = vm->m_nextCanvasFrameToken;
#endif
}

WasmScriptingVM::ScriptCallScope::~ScriptCallScope()
{
  t_moduleCalls--;
  if (m_deadlineArmed)
  {
    m_vm->disarmDeadline();
  }
#ifndef __EMSCRIPTEN__
  if (m_budgeted)
  {
    m_vm->disarmWatchdog();
  }
#endif
  if (--m_vm->m_callDepth == 0)
  {
    // A trap can unwind out of an exclusive section with its lock held.
    restoreHeldRegion(m_outerRegionId, m_outerRegion);
#if defined(RIVE_CANVAS) && defined(RIVE_ORE)
    ore::Context* oreContext = gpuOreContext(m_vm);
    if (oreContext != nullptr &&
        oreContext->finishOpenRenderPassesFrom(m_passToken) != 0)
    {
      fprintf(
          stderr,
          "GPU render pass left open at script return. Call finish() "
          "on render passes before returning.\n");
    }
#endif
#ifdef RIVE_CANVAS
    std::vector<uint32_t> openFrames = m_vm->takeOpenCanvasFramesFrom(
        m_frameToken);
    for (uint32_t canvas : openFrames)
    {
      canvasEndFrameImpl(m_vm, canvas);
    }
    if (!openFrames.empty())
    {
      fprintf(
          stderr,
          "Canvas frame left open at script return. Call endFrame() "
          "before returning.\n");
    }
#endif
  }
  if (m_vm->m_debugHooks != nullptr)
  {
    // Still set here when the call trapped; the caller clears it.
    const char* trap = nullptr;
#ifndef __EMSCRIPTEN__
    trap = m_vm->m_state != nullptr && m_vm->m_state->instance != nullptr
               ? wasm_runtime_get_exception(m_vm->m_state->instance)
               : nullptr;
#endif
    m_vm->m_debugHooks->onCallEnd(*m_vm, trap);
  }
}

// A debugger pauses mid call, so its sessions run unbudgeted. A metered
// module ends its own overrun, but waits must still give up in time.
bool WasmScriptingVM::boundsCalls() const
{
  return (m_hostBudget || m_fuelBudget) && m_timeoutMs > 0 &&
         m_debugHooks == nullptr;
}

uint32_t WasmScriptingVM::pageThreadBudgetMs() const
{
  return boundsCalls() ? (uint32_t)m_timeoutMs : 0;
}

// Only joins, region locks and signal waits read the call deadline.
bool WasmScriptingVM::armDeadline()
{
  if (m_deadlineArmed || !hasLanes() || !boundsCalls())
  {
    return false;
  }
  m_outerDeadline = armCallDeadline(
      Clock::now() + std::chrono::milliseconds(m_timeoutMs));
  m_deadlineArmed = true;
  return true;
}

void WasmScriptingVM::disarmDeadline()
{
  restoreCallDeadline(m_outerDeadline);
  m_deadlineArmed = false;
}

#ifndef __EMSCRIPTEN__
void WasmScriptingVM::armWatchdog(void* env)
{
  if (m_budgetSlot == nullptr)
  {
    m_budgetSlot = std::make_unique<BudgetSlot>();
    ScriptWatchdog::instance().add(m_budgetSlot.get());
  }
  BudgetSlot* slot = m_budgetSlot.get();
  slot->env = (wasm_exec_env_t)env;
  slot->fired.store(false);
  int64_t deadline = steadyNanos() + (int64_t)watchdogBudgetMs() * 1000000;
  slot->deadline.store(deadline);
  ScriptWatchdog::instance().armed(deadline);
  m_budgetArmed = true;
}

void WasmScriptingVM::disarmWatchdog()
{
  BudgetSlot* slot = m_budgetSlot.get();
  takeDeadline(*slot);
  if (slot->fired.load())
  {
    reportError(budgetExceededLine(watchdogBudgetMs()));
    // The interrupt can land inside the module's allocator or
    // collector, so its heap is no longer trusted.
    poison();
    reportError(
        "script stopped: its module may have been interrupted mid-update");
  }
  wasm_runtime_rive_clear_interrupt(slot->env);
  m_budgetArmed = false;
}

WasmScriptingVM::CallOutcome WasmScriptingVM::callModuleChecked(
    const char* name,
    uint32_t argc,
    uint32_t* argv,
    uint32_t* result)
{
  if (m_poisoned)
  {
    return CallOutcome::trapped;
  }
  wasm_function_inst_t f = m_state->lookupExport(name);
  if (f == nullptr)
  {
    // Callers probing optional exports read the outcome; a plain
    // callModule folds this to zero, so probe before relying on it.
    return CallOutcome::missing;
  }
  // WAMR writes results into the argument cells, so both must fit buf.
  if (argc > 8 || wasmCallCells(m_state->instance, f, 8) != argc)
  {
    recordTrap(name, "export signature mismatch");
    return CallOutcome::trapped;
  }
  uint32_t buf[8] = {0};
  for (uint32_t i = 0; i < argc; i++)
  {
    buf[i] = argv[i];
  }
  bool ok;
  {
    WamrState::CallStateScope callState(*m_state);
    // Runtime services, the collector among them, must never be cut off.
    ScriptCallScope callScope(this, strncmp(name, "__rive", 6) != 0);
    ok = m_callDepth > 1
             ? wasm_runtime_call_wasm_nested(m_state->execEnv, f, argc, buf)
             : wasm_runtime_call_wasm(m_state->execEnv, f, argc, buf);
  }
  if (!ok)
  {
    reportTrap(name);
    return CallOutcome::trapped;
  }
  *result = buf[0];
  return CallOutcome::ok;
}

WasmScriptingVM::CallOutcome WasmScriptingVM::callF64(
    const char* name,
    const double* args,
    uint32_t argc,
    double* result)
{
  if (m_poisoned)
  {
    return CallOutcome::trapped;
  }
  wasm_function_inst_t f = m_state->lookupExport(name);
  if (f == nullptr)
  {
    return CallOutcome::missing;
  }
  wasm_module_inst_t inst = m_state->instance;
  uint32_t retCount = wasm_func_get_result_count(f, inst);
  // Modules load unsigned, so the export may not match.
  // host_obj_scroll_event takes the most.
  constexpr uint32_t kMaxArgs = 10;
  if (argc > kMaxArgs || wasm_func_get_param_count(f, inst) != argc ||
      retCount > 1)
  {
    recordTrap(name, "export signature mismatch");
    return CallOutcome::trapped;
  }
  wasm_valkind_t kinds[kMaxArgs];
  wasm_func_get_param_types(f, inst, kinds);
  wasm_val_t values[kMaxArgs];
  for (uint32_t i = 0; i < argc; i++)
  {
    values[i].kind = kinds[i];
    switch (kinds[i])
    {
      case WASM_F64:
        values[i].of.f64 = args[i];
        break;
      case WASM_F32:
        values[i].of.f32 = (float)args[i];
        break;
      case WASM_I64:
        values[i].of.i64 = (int64_t)args[i];
        break;
      default:
        values[i].of.i32 = (int32_t)(uint32_t)(int64_t)args[i];
        break;
    }
  }
  wasm_val_t ret[1];
  bool ok;
  {
    WamrState::CallStateScope callState(*m_state);
    ScriptCallScope callScope(this);
    ok = wasm_runtime_call_wasm_a(
        m_state->execEnv,
        f,
        retCount,
        ret,
        argc,
        values);
  }
  if (!ok)
  {
    reportTrap(name);
    return CallOutcome::trapped;
  }
  *result = 0;
  if (retCount != 0)
  {
    switch (ret[0].kind)
    {
      case WASM_F64:
        *result = ret[0].of.f64;
        break;
      case WASM_F32:
        *result = ret[0].of.f32;
        break;
      case WASM_I64:
        *result = (double)ret[0].of.i64;
        break;
      default:
        *result = ret[0].of.i32;
        break;
    }
  }
  return CallOutcome::ok;
}

bool WasmScriptingVM::hasExport(const char* name)
{
  return m_state->lookupExport(name) != nullptr;
}

void WasmScriptingVM::reportTrap(const char* where)
{
  wasm_module_inst_t inst = m_state->instance;
  recordTrap(where, wasm_runtime_get_exception(inst));
#if WASM_ENABLE_DUMP_CALL_STACK != 0
  if (!m_quietTraps)
  {
    wasm_runtime_dump_call_stack(m_state->execEnv);
  }
#endif
  // A pending exception makes WAMR refuse the next call, the collector's
  // included.
  wasm_runtime_clear_exception(inst);
}
#endif

void WasmScriptingVM::recordTrap(const char* where, const char* exception)
{
  // A silent fold hides real traps; name them so a script that dies
  // mid-call is diagnosable instead of a mystery no-op.
  exception = fullTrapMessage(exception);
  // AOT code traps as unreachable; both tiers name the timeout.
  m_lastTrap.budget = m_budgetRaised ||
                      (m_budgetSlot != nullptr && m_budgetSlot->fired.load());
  m_lastTrap.thrown = m_thrownRaised && !m_lastTrap.budget;
  m_budgetRaised = false;
  m_thrownRaised = false;
  if (m_lastTrap.budget)
  {
    exception = "execution exceeded timeout";
  }
  m_lastTrap.message = exception != nullptr ? exception : "";
  if (exception == nullptr || m_quietTraps)
  {
    return;
  }
  reportError(std::string("wasm call trapped in ") + where + ": " + exception);
  if (m_leakWarningCount > 0 && !m_leakTrapContextPrinted)
  {
    // A bare trap after leak warnings is almost always the memory
    // ceiling; say so once for hosts that dropped the warning strings.
    m_leakTrapContextPrinted = true;
    reportError(
        "wasm call trapped after " + std::to_string(m_leakWarningCount) +
        " script heap leak warnings; the module likely hit its "
        "wasmMaxPages ceiling");
  }
}

void WasmScriptingVM::reportError(const std::string& line)
{
  if (m_error)
  {
    m_error(line);
  }
  else
  {
    fprintf(stderr, "%s\n", line.c_str());
  }
}

void WasmScriptingVM::unregisterOpenCanvasFrame(uint32_t canvas)
{
  for (size_t i = 0; i < m_openCanvasFrames.size(); i++)
  {
    if (m_openCanvasFrames[i].canvas == canvas)
    {
      m_openCanvasFrames.erase(m_openCanvasFrames.begin() + i);
      return;
    }
  }
}

std::vector<uint32_t> WasmScriptingVM::takeOpenCanvasFramesFrom(uint64_t token)
{
  std::vector<uint32_t> taken;
  size_t keep = 0;
  for (size_t i = 0; i < m_openCanvasFrames.size(); i++)
  {
    if (m_openCanvasFrames[i].token >= token)
    {
      taken.push_back(m_openCanvasFrames[i].canvas);
    }
    else
    {
      m_openCanvasFrames[keep++] = m_openCanvasFrames[i];
    }
  }
  m_openCanvasFrames.resize(keep);
  return taken;
}

#ifndef __EMSCRIPTEN__
void* WasmScriptingVM::resolveModulePtr(uint32_t appAddr, uint32_t size)
{
  return wasmAppBytes(m_state->instance, appAddr, size);
}

const char* WasmScriptingVM::resolveModuleString(uint32_t appAddr)
{
  // The terminator has to lie inside linear memory for strlen to stop.
  wasm_module_inst_t inst = m_state->instance;
  uint64_t end = 0;
  if (!wasm_runtime_get_app_addr_range(inst, appAddr, nullptr, &end))
  {
    return nullptr;
  }
  auto str = (const char*)wasm_runtime_addr_app_to_native(inst, appAddr);
  return memchr(str, '\0', end - appAddr) != nullptr ? str : nullptr;
}

bool WasmScriptingVM::copyToModule(
    uint32_t appAddr,
    const void* src,
    uint32_t size)
{
  void* dst = resolveModulePtr(appAddr, size);
  if (dst == nullptr)
  {
    return false;
  }
  memcpy(dst, src, size);
  return true;
}

void WasmScriptingVM::raiseModuleError(const char* message)
{
  // The runtime's exception buffer truncates long messages (shader
  // compiler output); keep the full text for trap reporting.
  m_moduleErrorDetail = message;
  wasm_runtime_set_exception(m_state->instance, message);
}
#else
// The browser backend overrides every seam; these complete the vtable.
WasmScriptingVM::CallOutcome WasmScriptingVM::callModuleChecked(
    const char*,
    uint32_t,
    uint32_t*,
    uint32_t*)
{
  return CallOutcome::missing;
}
WasmScriptingVM::CallOutcome WasmScriptingVM::callF64(
    const char*,
    const double*,
    uint32_t,
    double*)
{
  return CallOutcome::missing;
}
bool WasmScriptingVM::hasExport(const char*) { return false; }
void* WasmScriptingVM::resolveModulePtr(uint32_t, uint32_t) { return nullptr; }
bool WasmScriptingVM::copyToModule(uint32_t, const void*, uint32_t)
{
  return false;
}
const char* WasmScriptingVM::resolveModuleString(uint32_t) { return nullptr; }
void WasmScriptingVM::raiseModuleError(const char*) {}
bool WasmScriptingVM::memoryLimits(uint32_t*, uint32_t*) const { return false; }
bool WasmScriptingVM::valid() const { return false; }
#endif

void WasmScriptingVM::raiseBudgetExceeded()
{
  m_budgetRaised = true;
  raiseModuleError("execution exceeded timeout");
}

void WasmScriptingVM::raiseThrown(const char* message)
{
  m_thrownRaised = true;
  raiseModuleError(message);
}

const char* WasmScriptingVM::fullTrapMessage(const char* exception) const
{
  const char* text = trapText(exception);
  if (text == nullptr)
  {
    return nullptr;
  }
  // Substitute only when the exception is a truncation of the detail, so
  // an unrelated later trap keeps its own message. An empty remainder
  // matches every prefix and must not adopt stale detail.
  if (*text != '\0' && !m_moduleErrorDetail.empty() &&
      m_moduleErrorDetail.compare(0, strlen(text), text) == 0)
  {
    return m_moduleErrorDetail.c_str();
  }
  return text;
}

#ifndef __EMSCRIPTEN__
WasmScriptingVM* WasmScriptingVMNatives::adoptBooting(wasm_exec_env_t env)
{
  // Instantiate assigns the same instance on success and null on failure.
  if (s_booting != nullptr)
  {
    s_booting->m_state->instance = wasm_runtime_get_module_inst(env);
  }
  return s_booting;
}
#else
void WasmScriptingVMNatives::booting(WasmScriptingVM* vm)
{
  // A top level's waits give up in time, as a call's do, and it neither
  // sees a region the call it nests in holds nor leaves one held.
  if (vm != nullptr)
  {
    vm->armDeadline();
    vm->m_outerBooting = s_booting;
    vm->m_outerRegion = stashHeldRegion(&vm->m_outerRegionId);
    s_booting = vm;
  }
  else if (WasmScriptingVM* ending = s_booting)
  {
    if (ending->m_deadlineArmed)
    {
      ending->disarmDeadline();
    }
    restoreHeldRegion(ending->m_outerRegionId, ending->m_outerRegion);
    s_booting = ending->m_outerBooting;
  }
  s_bootPrint = s_booting != nullptr ? &s_booting->m_print : nullptr;
}
#endif

std::unordered_map<std::string, int64_t>& WasmScriptingVMNatives::consoleTimers(
    WasmScriptingVM* vm)
{
  return vm->m_consoleTimers;
}

// The budget a metered module arms its own fuel with around its top level,
// as its calls do, or 0. A debugger pauses mid call, so it runs unbudgeted.
int WasmScriptingVMNatives::startFuelMs(const WasmScriptingVM* vm)
{
  return vm->m_fuelBudget && vm->m_debugHooks == nullptr && vm->m_timeoutMs > 0
             ? vm->m_timeoutMs
             : 0;
}

// Why a host that requires bounds will not start this module, or null.
const char* WasmScriptingVMNatives::startRefusal(const WasmScriptingVM* vm)
{
  if (!s_requireBounds.load() || vm->m_fuelBudget)
  {
    return nullptr;
  }
#ifdef __EMSCRIPTEN__
  return "this host runs only scripts it can bound, and a browser bounds "
         "only a module with fuel; bake it with wasmFuel";
#else
  return vm->m_hostBudget
             ? nullptr
             : "this host runs only scripts it can bound, AnimaScript with "
               "fuel or the watchdog, and dangerouslyFast bakes and Luau "
               "wasm modules have neither";
#endif
}

void WasmScriptingVMNatives::print(
    WasmScriptingVM* vm,
    const char* data,
    size_t size)
{
  const std::function<void(const char*, size_t)>* sink = vm != nullptr
                                                             ? &vm->m_print
                                                             : s_bootPrint;
  if (sink != nullptr && *sink)
  {
    (*sink)(data, size);
  }
  else
  {
    fwrite(data, 1, size, stdout);
  }
}

namespace
{

// A string argument as the impl cores read it, UTF-8. A module that hands
// over its own UTF-16 has it transcoded here, on the stack for anything the
// size of a name.
class WasmStringArg
{
public:
  WasmStringArg(WasmScriptingVM* vm, const char* bytes, uint32_t byteCount)
      : m_data(bytes)
      , m_size(byteCount)
  {
    // Module start calls in before the vm rides on the exec env.
    WasmScriptingVM* owner = vm != nullptr ? vm : s_booting;
    if (owner == nullptr || !owner->utf16Strings() || bytes == nullptr)
    {
      return;
    }
    auto in = reinterpret_cast<const uint8_t*>(bytes);
    // A unit is three bytes at most, a surrogate pair four for its two.
    size_t most = (size_t)(byteCount / 2) * 3;
    char* out = m_inline;
    if (most > sizeof(m_inline))
    {
      m_heap.resize(most);
      out = &m_heap[0];
    }
    m_data = out;
    for (uint32_t i = 0; i + 1 < byteCount;)
    {
      out += utf8Of(nextUtf16(in, byteCount, i), (uint8_t*)out);
    }
    m_size = (uint32_t)(out - m_data);
  }

  const char* data() const { return m_data; }
  uint32_t size() const { return m_size; }

private:
  const char* m_data;
  uint32_t m_size;
  char m_inline[192];
  std::string m_heap;
};

// Transcodes UTF-8 the way the module's own decoder would, writing only the
// units that fit; returns the full UTF-16 byte length.
size_t encodeUtf16(const char* data, size_t size, uint8_t* out, size_t capacity)
{
  auto in = reinterpret_cast<const uint8_t*>(data);
  size_t written = 0;
  auto unit = [&](uint32_t value) {
    if (written + 2 <= capacity)
    {
      out[written] = (uint8_t)value;
      out[written + 1] = (uint8_t)(value >> 8);
    }
    written += 2;
  };
  size_t i = 0;
  while (i < size)
  {
    uint32_t code = in[i];
    if (code < 0x80)
    {
      unit(code);
      i++;
      continue;
    }
    size_t tail = (code & 0xE0) == 0xC0 ? 1 : (code & 0xF0) == 0xE0 ? 2 : 3;
    if (i + tail >= size)
    {
      break;
    }
    code &= 0x3F >> tail;
    for (size_t k = 1; k <= tail; k++)
    {
      code = code << 6 | (in[i + k] & 0x3F);
    }
    i += tail + 1;
    if (code < 0x10000)
    {
      unit(code);
    }
    else
    {
      code -= 0x10000;
      unit(0xD800 | code >> 10);
      unit(0xDC00 | (code & 0x3FF));
    }
  }
  return written;
}

bool moduleReadsUtf16(WasmScriptingVM* vm)
{
  WasmScriptingVM* owner = vm != nullptr ? vm : s_booting;
  return owner != nullptr && owner->utf16HostStrings();
}

// A host string in the encoding the module reads.
std::string moduleStringBytes(
    WasmScriptingVM* vm,
    const char* data,
    size_t size)
{
  if (!moduleReadsUtf16(vm))
  {
    return std::string(data, size);
  }
  std::string out(encodeUtf16(data, size, nullptr, 0), '\0');
  encodeUtf16(data, size, reinterpret_cast<uint8_t*>(&out[0]), out.size());
  return out;
}

// The retrying length contract in the module's encoding: copies what fits,
// returns the full byte length.
uint32_t fillModuleString(
    WasmScriptingVM* vm,
    const std::string& value,
    char* buffer,
    uint32_t capacity)
{
  if (!moduleReadsUtf16(vm))
  {
    memcpy(buffer, value.data(), std::min<size_t>(value.size(), capacity));
    return (uint32_t)value.size();
  }
  return (uint32_t)encodeUtf16(
      value.data(),
      value.size(),
      reinterpret_cast<uint8_t*>(buffer),
      capacity);
}

#ifndef __EMSCRIPTEN__
// Module start runs inside wasm_runtime_instantiate, before the exec env
// carries the vm, so a native its top level reaches adopts the booting one;
// a lane that top level joined runs on this thread without one.
WasmScriptingVM* bootVmFromEnv(wasm_exec_env_t env)
{
  WasmScriptingVM* vm = vmFromEnv(env);
  return vm != nullptr || WasmLane::current() != nullptr
             ? vm
             : WasmScriptingVMNatives::adoptBooting(env);
}
#else
// As on WAMR, module start resolves no vm for the ops that do not trap.
WasmScriptingVM* vmFromHandle(uint32_t handle)
{
  auto vm = (WasmScriptingVM*)(uintptr_t)handle;
  return vm == s_booting ? nullptr : vm;
}

// Lanes pass 1 and reach only the shared ops, which all boot.
WasmScriptingVM* bootVmFromHandle(uint32_t handle)
{
  return handle == 1 ? nullptr : (WasmScriptingVM*)(uintptr_t)handle;
}
#endif

// Prototypes, descriptor PODs, and registration tables for the rive_*_v1
// namespaces come from the binding IDL (src/wasm/idl/bindings.py); the
// declarations pin each implementation below to the contract signature.
#include "wasm_natives_gen.hpp"

void consoleWrite(WasmScriptingVM* vm, const std::string& message)
{
  rtLogImpl(vm, 0, message.data(), (uint32_t)message.size());
}

void consoleTimeStart(WasmScriptingVM* vm, const std::string& label)
{
  if (vm != nullptr)
  {
    startConsoleTimer(WasmScriptingVMNatives::consoleTimers(vm), label);
  }
}

void consoleTimeReport(WasmScriptingVM* vm, const std::string& name, bool end)
{
  if (vm != nullptr)
  {
    consoleWrite(
        vm,
        reportConsoleTimer(
            WasmScriptingVMNatives::consoleTimers(vm),
            name,
            end));
  }
}

std::string traceNumber(double value)
{
  if (std::isnan(value))
  {
    return "NaN";
  }
  if (std::isinf(value))
  {
    return value < 0 ? "-Infinity" : "Infinity";
  }
  char text[32];
  if (value == std::trunc(value) && std::fabs(value) < 1e21)
  {
    snprintf(text, sizeof(text), "%.0f", value);
    return text;
  }
  for (int digits = 1; digits <= 17; digits++)
  {
    snprintf(text, sizeof(text), "%.*g", digits, value);
    if (strtod(text, nullptr) == value)
    {
      break;
    }
  }
  return text;
}

void traceWrite(
    WasmScriptingVM* vm,
    const std::string& message,
    int32_t count,
    const double* args)
{
  std::string line = "trace: " + message;
  for (int32_t i = 0; i < count && i < 5; i++)
  {
    line += ' ' + traceNumber(args[i]);
  }
  consoleWrite(vm, line);
}

#ifndef __EMSCRIPTEN__
// --- emscripten setjmp/longjmp glue -----------------------------------------

void throwLongjmpNative(wasm_exec_env_t env)
{
  wasm_runtime_set_exception(wasm_runtime_get_module_inst(env), kLongjmpTag);
}

void invokeViiNative(
    wasm_exec_env_t env,
    uint32_t index,
    uint32_t a1,
    uint32_t a2)
{
  WasmScriptingVM* vm = vmFromEnv(env);
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(env);
  // Module start has no exec env yet to restore the stack through.
  if (vm == nullptr)
  {
    wasm_runtime_set_exception(inst, "script module invoke at start");
    return;
  }
  uint32_t sp = vm->callModule("emscripten_stack_get_current", 0, nullptr);
  uint32_t argv[2] = {a1, a2};
  if (wasm_runtime_call_indirect(env, index, 2, argv))
  {
    return;
  }
  const char* exception = wasm_runtime_get_exception(inst);
  if (exception != nullptr && strstr(exception, kLongjmpTag) != nullptr)
  {
    wasm_runtime_clear_exception(inst);
    uint32_t restoreArgs[1] = {sp};
    vm->callModule("_emscripten_stack_restore", 1, restoreArgs);
    uint32_t threwArgs[2] = {1, 0};
    vm->callModule("setThrew", 2, threwArgs);
    return;
  }
  // Genuine trap: leave the exception set so it propagates outward.
}

// --- remaining emscripten env imports ---------------------------------------

// Module start reaches these too, before the exec env carries the vm, so
// they resolve memory through the booting one and trap when there is none.
WasmScriptingVM* envVm(wasm_exec_env_t env)
{
  WasmScriptingVM* vm = bootVmFromEnv(env);
  if (vm == nullptr)
  {
    wasm_runtime_set_exception(
        wasm_runtime_get_module_inst(env),
        "script module import without a vm");
  }
  return vm;
}

void abortJs(wasm_exec_env_t env)
{
  wasm_runtime_set_exception(
      wasm_runtime_get_module_inst(env),
      "script module abort");
}

void memcpyJs(wasm_exec_env_t env, uint32_t dst, uint32_t src, uint32_t n)
{
  WasmScriptingVM* vm = envVm(env);
  if (vm == nullptr)
  {
    return;
  }
  void* dstPtr = vm->resolveModulePtr(dst, n);
  void* srcPtr = vm->resolveModulePtr(src, n);
  if (dstPtr == nullptr || srcPtr == nullptr)
  {
    wasm_runtime_set_exception(
        wasm_runtime_get_module_inst(env),
        "script module memcpy out of bounds");
    return;
  }
  memmove(dstPtr, srcPtr, n);
}

double rtNowImpl(WasmScriptingVM* vm)
{
  // Same dev hook as dateNow: clock() feeds Luau's default RNG seed. A
  // pinned clock also parks the module's execution budget.
  if (getenv("RIVE_WASM_FIXED_DATE") != nullptr)
  {
    return 0;
  }
  auto now = std::chrono::steady_clock::now().time_since_epoch();
  return std::chrono::duration<double, std::milli>(now).count();
}

double rtDateNowImpl(WasmScriptingVM* vm)
{
  // Dev hook: a pinned date (epoch ms) keeps wasm lanes reproducible when
  // harnesses A/B the same module over wall-clock-seeded content.
  static const char* fixed = getenv("RIVE_WASM_FIXED_DATE");
  if (fixed != nullptr)
  {
    return atof(fixed);
  }
  auto now = std::chrono::system_clock::now().time_since_epoch();
  return std::chrono::duration<double, std::milli>(now).count();
}

// The Luau host module reads the same clocks under the names emscripten
// gives them.
double getNow(wasm_exec_env_t env) { return rtNowImpl(nullptr); }
double dateNow(wasm_exec_env_t env) { return rtDateNowImpl(nullptr); }

// sbrk's growth request: enlarge linear memory to cover `size` bytes.
// The module's declared maximum still caps the growth.
uint32_t resizeHeap(wasm_exec_env_t env, uint32_t size)
{
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(env);
  wasm_memory_inst_t memory = wasm_runtime_get_memory(inst, 0);
  if (memory == nullptr)
  {
    return 0;
  }
  const uint64_t kPageSize = 65536;
  uint64_t wantPages = ((uint64_t)size + kPageSize - 1) / kPageSize;
  uint64_t curPages = wasm_memory_get_cur_page_count(memory);
  if (wantPages <= curPages)
  {
    return 1;
  }
  return wasm_runtime_enlarge_memory(inst, wantPages - curPages) ? 1 : 0;
}
#endif

static bool localTime(double epochSeconds, time_t& at, struct tm& local)
{
  at = (time_t)epochSeconds;
#ifdef _WIN32
  return localtime_s(&local, &at) == 0;
#else
  return localtime_r(&at, &local) != nullptr;
#endif
}

// Seconds the local rendering sits ahead of UTC.
static int32_t utcOffsetOf(struct tm local, time_t at)
{
#ifdef _WIN32
  return (int32_t)(_mkgmtime(&local) - at);
#else
  return (int32_t)(timegm(&local) - at);
#endif
}

#ifndef __EMSCRIPTEN__
// --- emscripten time imports: the Luau VM blob's os.date and os.time walk
// through these, so they serve the host's real calendar. Without WASM_BIGINT
// the module passes time_t as two i32 halves. struct tm is musl's wasm32
// layout: nine ints, the gmtoff long, then tm_zone, which the module's own
// libc fills.

static time_t moduleTime(uint32_t low, uint32_t high)
{
  return (time_t)(((uint64_t)high << 32) | low);
}

static void writeTm(
    wasm_exec_env_t env,
    uint32_t tmPtr,
    const struct tm& time,
    int32_t gmtoff)
{
  WasmScriptingVM* vm = envVm(env);
  int32_t* fields = vm != nullptr ? (int32_t*)vm->resolveModulePtr(tmPtr, 44)
                                  : nullptr;
  if (fields == nullptr)
  {
    return;
  }
  fields[0] = time.tm_sec;
  fields[1] = time.tm_min;
  fields[2] = time.tm_hour;
  fields[3] = time.tm_mday;
  fields[4] = time.tm_mon;
  fields[5] = time.tm_year;
  fields[6] = time.tm_wday;
  fields[7] = time.tm_yday;
  fields[8] = time.tm_isdst;
  fields[9] = gmtoff;
}

void gmtimeJs(wasm_exec_env_t env, uint32_t low, uint32_t high, uint32_t tmPtr)
{
  time_t at = moduleTime(low, high);
  struct tm utc;
#ifdef _WIN32
  if (gmtime_s(&utc, &at) != 0)
#else
  if (gmtime_r(&at, &utc) == nullptr)
#endif
  {
    return;
  }
  writeTm(env, tmPtr, utc, 0);
}

void localtimeJs(
    wasm_exec_env_t env,
    uint32_t low,
    uint32_t high,
    uint32_t tmPtr)
{
  time_t at;
  struct tm local;
  if (!localTime((double)moduleTime(low, high), at, local))
  {
    return;
  }
  writeTm(env, tmPtr, local, utcOffsetOf(local, at));
}

// The module's tzset: seconds west of UTC, whether the zone observes
// daylight saving, and the two zone names into its 17 byte buffers.
void tzsetJs(
    wasm_exec_env_t env,
    uint32_t timezonePtr,
    uint32_t daylightPtr,
    uint32_t stdNamePtr,
    uint32_t dstNamePtr)
{
  WasmScriptingVM* vm = envVm(env);
  if (vm == nullptr)
  {
    return;
  }
#ifdef _WIN32
  _tzset();
  long west = 0;
  int observesDst = 0;
  _get_timezone(&west);
  _get_daylight(&observesDst);
  const char* names[2] = {_tzname[0], _tzname[1]};
#elif defined(RIVE_NX)
  // nnSdk's libc has no tz globals, so read the zone off January and July
  // of this year, standard time being whichever sits further west.
  long west = 0;
  int observesDst = 0;
  const char* names[2] = {"", ""};
  time_t at;
  struct tm probe[2];
  if (localTime((double)time(nullptr), at, probe[0]))
  {
    int year = probe[0].tm_year;
    int32_t offset[2];
    bool ok = true;
    for (int i = 0; i < 2 && ok; i++)
    {
      struct tm start{};
      start.tm_year = year;
      start.tm_mon = i * 6;
      start.tm_mday = 1;
      ok = localTime((double)timegm(&start), at, probe[i]);
      offset[i] = utcOffsetOf(probe[i], at);
    }
    if (ok)
    {
      int std = offset[1] < offset[0] ? 1 : 0;
      west = -offset[std];
      observesDst = offset[0] != offset[1];
      names[0] = probe[std].tm_zone;
      names[1] = probe[observesDst ? 1 - std : std].tm_zone;
    }
  }
#else
  tzset();
  long west = timezone;
  int observesDst = daylight;
  const char* names[2] = {tzname[0], tzname[1]};
#endif
  if (int32_t* out = (int32_t*)vm->resolveModulePtr(timezonePtr, 4))
  {
    *out = (int32_t)west;
  }
  if (int32_t* out = (int32_t*)vm->resolveModulePtr(daylightPtr, 4))
  {
    *out = observesDst;
  }
  uint32_t namePtrs[2] = {stdNamePtr, dstNamePtr};
  for (int i = 0; i < 2; i++)
  {
    if (char* out = (char*)vm->resolveModulePtr(namePtrs[i], 17))
    {
      strncpy(out, names[i] != nullptr ? names[i] : "", 16);
      out[16] = '\0';
    }
  }
}

uint32_t strftimeNative(
    wasm_exec_env_t env,
    uint32_t out,
    uint32_t capacity,
    uint32_t format,
    uint32_t tmPtr)
{
  WasmScriptingVM* vm = envVm(env);
  if (vm == nullptr)
  {
    return 0;
  }
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(env);
  char* outPtr = (char*)vm->resolveModulePtr(out, capacity);
  const int32_t* fields = (const int32_t*)vm->resolveModulePtr(tmPtr, 44);
  if (outPtr == nullptr || fields == nullptr ||
      !wasm_runtime_validate_app_str_addr(inst, format))
  {
    return 0;
  }
  struct tm time = {};
  time.tm_sec = fields[0];
  time.tm_min = fields[1];
  time.tm_hour = fields[2];
  time.tm_mday = fields[3];
  time.tm_mon = fields[4];
  time.tm_year = fields[5];
  time.tm_wday = fields[6];
  time.tm_yday = fields[7];
  time.tm_isdst = fields[8];
#ifndef _WIN32
  time.tm_gmtoff = fields[9];
  // The zone name lives in module memory, so it must be translated before
  // the host's %Z reads it.
  uint32_t zone = (uint32_t)fields[10];
  if (zone != 0 && wasm_runtime_validate_app_str_addr(inst, zone))
  {
    time.tm_zone = (char*)wasm_runtime_addr_app_to_native(inst, zone);
  }
#endif
  return (uint32_t)strftime(
      outPtr,
      capacity,
      (const char*)wasm_runtime_addr_app_to_native(inst, format),
      &time);
}

// Older libc++ routes stream formatting through the locale variant.
uint32_t strftimeLNative(
    wasm_exec_env_t env,
    uint32_t out,
    uint32_t capacity,
    uint32_t format,
    uint32_t tmPtr,
    uint32_t locale)
{
  return strftimeNative(env, out, capacity, format, tmPtr);
}

// A module assert would otherwise trap with no message.
void assertFail(
    wasm_exec_env_t env,
    uint32_t message,
    uint32_t file,
    uint32_t line,
    uint32_t function)
{
  WasmScriptingVM* vm = bootVmFromEnv(env);
  auto text = [vm](uint32_t ptr) {
    const char* str = ptr != 0 && vm != nullptr ? vm->resolveModuleString(ptr)
                                                : nullptr;
    return str != nullptr ? str : "?";
  };
  reportScriptError(
      vm,
      std::string("module assertion failed: ") + text(message) + " (" +
          text(file) + ":" + std::to_string(line) + ", " + text(function) +
          ")");
  wasm_runtime_set_exception(
      wasm_runtime_get_module_inst(env),
      "module assertion failed");
}

// The blob's print goes through fd_write; intercept it so script output
// reaches the host's sink instead of the process stdout. The signature must
// match the libc-wasi builtin's "(i*i*)i": wamrc resolves WASI imports
// against the builtin table at compile time and inlines the app-to-native
// conversion for the '*' params, so an int-only registration receives
// native pointers under AOT while the interpreter honors the raw ints.
uint32_t fdWrite(
    wasm_exec_env_t env,
    uint32_t fd,
    uint32_t* iovs,
    uint32_t iovcnt,
    uint32_t* nwritten)
{
  WasmScriptingVM* vm = envVm(env);
  if (vm == nullptr)
  {
    return 28; // EINVAL
  }
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(env);
  if (!wasm_runtime_validate_native_addr(inst, iovs, (uint64_t)iovcnt * 8) ||
      !wasm_runtime_validate_native_addr(inst, nwritten, 4))
  {
    return 28; // EINVAL
  }
  uint32_t total = 0;
  for (uint32_t i = 0; i < iovcnt; i++)
  {
    uint32_t* io = iovs + i * 2;
    uint32_t ptr = io[0], len = io[1];
    const char* data = len != 0 ? (const char*)vm->resolveModulePtr(ptr, len)
                                : nullptr;
    if (data != nullptr)
    {
      // Module stderr carries script errors; keep it out of the print
      // sink so a capturing harness cannot swallow them.
      if (fd == 2)
      {
        fwrite(data, 1, len, stderr);
      }
      else
      {
        WasmScriptingVMNatives::print(vm, data, len);
      }
      total += len;
    }
  }
  *nwritten = total;
  return 0;
}

// vm_host externalizes a handful of rive C++ methods as env imports that
// the runtime path never reaches (the live GPU path goes through the
// rive_gpu_v1 natives, not these direct calls). The fast interpreter
// tolerates an unresolved import, trapping only on an actual call, but
// wamrc bakes a null call target into the AOT that faults the moment a
// static initializer takes its address. Registering inert stubs gives the
// AOT valid pointers. Signatures mirror vm_host's import types.
uint32_t deadEnvImport_i_i(wasm_exec_env_t, uint32_t) { return 0; }
void deadCoreBytesDeserialize(wasm_exec_env_t, uint32_t, uint32_t) {}
void deadCoreStringDeserialize(wasm_exec_env_t, uint32_t, uint32_t) {}
void deadBeginFrame(wasm_exec_env_t, uint32_t, uint32_t) {}
void deadFlush(wasm_exec_env_t, uint32_t, uint32_t) {}
void deadAddFileAsset(wasm_exec_env_t, uint32_t, uint32_t) {}
uint32_t deadEnvImport_ii_i(wasm_exec_env_t, uint32_t, uint32_t) { return 0; }
void deadEnvImport_iiii(wasm_exec_env_t, uint32_t, uint32_t, uint32_t, uint32_t)
{}

// __cxa_throw never returns; a no-op stub falls through to the module's
// `unreachable` and corrupts the shadow stack under AOT. Trap instead so
// the call unwinds like the interpreter's unlinked-import path, and name
// the thrown type from its Itanium type_info (wasm32: __type_name at +4).
// Registered with libc-builtin's "(**i)": wamrc knows that table and
// inlines the pointer conversion, so an int-only registration would
// receive native pointers under AOT.
void cxaThrowNative(
    wasm_exec_env_t env,
    void* thrown,
    void* tinfo,
    uint32_t dest)
{
  (void)thrown;
  (void)dest;
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(env);
  const char* typeName = "?";
  if (tinfo != nullptr && wasm_runtime_validate_native_addr(inst, tinfo, 8))
  {
    uint32_t nameApp = ((uint32_t*)tinfo)[1];
    if (wasm_runtime_validate_app_str_addr(inst, nameApp))
    {
      typeName = (const char*)wasm_runtime_addr_app_to_native(inst, nameApp);
    }
  }
  reportScriptError(
      bootVmFromEnv(env),
      std::string("[wasm] C++ exception thrown: ") + typeName);
  wasm_runtime_set_exception(inst, "c++ exception");
}

// Math.random's seed. A deterministic run's seed comes first, so --seed
// picks the same sequence here as in Luau; the fixed date env that pins
// script time pins it too, to 1, the deterministic default.
double envSeed(wasm_exec_env_t)
{
  if (File::deterministicMode)
  {
    return (double)File::deterministicSeed;
  }
  if (getenv("RIVE_WASM_FIXED_DATE") != nullptr)
  {
    return 1.0;
  }
  return (double)std::chrono::steady_clock::now().time_since_epoch().count();
}

// The std's console passes a managed string: UTF-16 whose byte length is the
// word ahead of it.
std::string consoleString(wasm_exec_env_t env, uint32_t text)
{
  uint32_t length = 0;
  auto data = (const char*)
      wasmConsoleText(wasm_runtime_get_module_inst(env), text, length);
  if (data == nullptr)
  {
    return std::string();
  }
  WasmStringArg utf8(vmFromEnv(env), data, length);
  return std::string(utf8.data(), utf8.size());
}

void consoleLog(wasm_exec_env_t env, uint32_t text)
{
  if (WasmLane* lane = WasmLane::current())
  {
    lane->logConsole(env, text);
    return;
  }
  consoleWrite(vmFromEnv(env), consoleString(env, text));
}

void consoleTime(wasm_exec_env_t env, uint32_t label)
{
  if (WasmLane* lane = WasmLane::current())
  {
    lane->consoleTimer(env, label, false, false);
    return;
  }
  consoleTimeStart(bootVmFromEnv(env), consoleString(env, label));
}

void consoleTimeLog(wasm_exec_env_t env, uint32_t label)
{
  if (WasmLane* lane = WasmLane::current())
  {
    lane->consoleTimer(env, label, true, false);
    return;
  }
  consoleTimeReport(bootVmFromEnv(env), consoleString(env, label), false);
}

void consoleTimeEnd(wasm_exec_env_t env, uint32_t label)
{
  if (WasmLane* lane = WasmLane::current())
  {
    lane->consoleTimer(env, label, true, true);
    return;
  }
  consoleTimeReport(bootVmFromEnv(env), consoleString(env, label), true);
}

void envTrace(
    wasm_exec_env_t env,
    uint32_t message,
    int32_t count,
    double a0,
    double a1,
    double a2,
    double a3,
    double a4)
{
  double args[5] = {a0, a1, a2, a3, a4};
  traceWrite(vmFromEnv(env), consoleString(env, message), count, args);
}

NativeSymbol kEnvNatives[] = {
    {"console.log", (void*)consoleLog, "(i)", nullptr},
    {"console.debug", (void*)consoleLog, "(i)", nullptr},
    {"console.info", (void*)consoleLog, "(i)", nullptr},
    {"console.warn", (void*)consoleLog, "(i)", nullptr},
    {"console.error", (void*)consoleLog, "(i)", nullptr},
    {"console.time", (void*)consoleTime, "(i)", nullptr},
    {"console.timeLog", (void*)consoleTimeLog, "(i)", nullptr},
    {"console.timeEnd", (void*)consoleTimeEnd, "(i)", nullptr},
    {"trace", (void*)envTrace, "(iiFFFFF)", nullptr},
    {"invoke_vii", (void*)invokeViiNative, "(iii)", nullptr},
    {"seed", (void*)envSeed, "()F", nullptr},
    {"_ZN4rive12CoreUintType11deserializeERNS_12BinaryReaderE",
     (void*)deadEnvImport_i_i,
     "(i)i",
     nullptr},
    {"_ZN4rive13CoreBytesType11deserializeERNS_12BinaryReaderE",
     (void*)deadCoreBytesDeserialize,
     "(ii)",
     nullptr},
    {"_ZN4rive14CoreStringType11deserializeERNS_12BinaryReaderE",
     (void*)deadCoreStringDeserialize,
     "(ii)",
     nullptr},
    {"_ZN4rive3gpu13RenderContext24makeDeferredRenderCanvasEjj",
     (void*)deadEnvImport_iiii,
     "(iiii)",
     nullptr},
    {"_ZN4rive3gpu13RenderContext16makeRenderCanvasEjj",
     (void*)deadEnvImport_iiii,
     "(iiii)",
     nullptr},
    {"_ZN4rive3gpu13RenderContext10beginFrameERKNS1_15FrameDescriptorE",
     (void*)deadBeginFrame,
     "(ii)",
     nullptr},
    {"_ZN4rive12RiveRendererC1EPNS_3gpu13RenderContextE",
     (void*)deadEnvImport_ii_i,
     "(ii)i",
     nullptr},
    {"_ZN4rive3gpu13RenderContext5flushERKNS1_14FlushResourcesE",
     (void*)deadFlush,
     "(ii)",
     nullptr},
    {"_ZN4rive17BackboardImporter12addFileAssetENS_3rcpINS_9FileAssetEEE",
     (void*)deadAddFileAsset,
     "(ii)",
     nullptr},
    {"__cxa_throw", (void*)cxaThrowNative, "(**i)", nullptr},
    {"_emscripten_throw_longjmp", (void*)throwLongjmpNative, "()", nullptr},
    {"_abort_js", (void*)abortJs, "()", nullptr},
    {"_emscripten_memcpy_js", (void*)memcpyJs, "(iii)", nullptr},
    {"emscripten_get_now", (void*)getNow, "()F", nullptr},
    {"emscripten_date_now", (void*)dateNow, "()F", nullptr},
    {"emscripten_resize_heap", (void*)resizeHeap, "(i)i", nullptr},
    {"strftime", (void*)strftimeNative, "(iiii)i", nullptr},
    {"strftime_l", (void*)strftimeLNative, "(iiiii)i", nullptr},
    {"_tzset_js", (void*)tzsetJs, "(iiii)", nullptr},
    {"_localtime_js", (void*)localtimeJs, "(iii)", nullptr},
    {"_gmtime_js", (void*)gmtimeJs, "(iii)", nullptr},
    {"__assert_fail", (void*)assertFail, "(iiii)", nullptr},
};

// Module streams are not seekable. The blob imports the emscripten-legalized
// form, offset i64 split across two i32s.
uint32_t fdSeek(
    wasm_exec_env_t env,
    uint32_t fd,
    uint32_t offsetLow,
    uint32_t offsetHigh,
    uint32_t whence,
    uint32_t newOffsetPtr)
{
  return 70; // WASI ESPIPE
}

// The blob's libc asks for the environment at start and closes its streams
// at exit; there is no environment and the streams are the log.
uint32_t environSizesGet(wasm_exec_env_t env, uint32_t* count, uint32_t* size)
{
  wasm_module_inst_t inst = wasm_runtime_get_module_inst(env);
  if (!wasm_runtime_validate_native_addr(inst, count, 4) ||
      !wasm_runtime_validate_native_addr(inst, size, 4))
  {
    return 21; // WASI EFAULT
  }
  *count = 0;
  *size = 0;
  return 0;
}

uint32_t environGet(wasm_exec_env_t env, uint32_t* environ, char* buffer)
{
  return 0;
}

uint32_t fdClose(wasm_exec_env_t env, uint32_t fd) { return 0; }

NativeSymbol kWasiNatives[] = {
    {"fd_write", (void*)fdWrite, "(i*i*)i", nullptr},
    {"fd_seek", (void*)fdSeek, "(iiiii)i", nullptr},
    {"environ_sizes_get", (void*)environSizesGet, "(**)i", nullptr},
    {"environ_get", (void*)environGet, "(**)i", nullptr},
    {"fd_close", (void*)fdClose, "(i)i", nullptr},
};

#endif

void deleteHostObject(
    WasmScriptingVM* vm,
    WasmScriptingVM::HandleTable::Tag tag,
    void* object);

void releaseHostHandle(
    WasmScriptingVM* vm,
    uint32_t handle,
    WasmScriptingVM::HandleTable::Tag tag)
{
  void* object = vm != nullptr ? vm->handles().resolve(handle, tag) : nullptr;
  if (object == nullptr)
  {
    return;
  }
  deleteHostObject(vm, tag, object);
  vm->handles().release(handle, tag);
}

// --- rive_path/paint/renderer_v1: handle-backed render objects --------------

// The module's ModuleRenderPath mirror; geometry arrives through update and
// rebuilds the real render path.
// Size then fill: copies when it fits, always answers the full count.
template <typename T>
uint32_t fillOut(Span<const T> words, T* out, uint32_t outCount)
{
  if (out != nullptr && words.size() <= outCount)
  {
    memcpy(out, words.data(), words.size() * sizeof(T));
  }
  return (uint32_t)words.size();
}

template <typename T>
uint32_t fillOut(const std::vector<T>& words, T* out, uint32_t outCount)
{
  return fillOut(Span<const T>(words.data(), words.size()), out, outCount);
}

// A RawPath's points as the flat float stream the modules read.
Span<const float> pathFloats(const RawPath& path)
{
  return Span<const float>(
      (const float*)path.points().data(),
      path.points().size() * 2);
}

Span<const uint8_t> pathVerbBytes(const RawPath& path)
{
  return Span<const uint8_t>(
      (const uint8_t*)path.verbs().data(),
      path.verbs().size());
}

// The geometry stays host side so paths composed from other paths never
// cross back; the render path rebuilds lazily after a host side append.
struct HostPath
{
  RawPath raw;
  FillRule fillRule = FillRule::clockwise;
  rcp<RenderPath> path;
  bool renderDirty = false;
};

HostPath* resolvePath(WasmScriptingVM* vm, uint32_t handle)
{
  if (vm == nullptr)
  {
    return nullptr;
  }
  return static_cast<HostPath*>(
      vm->handles().resolve(handle, WasmScriptingVM::HandleTable::Tag::path));
}

void rebuildRenderPath(WasmScriptingVM* vm, HostPath* hostPath)
{
  // makeRenderPath may take the arrays it is handed, and raw stays the
  // geometry add and the readbacks use.
  RawPath copy(hostPath->raw);
  hostPath->path = vm->factory()->makeRenderPath(copy, hostPath->fillRule);
  hostPath->renderDirty = false;
}

RenderPath* hostRenderPath(WasmScriptingVM* vm, HostPath* hostPath)
{
  if (hostPath == nullptr)
  {
    return nullptr;
  }
  if (hostPath->renderDirty)
  {
    rebuildRenderPath(vm, hostPath);
  }
  return hostPath->path.get();
}

struct HostPaint
{
  rcp<RenderPaint> paint;
};

// Enums cross as ordinals; one out of range raises as a bad name does in the
// Luau lane, since the renderer's switches treat other values as unreachable.
bool validOrdinal(
    WasmScriptingVM* vm,
    uint32_t value,
    bool valid,
    const char* type)
{
  if (!valid)
  {
    char message[64];
    snprintf(message, sizeof(message), "%u is not a valid %s", value, type);
    vm->raiseModuleError(message);
  }
  return valid;
}

bool validBlendMode(WasmScriptingVM* vm, uint32_t value)
{
  return validOrdinal(
      vm,
      value,
      value == (uint32_t)BlendMode::srcOver ||
          value == (uint32_t)BlendMode::additive ||
          (value >= (uint32_t)BlendMode::screen &&
           value <= (uint32_t)BlendMode::luminosity),
      "BlendMode");
}

// Backends index a sampler table with the key.
bool validSamplerKey(WasmScriptingVM* vm, uint32_t key)
{
  return validOrdinal(
      vm,
      key,
      key < ImageSampler::MAX_SAMPLER_PERMUTATIONS,
      "ImageSampler key");
}

// RawPath walkers trust a stream to open with a move and its verbs to take
// exactly its points, so guest geometry is checked before it becomes one.
bool guestRawPath(
    WasmScriptingVM* vm,
    const uint8_t* verbs,
    uint32_t verbCount,
    const float* points,
    uint32_t floatCount,
    RawPath* out)
{
  uint64_t floats = 0;
  bool valid = verbCount == 0 || verbs[0] == (uint8_t)PathVerb::move;
  for (uint32_t i = 0; valid && i < verbCount; i++)
  {
    switch ((PathVerb)verbs[i])
    {
      case PathVerb::move:
      case PathVerb::line:
        floats += 2;
        continue;
      case PathVerb::quad:
        floats += 4;
        continue;
      case PathVerb::cubic:
        floats += 6;
        continue;
      case PathVerb::close:
        continue;
    }
    valid = false;
  }
  if (!valid || floats != floatCount)
  {
    vm->raiseModuleError("Path: verbs and points do not match");
    return false;
  }
  *out = RawPath(
      Span<const PathVerb>((const PathVerb*)verbs, verbCount),
      Span<const Vec2D>((const Vec2D*)points, floatCount / 2));
  return true;
}

uint32_t pathNewImpl(WasmScriptingVM* vm)
{
  if (vm == nullptr)
  {
    return 0;
  }
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::path,
      new HostPath());
}

void pathUpdateImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    const uint8_t* verbs,
    uint32_t verbCount,
    const float* points,
    uint32_t floatCount,
    uint32_t fillRule)
{
  if (vm == nullptr)
  {
    return;
  }
  auto hostPath = static_cast<HostPath*>(
      vm->handles().resolve(handle, WasmScriptingVM::HandleTable::Tag::path));
  if (hostPath == nullptr ||
      !validOrdinal(
          vm,
          fillRule,
          fillRule <= (uint32_t)FillRule::clockwise,
          "FillRule") ||
      !guestRawPath(vm, verbs, verbCount, points, floatCount, &hostPath->raw))
  {
    return;
  }
  hostPath->fillRule = (FillRule)fillRule;
  rebuildRenderPath(vm, hostPath);
}

void pathAddImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint32_t otherHandle,
    float xx,
    float xy,
    float yx,
    float yy,
    float tx,
    float ty)
{
  auto hostPath = resolvePath(vm, handle);
  auto other = resolvePath(vm, otherHandle);
  if (hostPath == nullptr || other == nullptr)
  {
    return;
  }
  Mat2D transform(xx, xy, yx, yy, tx, ty);
  hostPath->raw.addPath(other->raw, &transform);
  hostPath->renderDirty = true;
}

uint32_t pathVerbsImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint8_t* out,
    uint32_t outCount)
{
  auto hostPath = resolvePath(vm, handle);
  if (hostPath == nullptr)
  {
    return 0;
  }
  return fillOut(pathVerbBytes(hostPath->raw), out, outCount);
}

uint32_t pathPointsImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    float* out,
    uint32_t outCount)
{
  auto hostPath = resolvePath(vm, handle);
  if (hostPath == nullptr)
  {
    return 0;
  }
  return fillOut(pathFloats(hostPath->raw), out, outCount);
}

void pathEffectResultImpl(
    WasmScriptingVM* vm,
    const uint8_t* verbs,
    uint32_t verbCount,
    const float* points,
    uint32_t floatCount)
{
  RawPath rawPath;
  if (vm == nullptr || vm->pathEffectOut() == nullptr ||
      !guestRawPath(vm, verbs, verbCount, points, floatCount, &rawPath))
  {
    return;
  }
  vm->pathEffectOut()->addPath(rawPath);
}

// One handle kind for the three measure shapes: a whole path measure, a
// contour iterator (measures nothing itself) and a contour it produced.
// The geometry is copied so the module's path can change underneath.
struct HostMeasure
{
  std::unique_ptr<RawPath> source;
  std::unique_ptr<PathMeasure> path;
  std::unique_ptr<ContourMeasureIter> iter;
  rcp<ContourMeasure> contour;
  RawPath segment;

  float length() const
  {
    return path ? path->length() : contour ? contour->length() : 0.0f;
  }

  bool isClosed() const
  {
    return path ? path->isClosed() : contour ? contour->isClosed() : false;
  }

  ContourMeasure::PosTan posTan(float distance) const
  {
    if (path)
    {
      auto at = path->atDistance(distance);
      return {at.pos, at.tan};
    }
    if (contour)
    {
      return contour->getPosTan(distance);
    }
    return {};
  }

  void getSegment(float start, float end, RawPath* dst, bool startWithMove)
  {
    if (path)
    {
      path->getSegment(start, end, dst, startWithMove);
    }
    else if (contour)
    {
      contour->getSegment(start, end, dst, startWithMove);
    }
  }
};

HostMeasure* resolveMeasure(WasmScriptingVM* vm, uint32_t handle)
{
  if (vm == nullptr)
  {
    return nullptr;
  }
  return static_cast<HostMeasure*>(vm->handles().resolve(
      handle,
      WasmScriptingVM::HandleTable::Tag::measure));
}

std::unique_ptr<RawPath> copyGeometry(
    WasmScriptingVM* vm,
    const uint8_t* verbs,
    uint32_t verbCount,
    const float* points,
    uint32_t floatCount)
{
  auto path = std::make_unique<RawPath>();
  if (vm == nullptr ||
      !guestRawPath(vm, verbs, verbCount, points, floatCount, path.get()))
  {
    return nullptr;
  }
  return path;
}

uint32_t measurePathNewImpl(
    WasmScriptingVM* vm,
    const uint8_t* verbs,
    uint32_t verbCount,
    const float* points,
    uint32_t floatCount)
{
  auto source = copyGeometry(vm, verbs, verbCount, points, floatCount);
  if (source == nullptr)
  {
    return 0;
  }
  auto measure = new HostMeasure();
  measure->source = std::move(source);
  measure->path = std::make_unique<PathMeasure>(measure->source.get());
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::measure,
      measure);
}

uint32_t measureContoursNewImpl(
    WasmScriptingVM* vm,
    const uint8_t* verbs,
    uint32_t verbCount,
    const float* points,
    uint32_t floatCount)
{
  auto source = copyGeometry(vm, verbs, verbCount, points, floatCount);
  if (source == nullptr)
  {
    return 0;
  }
  auto measure = new HostMeasure();
  measure->source = std::move(source);
  measure->iter = std::make_unique<ContourMeasureIter>(measure->source.get());
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::measure,
      measure);
}

uint32_t measureContourNextImpl(WasmScriptingVM* vm, uint32_t handle)
{
  HostMeasure* iter = resolveMeasure(vm, handle);
  if (iter == nullptr || !iter->iter)
  {
    return 0;
  }
  rcp<ContourMeasure> next = iter->iter->next();
  if (next == nullptr)
  {
    return 0;
  }
  auto measure = new HostMeasure();
  measure->contour = std::move(next);
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::measure,
      measure);
}

float measureLengthImpl(WasmScriptingVM* vm, uint32_t handle)
{
  HostMeasure* measure = resolveMeasure(vm, handle);
  return measure != nullptr ? measure->length() : 0.0f;
}

uint32_t measureIsClosedImpl(WasmScriptingVM* vm, uint32_t handle)
{
  HostMeasure* measure = resolveMeasure(vm, handle);
  return measure != nullptr && measure->isClosed() ? 1 : 0;
}

void measurePosTanImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    float distance,
    float* out,
    uint32_t outCount)
{
  HostMeasure* measure = resolveMeasure(vm, handle);
  if (measure == nullptr || out == nullptr || outCount < 4)
  {
    return;
  }
  auto posTan = measure->posTan(distance);
  out[0] = posTan.pos.x;
  out[1] = posTan.pos.y;
  out[2] = posTan.tan.x;
  out[3] = posTan.tan.y;
}

void measureWarpImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    float x,
    float y,
    float* out,
    uint32_t outCount)
{
  HostMeasure* measure = resolveMeasure(vm, handle);
  if (measure == nullptr || out == nullptr || outCount < 2)
  {
    return;
  }
  auto posTan = measure->posTan(x);
  out[0] = posTan.pos.x - posTan.tan.y * y;
  out[1] = posTan.pos.y + posTan.tan.x * y;
}

uint32_t measureExtractImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    float startDistance,
    float endDistance,
    uint32_t startWithMove)
{
  HostMeasure* measure = resolveMeasure(vm, handle);
  if (measure == nullptr)
  {
    return 0;
  }
  measure->segment.rewind();
  measure->getSegment(
      startDistance,
      endDistance,
      &measure->segment,
      startWithMove != 0);
  return (uint32_t)measure->segment.verbs().size();
}

uint32_t measureExtractReadImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint8_t* verbs,
    uint32_t verbCount,
    float* points,
    uint32_t floatCount)
{
  HostMeasure* measure = resolveMeasure(vm, handle);
  if (measure == nullptr)
  {
    return 0;
  }
  const RawPath& segment = measure->segment;
  uint32_t floats = (uint32_t)segment.points().size() * 2;
  if (verbCount < segment.verbs().size() || floatCount < floats)
  {
    return 0;
  }
  memcpy(verbs, segment.verbs().data(), segment.verbs().size());
  memcpy(points, segment.points().data(), floats * sizeof(float));
  return floats;
}

void measureReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
  releaseHostHandle(vm, handle, WasmScriptingVM::HandleTable::Tag::measure);
}

void pathReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
  releaseHostHandle(vm, handle, WasmScriptingVM::HandleTable::Tag::path);
}

uint32_t paintNewImpl(WasmScriptingVM* vm)
{
  if (vm == nullptr)
  {
    return 0;
  }
  auto hostPaint = new HostPaint{vm->factory()->makeRenderPaint()};
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::paint,
      hostPaint);
}

void paintReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
  releaseHostHandle(vm, handle, WasmScriptingVM::HandleTable::Tag::paint);
}

RenderPaint* resolvePaint(WasmScriptingVM* vm, uint32_t handle)
{
  if (vm == nullptr)
  {
    return nullptr;
  }
  auto hostPaint = static_cast<HostPaint*>(
      vm->handles().resolve(handle, WasmScriptingVM::HandleTable::Tag::paint));
  return hostPaint != nullptr ? hostPaint->paint.get() : nullptr;
}

void paintStyleImpl(WasmScriptingVM* vm, uint32_t handle, uint32_t value)
{
  if (auto paint = resolvePaint(vm, handle))
  {
    paint->style((RenderPaintStyle)value);
  }
}
void paintColorImpl(WasmScriptingVM* vm, uint32_t handle, uint32_t value)
{
  if (auto paint = resolvePaint(vm, handle))
  {
    paint->color(value);
  }
}
void paintThicknessImpl(WasmScriptingVM* vm, uint32_t handle, float value)
{
  if (auto paint = resolvePaint(vm, handle))
  {
    paint->thickness(value);
  }
}
void paintJoinImpl(WasmScriptingVM* vm, uint32_t handle, uint32_t value)
{
  auto paint = resolvePaint(vm, handle);
  if (paint != nullptr && validOrdinal(
                              vm,
                              value,
                              value <= (uint32_t)StrokeJoin::bevel,
                              "StrokeJoin"))
  {
    paint->join((StrokeJoin)value);
  }
}
void paintCapImpl(WasmScriptingVM* vm, uint32_t handle, uint32_t value)
{
  auto paint = resolvePaint(vm, handle);
  if (paint != nullptr && validOrdinal(
                              vm,
                              value,
                              value <= (uint32_t)StrokeCap::square,
                              "StrokeCap"))
  {
    paint->cap((StrokeCap)value);
  }
}
void paintBlendModeImpl(WasmScriptingVM* vm, uint32_t handle, uint32_t value)
{
  auto paint = resolvePaint(vm, handle);
  if (paint != nullptr && validBlendMode(vm, value))
  {
    paint->blendMode((BlendMode)value);
  }
}
void paintFeatherImpl(WasmScriptingVM* vm, uint32_t handle, float value)
{
  if (auto paint = resolvePaint(vm, handle))
  {
    paint->feather(value);
  }
}

struct HostShader
{
  rcp<RenderShader> shader;
};

// Both gradients read count colors and count stops from the module.
bool resolveGradientStops(
    WasmScriptingVM* vm,
    uint32_t colorsPtr,
    uint32_t stopsPtr,
    uint32_t count,
    const ColorInt*& colors,
    const float*& stops)
{
  // A larger count wraps the byte size, which then passes the bounds check.
  if (count > UINT32_MAX / 4)
  {
    return false;
  }
  colors = (const ColorInt*)vm->resolveModulePtr(colorsPtr, count * 4);
  stops = (const float*)vm->resolveModulePtr(stopsPtr, count * 4);
  return colors != nullptr && stops != nullptr;
}

uint32_t shaderLinearImpl(
    WasmScriptingVM* vm,
    float sx,
    float sy,
    float ex,
    float ey,
    uint32_t colorsPtr,
    uint32_t stopsPtr,
    uint32_t count)
{
  if (vm == nullptr)
  {
    return 0;
  }
  const ColorInt* colors = nullptr;
  const float* stops = nullptr;
  if (!resolveGradientStops(vm, colorsPtr, stopsPtr, count, colors, stops))
  {
    return 0;
  }
  auto shader = vm->factory()
                    ->makeLinearGradient(sx, sy, ex, ey, colors, stops, count);
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::shader,
      new HostShader{std::move(shader)});
}

uint32_t shaderRadialImpl(
    WasmScriptingVM* vm,
    float cx,
    float cy,
    float radius,
    uint32_t colorsPtr,
    uint32_t stopsPtr,
    uint32_t count)
{
  if (vm == nullptr)
  {
    return 0;
  }
  const ColorInt* colors = nullptr;
  const float* stops = nullptr;
  if (!resolveGradientStops(vm, colorsPtr, stopsPtr, count, colors, stops))
  {
    return 0;
  }
  auto shader = vm->factory()
                    ->makeRadialGradient(cx, cy, radius, colors, stops, count);
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::shader,
      new HostShader{std::move(shader)});
}

void shaderReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
  releaseHostHandle(vm, handle, WasmScriptingVM::HandleTable::Tag::shader);
}

void paintShaderImpl(
    WasmScriptingVM* vm,
    uint32_t paintHandle,
    uint32_t shaderHandle)
{
  auto paint = resolvePaint(vm, paintHandle);
  if (vm == nullptr || paint == nullptr)
  {
    return;
  }
  auto hostShader = static_cast<HostShader*>(vm->handles().resolve(
      shaderHandle,
      WasmScriptingVM::HandleTable::Tag::shader));
  paint->shader(hostShader != nullptr ? hostShader->shader : nullptr);
}

void paintShaderTransformImpl(
    WasmScriptingVM* vm,
    uint32_t paintHandle,
    const float* values,
    uint32_t floatCount)
{
  if (values == nullptr || floatCount < 6)
  {
    return;
  }
  if (auto paint = resolvePaint(vm, paintHandle))
  {
    paint->shaderTransform(Mat2D(
        values[0],
        values[1],
        values[2],
        values[3],
        values[4],
        values[5]));
  }
}

// The object's file asset of type T by name; accept skips matches that
// carry nothing, the way the Luau lookups keep scanning past them.
FileAsset* findFileAsset(
    WasmScriptingVM* vm,
    uint32_t objectHandle,
    const char* name,
    uint32_t length,
    uint16_t typeKey,
    bool (*accept)(FileAsset*))
{
  if (vm == nullptr)
  {
    return nullptr;
  }
  auto object = static_cast<ScriptedObject*>(vm->handles().resolve(
      objectHandle,
      WasmScriptingVM::HandleTable::Tag::object));
  if (object == nullptr || object->scriptAsset() == nullptr ||
      object->scriptAsset()->file() == nullptr)
  {
    return nullptr;
  }
  std::string key(name, length);
  for (const auto& asset : object->scriptAsset()->file()->assets())
  {
    if (!asset->isTypeOf(typeKey) || asset->name() != key)
    {
      continue;
    }
    if (accept == nullptr || accept(asset.get()))
    {
      return asset.get();
    }
  }
  return nullptr;
}

template <typename T>
T* findFileAsset(
    WasmScriptingVM* vm,
    uint32_t objectHandle,
    const char* name,
    uint32_t length,
    bool (*accept)(FileAsset*) = nullptr)
{
  return static_cast<T*>(
      findFileAsset(vm, objectHandle, name, length, T::typeKey, accept));
}

struct HostImage
{
  rcp<RenderImage> image;
#ifdef RIVE_CANVAS
  // A canvas image outlives its canvas only through this.
  rcp<gpu::RenderCanvas> sourceCanvas;
#endif
};

HostImage* resolveImage(WasmScriptingVM* vm, uint32_t handle)
{
  if (vm == nullptr)
  {
    return nullptr;
  }
  return static_cast<HostImage*>(
      vm->handles().resolve(handle, WasmScriptingVM::HandleTable::Tag::image));
}

uint32_t imageFromAssetImpl(
    WasmScriptingVM* vm,
    uint32_t objectHandle,
    const char* name,
    uint32_t length)
{
  auto asset = findFileAsset<ImageAsset>(vm, objectHandle, name, length);
  if (asset == nullptr || asset->renderImage() == nullptr)
  {
    return 0;
  }
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::image,
      new HostImage{ref_rcp(asset->renderImage())});
}

uint32_t imageWidthImpl(WasmScriptingVM* vm, uint32_t handle)
{
  auto host = resolveImage(vm, handle);
  return host != nullptr ? (uint32_t)host->image->width() : 0;
}

uint32_t imageHeightImpl(WasmScriptingVM* vm, uint32_t handle)
{
  auto host = resolveImage(vm, handle);
  return host != nullptr ? (uint32_t)host->image->height() : 0;
}

void imageReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
  releaseHostHandle(vm, handle, WasmScriptingVM::HandleTable::Tag::image);
}

uint32_t imageDecodeImpl(
    WasmScriptingVM* vm,
    const uint8_t* bytes,
    uint32_t byteCount,
    uint32_t token)
{
  if (vm == nullptr || bytes == nullptr || byteCount == 0)
  {
    return 0;
  }
  return vm->startImageDecode(bytes, byteCount, token) ? 1 : 0;
}

void imageDecodeCancelImpl(WasmScriptingVM* vm, uint32_t token)
{
  if (vm != nullptr)
  {
    vm->cancelImageDecode(token);
  }
}

#if defined(RIVE_CANVAS) && defined(RIVE_ORE)
struct HostGpuCanvas
{
  rcp<gpu::RenderCanvas> canvas;
  rcp<ore::TextureView> colorView;
};

struct HostGpuPass
{
  std::unique_ptr<ore::RenderPass> pass;
  // setBindGroup calls made before the first setPipeline.
  ore::DeferredBindGroups deferredBindGroups;
  ore::DrawBindings drawBindings;
  bool pipelineSet = false;
};

struct HostGpuBuffer
{
  rcp<ore::Buffer> buffer;
  // Ore allows no update to an immutable buffer.
  bool immutable;
};

struct HostGpuTexture
{
  rcp<ore::Texture> texture;
};

struct HostGpuSampler
{
  rcp<ore::Sampler> sampler;
};

struct HostGpuTextureView
{
  rcp<ore::TextureView> view;
};

struct HostGpuShaderModule
{
  rcp<ore::ShaderModule> shaderModule;
};

struct HostGpuBindGroupLayout
{
  rcp<ore::BindGroupLayout> layout;
};

struct HostGpuBindGroup
{
  rcp<ore::BindGroup> bindGroup;
};

struct HostGpuPipeline
{
  rcp<ore::Pipeline> pipeline;
};

ore::Context* gpuOreContext(WasmScriptingVM* vm)
{
  if (vm == nullptr || vm->factory() == nullptr)
  {
    return nullptr;
  }
  if (auto* recording = vm->factory()->ore())
  {
    return recording;
  }
  Factory* renderContext = vm->factory()->renderContext();
  return renderContext != nullptr ? renderContext->ore() : nullptr;
}

// The Lua bindings raise lastError as a script error; the module lane traps
// with the same text so the author reads the cause instead of a 0 handle.
uint32_t gpuRejected(
    WasmScriptingVM* vm,
    ore::Context* context,
    const char* what)
{
  std::string message = std::string(what) + ": " +
                        (context->lastError().empty() ? "creation failed"
                                                      : context->lastError());
  vm->raiseModuleError(message.c_str());
  return 0;
}

template <typename E>
bool validOre(WasmScriptingVM* vm, uint32_t value, E last, const char* type)
{
  return validOrdinal(vm, value, value <= (uint32_t)last, type);
}

bool guardPassed(WasmScriptingVM* vm, bool passed, const std::string& error)
{
  if (!passed)
  {
    vm->raiseModuleError(error.c_str());
  }
  return passed;
}

uint32_t gpuCanvasNewImpl(WasmScriptingVM* vm, uint32_t width, uint32_t height)
{
  if (vm == nullptr || vm->factory() == nullptr)
  {
    return 0;
  }
  // Like the Luau lane, a zero size gives a canvas without backing that
  // resize allocates once the layout size is known.
  if (width == 0 || height == 0)
  {
    return vm->handles().mint(
        WasmScriptingVM::HandleTable::Tag::gpuCanvas,
        new HostGpuCanvas{});
  }
  auto* renderContext = static_cast<gpu::RenderContext*>(
      vm->factory()->renderContext());
  ore::Context* oreContext = gpuOreContext(vm);
  if (renderContext == nullptr || oreContext == nullptr)
  {
    return 0;
  }
  auto* deferredHost = vm->factory()->deferredCanvasHost();
  auto canvas = deferredHost != nullptr
                    ? deferredHost->makeContentCanvas(width, height)
                    : renderContext->makeRenderCanvas(width, height);
  if (canvas == nullptr)
  {
    return 0;
  }
  auto colorView = oreContext->wrapCanvasTexture(canvas.get());
  if (colorView == nullptr)
  {
    return 0;
  }
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::gpuCanvas,
      new HostGpuCanvas{std::move(canvas), std::move(colorView)});
}

void gpuCanvasReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
  releaseHostHandle(vm, handle, WasmScriptingVM::HandleTable::Tag::gpuCanvas);
}

uint32_t gpuCanvasColorViewImpl(
    WasmScriptingVM* vm,
    uint32_t canvasHandle,
    uint32_t* props,
    uint32_t propCount)
{
  ore::Context* oreContext = gpuOreContext(vm);
  if (vm == nullptr || oreContext == nullptr || propCount < 4)
  {
    return 0;
  }
  auto host = static_cast<HostGpuCanvas*>(vm->handles().resolve(
      canvasHandle,
      WasmScriptingVM::HandleTable::Tag::gpuCanvas));
  if (host == nullptr)
  {
    return 0;
  }
  props[2] = (uint32_t)oreContext->canvasTargetFormat();
  props[3] = 1;
  if (host->canvas == nullptr)
  {
    props[0] = 0;
    props[1] = 0;
    return 0;
  }
  props[0] = host->canvas->width();
  props[1] = host->canvas->height();
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::gpuTextureView,
      new HostGpuTextureView{host->colorView});
}

uint32_t gpuCanvasImageImpl(WasmScriptingVM* vm, uint32_t canvasHandle)
{
  if (vm == nullptr)
  {
    return 0;
  }
  auto host = static_cast<HostGpuCanvas*>(vm->handles().resolve(
      canvasHandle,
      WasmScriptingVM::HandleTable::Tag::gpuCanvas));
  if (host == nullptr || host->canvas == nullptr ||
      host->canvas->renderImage() == nullptr)
  {
    return 0;
  }
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::image,
      new HostImage{
          ref_rcp(static_cast<RenderImage*>(host->canvas->renderImage())),
          host->canvas});
}

uint32_t gpuCanvasResizeImpl(
    WasmScriptingVM* vm,
    uint32_t canvasHandle,
    uint32_t width,
    uint32_t height,
    uint32_t* props,
    uint32_t propCount)
{
  ore::Context* oreContext = gpuOreContext(vm);
  if (vm == nullptr || vm->factory() == nullptr || oreContext == nullptr ||
      width == 0 || height == 0 || propCount < 4)
  {
    return 0;
  }
  auto host = static_cast<HostGpuCanvas*>(vm->handles().resolve(
      canvasHandle,
      WasmScriptingVM::HandleTable::Tag::gpuCanvas));
  if (host == nullptr)
  {
    return 0;
  }
  auto* renderContext = static_cast<gpu::RenderContext*>(
      vm->factory()->renderContext());
  if (renderContext == nullptr)
  {
    return 0;
  }
  auto* deferredHost = vm->factory()->deferredCanvasHost();
  auto canvas = deferredHost != nullptr
                    ? deferredHost->makeContentCanvas(width, height)
                    : renderContext->makeRenderCanvas(width, height);
  if (canvas == nullptr)
  {
    return 0;
  }
  auto colorView = oreContext->wrapCanvasTexture(canvas.get());
  if (colorView == nullptr)
  {
    return 0;
  }
  host->canvas = std::move(canvas);
  host->colorView = std::move(colorView);
  props[0] = host->canvas->width();
  props[1] = host->canvas->height();
  props[2] = (uint32_t)oreContext->canvasTargetFormat();
  props[3] = 1;
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::gpuTextureView,
      new HostGpuTextureView{host->colorView});
}

uint32_t gpuTargetViewImpl(
    WasmScriptingVM* vm,
    uint32_t current,
    uint32_t* props,
    uint32_t propCount)
{
  ore::Context* oreContext = gpuOreContext(vm);
  // Props stay unwritten where no host could ever expose a target.
  if (vm == nullptr || oreContext == nullptr || !oreContext->isRecording() ||
      !oreContext->exposesTarget() || propCount < 4)
  {
    return 0;
  }
  rcp<ore::TextureView> view = oreContext->targetView();
  if (view == nullptr)
  {
    props[0] = 0;
    props[1] = 0;
    props[2] = (uint32_t)ore::TextureFormat::rgba8unorm;
    props[3] = 1;
    return 0;
  }
  props[0] = view->width();
  props[1] = view->height();
  props[2] = (uint32_t)view->texture()->format();
  props[3] = view->texture()->sampleCount();
  auto host = static_cast<HostGpuTextureView*>(vm->handles().resolve(
      current,
      WasmScriptingVM::HandleTable::Tag::gpuTextureView));
  if (host != nullptr && host->view == view)
  {
    return current;
  }
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::gpuTextureView,
      new HostGpuTextureView{std::move(view)});
}

uint32_t gpuFeaturesImpl(WasmScriptingVM* vm, uint32_t* out, uint32_t outCount)
{
  ore::Context* oreContext = gpuOreContext(vm);
  if (vm == nullptr || oreContext == nullptr || outCount < 19)
  {
    return 0;
  }
  // With ReplayCaps a recording context answers from shipped data; an
  // unknown snapshot must raise module-side instead of guessing.
  if (!oreContext->featuresKnown())
  {
    return ~0u;
  }
  const ore::Features& f = oreContext->features();
  out[0] = f.bc ? 1 : 0;
  out[1] = f.etc2 ? 1 : 0;
  out[2] = f.astc ? 1 : 0;
  out[3] = (uint32_t)f.maxTextureSize2D;
  out[4] = (uint32_t)f.maxTextureSizeCube;
  out[5] = (uint32_t)f.maxTextureSize3D;
  out[6] = f.anisotropicFiltering ? 1 : 0;
  out[7] = f.texture3D ? 1 : 0;
  out[8] = f.textureArrays ? 1 : 0;
  out[9] = f.colorBufferFloat ? 1 : 0;
  out[10] = f.colorBufferHalfFloat ? 1 : 0;
  out[11] = f.perTargetBlend ? 1 : 0;
  out[12] = f.perTargetWriteMask ? 1 : 0;
  out[13] = f.drawBaseInstance ? 1 : 0;
  out[14] = f.depthBiasClamp ? 1 : 0;
  out[15] = (uint32_t)f.maxColorAttachments;
  out[16] = (uint32_t)f.maxUniformBufferSize;
  out[17] = (uint32_t)f.maxSamplers;
  out[18] = (uint32_t)f.maxSamples;
  // Guests built before the alignment field ask for 19.
  if (outCount < 20)
  {
    return 19;
  }
  out[19] = f.minUniformBufferOffsetAlignment;
  return 20;
}

uint32_t gpuPassBeginImpl(
    WasmScriptingVM* vm,
    const rive_gpu_pass_desc_v1* podDesc,
    uint32_t descByteCount,
    const uint8_t* blob,
    uint32_t blobCount)
{
  ore::Context* oreContext = gpuOreContext(vm);
  // The label is optional, so a descriptor from before it still passes.
  if (vm == nullptr || oreContext == nullptr ||
      descByteCount < offsetof(rive_gpu_pass_desc_v1, labelSize) ||
      podDesc->colorCount > 4)
  {
    return 0;
  }
  const uint32_t labelSize = descByteCount >= sizeof(*podDesc)
                                 ? podDesc->labelSize
                                 : 0;
  const uint64_t colorBytes = uint64_t(podDesc->colorCount) *
                              sizeof(rive_gpu_pass_color_attachment_v1);
  if (colorBytes + labelSize != blobCount)
  {
    return 0;
  }
  auto colors = reinterpret_cast<const rive_gpu_pass_color_attachment_v1*>(
      blob);
  std::string label;
  if (labelSize != 0)
  {
    label.assign(reinterpret_cast<const char*>(blob + colorBytes), labelSize);
  }
  auto resolveView = [vm](uint32_t handle) -> ore::TextureView* {
    auto host = static_cast<HostGpuTextureView*>(vm->handles().resolve(
        handle,
        WasmScriptingVM::HandleTable::Tag::gpuTextureView));
    return host != nullptr ? host->view.get() : nullptr;
  };
  ore::RenderPassDesc desc;
  desc.colorCount = podDesc->colorCount;
  for (uint32_t i = 0; i < podDesc->colorCount; i++)
  {
    auto& out = desc.colorAttachments[i];
    out.view = resolveView(colors[i].view);
    if (out.view == nullptr)
    {
      return 0;
    }
    out.resolveTarget = colors[i].resolveTarget != 0
                            ? resolveView(colors[i].resolveTarget)
                            : nullptr;
    if (!validOre(vm, colors[i].loadOp, ore::LoadOp::dontCare, "LoadOp") ||
        !validOre(vm, colors[i].storeOp, ore::StoreOp::discard, "StoreOp"))
    {
      return 0;
    }
    out.loadOp = (ore::LoadOp)colors[i].loadOp;
    out.storeOp = (ore::StoreOp)colors[i].storeOp;
    out.clearColor = {
        colors[i].clearR,
        colors[i].clearG,
        colors[i].clearB,
        colors[i].clearA};
  }
  if (podDesc->depthView != 0)
  {
    desc.depthStencil.view = resolveView(podDesc->depthView);
    if (desc.depthStencil.view == nullptr)
    {
      return 0;
    }
  }
  if (!validOre(vm, podDesc->depthLoadOp, ore::LoadOp::dontCare, "LoadOp") ||
      !validOre(vm, podDesc->depthStoreOp, ore::StoreOp::discard, "StoreOp") ||
      !validOre(vm, podDesc->stencilLoadOp, ore::LoadOp::dontCare, "LoadOp") ||
      !validOre(vm, podDesc->stencilStoreOp, ore::StoreOp::discard, "StoreOp"))
  {
    return 0;
  }
  desc.depthStencil.depthLoadOp = (ore::LoadOp)podDesc->depthLoadOp;
  desc.depthStencil.depthStoreOp = (ore::StoreOp)podDesc->depthStoreOp;
  desc.depthStencil.depthClearValue = podDesc->depthClearValue;
  desc.depthStencil.stencilLoadOp = (ore::LoadOp)podDesc->stencilLoadOp;
  desc.depthStencil.stencilStoreOp = (ore::StoreOp)podDesc->stencilStoreOp;
  desc.depthStencil.stencilClearValue = podDesc->stencilClearValue;
  desc.label = label.empty() ? nullptr : label.c_str();
  oreContext->clearLastError();
  auto pass = ore::cmd::beginRecordedRenderPass(*oreContext, desc);
  if (pass == nullptr)
  {
    // A pass refused without an error drops, like one with no target.
    return oreContext->lastError().empty()
               ? 0
               : gpuRejected(vm, oreContext, "beginRenderPass");
  }
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::gpuPass,
      new HostGpuPass{std::move(pass)});
}

uint32_t gpuImageViewImpl(
    WasmScriptingVM* vm,
    uint32_t imageHandle,
    uint32_t width,
    uint32_t height)
{
  ore::Context* oreContext = gpuOreContext(vm);
  if (vm == nullptr || oreContext == nullptr)
  {
    return 0;
  }
  auto host = static_cast<HostImage*>(vm->handles().resolve(
      imageHandle,
      WasmScriptingVM::HandleTable::Tag::image));
  if (host == nullptr)
  {
    return 0;
  }
  // Same dispatch as Image:view() in the Luau binding.
  rcp<ore::TextureView> view;
  if (!oreContext->isRecording())
  {
    if (host->sourceCanvas != nullptr)
    {
      view = oreContext->wrapCanvasSampleView(host->sourceCanvas.get());
    }
    else if (auto* riveImage = lite_rtti_cast<RiveRenderImage*>(
                 host->image.get());
             riveImage != nullptr && riveImage->getTexture() != nullptr)
    {
      view = oreContext->wrapRiveTexture(
          riveImage->getTexture(),
          host->image->width(),
          host->image->height());
    }
  }
  else if (host->sourceCanvas != nullptr)
  {
    view = oreContext->recordWrapCanvasImage(
        host->sourceCanvas.get(),
        host->image->width(),
        host->image->height());
  }
  else if (
      auto* deferredImage = lite_rtti_cast<cmd::DeferredRenderImage*>(
          host->image.get()))
  {
    view = oreContext->recordWrapImageView(
        deferredImage->id(),
        host->image->width(),
        host->image->height());
  }
  else
  {
    // A foreign image the recorder was handed rather than created.
    view = oreContext->recordWrapForeignImageView(
        host->image.get(),
        host->image->width(),
        host->image->height());
  }
  if (view == nullptr)
  {
    return 0;
  }
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::gpuTextureView,
      new HostGpuTextureView{std::move(view)});
}

HostGpuPass* resolveHostPass(WasmScriptingVM* vm, uint32_t handle)
{
  if (vm == nullptr)
  {
    return nullptr;
  }
  return static_cast<HostGpuPass*>(vm->handles().resolve(
      handle,
      WasmScriptingVM::HandleTable::Tag::gpuPass));
}

ore::RenderPass* resolvePass(WasmScriptingVM* vm, uint32_t handle)
{
  auto host = resolveHostPass(vm, handle);
  return host != nullptr ? host->pass.get() : nullptr;
}

// The draw guards the Luau binding raises, with the same text so both lanes
// read the same cause. Returns false after raising.
bool gpuPassDrawAllowed(
    WasmScriptingVM* vm,
    HostGpuPass* host,
    const char* what,
    int32_t baseVertex,
    uint32_t firstInstance)
{
  if (!host->pipelineSet)
  {
    vm->raiseModuleError(ore::kGuardSetPipelineBeforeDraw);
    return false;
  }
  if (std::string err; !host->drawBindings.validate(&err))
  {
    vm->raiseModuleError(err.c_str());
    return false;
  }
  ore::Context* oreContext = gpuOreContext(vm);
  bool featuresKnown = oreContext != nullptr && oreContext->featuresKnown();
  if (featuresKnown && !oreContext->features().drawBaseInstance)
  {
    char message[160];
    if (baseVertex != 0)
    {
      snprintf(
          message,
          sizeof(message),
          ore::kGuardBaseVertexFormat,
          what,
          baseVertex);
      vm->raiseModuleError(message);
      return false;
    }
    if (firstInstance > 0)
    {
      snprintf(
          message,
          sizeof(message),
          ore::kGuardFirstInstanceFormat,
          what,
          firstInstance);
      vm->raiseModuleError(message);
      return false;
    }
  }
  return true;
}

void gpuPassSetPipelineImpl(
    WasmScriptingVM* vm,
    uint32_t passHandle,
    uint32_t pipelineHandle)
{
  // The pass resolves first: it is null with no VM, as during module start.
  auto* host = resolveHostPass(vm, passHandle);
  if (host == nullptr)
  {
    return;
  }
  auto pipeline = static_cast<HostGpuPipeline*>(vm->handles().resolve(
      pipelineHandle,
      WasmScriptingVM::HandleTable::Tag::gpuPipeline));
  if (pipeline == nullptr)
  {
    return;
  }
  // An attachment compat failure no-ops the backend and later draws crash
  // in the driver, so it surfaces here the way the Luau binding raises it.
  ore::Context* oreContext = gpuOreContext(vm);
  if (oreContext != nullptr)
  {
    oreContext->clearLastError();
  }
  host->pass->setPipeline(pipeline->pipeline.get());
  host->deferredBindGroups.flush(host->pass.get());
  if (oreContext != nullptr && !oreContext->lastError().empty())
  {
    gpuRejected(vm, oreContext, "setPipeline");
    return;
  }
  host->drawBindings.setPipeline(pipeline->pipeline.get());
  host->pipelineSet = true;
}

void gpuPassSetVertexBufferImpl(
    WasmScriptingVM* vm,
    uint32_t passHandle,
    uint32_t slot,
    uint32_t bufferHandle,
    uint32_t offset)
{
  auto* pass = resolvePass(vm, passHandle);
  if (pass == nullptr)
  {
    return;
  }
  auto buffer = static_cast<HostGpuBuffer*>(vm->handles().resolve(
      bufferHandle,
      WasmScriptingVM::HandleTable::Tag::gpuBuffer));
  if (buffer == nullptr)
  {
    return;
  }
  if (slot >= ore::kMaxVertexBufferSlots)
  {
    char message[96];
    snprintf(
        message,
        sizeof(message),
        ore::kGuardVertexSlotRangeFormat,
        ore::kMaxVertexBufferSlots - 1,
        slot);
    vm->raiseModuleError(message);
    return;
  }
  pass->setVertexBuffer(slot, buffer->buffer.get(), offset);
}

void gpuPassSetIndexBufferImpl(
    WasmScriptingVM* vm,
    uint32_t passHandle,
    uint32_t bufferHandle,
    uint32_t indexFormat,
    uint32_t offset)
{
  auto* pass = resolvePass(vm, passHandle);
  if (pass == nullptr)
  {
    return;
  }
  auto buffer = static_cast<HostGpuBuffer*>(vm->handles().resolve(
      bufferHandle,
      WasmScriptingVM::HandleTable::Tag::gpuBuffer));
  if (buffer == nullptr ||
      !validOrdinal(
          vm,
          indexFormat,
          indexFormat == (uint32_t)ore::IndexFormat::uint16 ||
              indexFormat == (uint32_t)ore::IndexFormat::uint32,
          "IndexFormat"))
  {
    return;
  }
  pass->setIndexBuffer(
      buffer->buffer.get(),
      (ore::IndexFormat)indexFormat,
      offset);
}

void gpuPassSetBindGroupImpl(
    WasmScriptingVM* vm,
    uint32_t passHandle,
    uint32_t groupIndex,
    uint32_t bindGroupHandle,
    const uint32_t* dynamicOffsets,
    uint32_t dynamicOffsetByteCount)
{
  auto* host = resolveHostPass(vm, passHandle);
  if (host == nullptr || host->pass == nullptr)
  {
    return;
  }
  auto bindGroup = static_cast<HostGpuBindGroup*>(vm->handles().resolve(
      bindGroupHandle,
      WasmScriptingVM::HandleTable::Tag::gpuBindGroup));
  if (bindGroup == nullptr || dynamicOffsetByteCount % sizeof(uint32_t) != 0)
  {
    return;
  }
  uint32_t count = dynamicOffsetByteCount / (uint32_t)sizeof(uint32_t);
  // The checks the Luau binding makes, so a misuse raises on the call
  // instead of reaching a backend assert or a silent misbind.
  ore::Context* oreContext = gpuOreContext(vm);
  if (std::string err; !ore::validateSetBindGroup(
          groupIndex,
          bindGroup->bindGroup.get(),
          dynamicOffsets,
          count,
          ore::scriptDynamicOffsetAlignment(oreContext),
          &err))
  {
    vm->raiseModuleError(err.c_str());
    return;
  }
  if (!host->pipelineSet)
  {
    host->deferredBindGroups
        .defer(groupIndex, bindGroup->bindGroup.get(), dynamicOffsets, count);
    host->drawBindings.setBindGroup(groupIndex, bindGroup->bindGroup.get());
    return;
  }
  if (oreContext != nullptr)
  {
    oreContext->clearLastError();
  }
  host->pass->setBindGroup(
      groupIndex,
      bindGroup->bindGroup.get(),
      count != 0 ? dynamicOffsets : nullptr,
      count);
  if (oreContext != nullptr && !oreContext->lastError().empty())
  {
    gpuRejected(vm, oreContext, "setBindGroup");
    return;
  }
  host->drawBindings.setBindGroup(groupIndex, bindGroup->bindGroup.get());
}

void gpuPassSetViewportImpl(
    WasmScriptingVM* vm,
    uint32_t passHandle,
    float x,
    float y,
    float width,
    float height,
    float minDepth,
    float maxDepth)
{
  if (auto* pass = resolvePass(vm, passHandle))
  {
    pass->setViewport(x, y, width, height, minDepth, maxDepth);
  }
}

void gpuPassSetScissorImpl(
    WasmScriptingVM* vm,
    uint32_t passHandle,
    uint32_t x,
    uint32_t y,
    uint32_t width,
    uint32_t height)
{
  if (auto* pass = resolvePass(vm, passHandle))
  {
    pass->setScissorRect(x, y, width, height);
  }
}

void gpuPassSetStencilReferenceImpl(
    WasmScriptingVM* vm,
    uint32_t passHandle,
    uint32_t ref)
{
  if (auto* pass = resolvePass(vm, passHandle))
  {
    pass->setStencilReference(ref);
  }
}

void gpuPassSetBlendColorImpl(
    WasmScriptingVM* vm,
    uint32_t passHandle,
    float r,
    float g,
    float b,
    float a)
{
  if (auto* pass = resolvePass(vm, passHandle))
  {
    pass->setBlendColor(r, g, b, a);
  }
}

void gpuPassDrawImpl(
    WasmScriptingVM* vm,
    uint32_t passHandle,
    uint32_t vertexCount,
    uint32_t instanceCount,
    uint32_t firstVertex,
    uint32_t firstInstance)
{
  auto* host = resolveHostPass(vm, passHandle);
  if (host == nullptr ||
      !gpuPassDrawAllowed(vm, host, "draw", 0, firstInstance))
  {
    return;
  }
  host->pass->draw(vertexCount, instanceCount, firstVertex, firstInstance);
}

void gpuPassDrawIndexedImpl(
    WasmScriptingVM* vm,
    uint32_t passHandle,
    uint32_t indexCount,
    uint32_t instanceCount,
    uint32_t firstIndex,
    int32_t baseVertex,
    uint32_t firstInstance)
{
  auto* host = resolveHostPass(vm, passHandle);
  if (host == nullptr ||
      !gpuPassDrawAllowed(vm, host, "drawIndexed", baseVertex, firstInstance))
  {
    return;
  }
  host->pass->drawIndexed(
      indexCount,
      instanceCount,
      firstIndex,
      baseVertex,
      firstInstance);
}

void gpuPassFinishImpl(WasmScriptingVM* vm, uint32_t handle)
{
  if (vm == nullptr)
  {
    return;
  }
  auto host = static_cast<HostGpuPass*>(vm->handles().resolve(
      handle,
      WasmScriptingVM::HandleTable::Tag::gpuPass));
  if (host == nullptr || host->pass == nullptr)
  {
    return;
  }
  if (!host->pass->isFinished())
  {
    host->pass->finish();
  }
  host->drawBindings = {};
}

void gpuPassReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
  releaseHostHandle(vm, handle, WasmScriptingVM::HandleTable::Tag::gpuPass);
}

uint32_t gpuBufferNewImpl(
    WasmScriptingVM* vm,
    uint32_t usage,
    uint32_t sizeInBytes,
    uint32_t immutable,
    const uint8_t* data,
    uint32_t dataCount)
{
  ore::Context* oreContext = gpuOreContext(vm);
  if (oreContext == nullptr || sizeInBytes == 0 ||
      (dataCount != 0 && dataCount != sizeInBytes) ||
      !validOre(vm, usage, ore::BufferUsage::uniform, "BufferUsage"))
  {
    return 0;
  }
  ore::BufferDesc desc;
  desc.usage = (ore::BufferUsage)usage;
  desc.size = sizeInBytes;
  desc.immutable = immutable != 0;
  desc.data = dataCount != 0 ? data : nullptr;
  oreContext->clearLastError();
  auto buffer = oreContext->makeBuffer(desc);
  if (buffer == nullptr)
  {
    return gpuRejected(vm, oreContext, "GPUBuffer");
  }
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::gpuBuffer,
      new HostGpuBuffer{std::move(buffer), desc.immutable});
}

void gpuBufferUpdateImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint32_t dstOffset,
    const uint8_t* data,
    uint32_t dataCount)
{
  if (vm == nullptr)
  {
    return;
  }
  auto host = static_cast<HostGpuBuffer*>(vm->handles().resolve(
      handle,
      WasmScriptingVM::HandleTable::Tag::gpuBuffer));
  if (host == nullptr || host->immutable || dataCount == 0 ||
      uint64_t(dstOffset) + dataCount > host->buffer->size())
  {
    return;
  }
  host->buffer->update(data, dataCount, dstOffset);
}

void gpuBufferUpdateBlobImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint32_t dstOffset,
    uint32_t blob,
    uint32_t srcOffset,
    uint32_t byteCount)
{
  if (vm == nullptr)
  {
    return;
  }
  // The lookup holds the blob through the update, which copies the bytes
  // into the buffer or the recording before it returns.
  auto found = SharedRegistry::find<SharedBlob>(blob, vm->sharedOwner());
  if (found == nullptr || uint64_t(srcOffset) + byteCount > found->size())
  {
    return;
  }
  gpuBufferUpdateImpl(
      vm,
      handle,
      dstOffset,
      found->bytes() + srcOffset,
      byteCount);
}

uint32_t gpuBufferNewBlobImpl(
    WasmScriptingVM* vm,
    uint32_t usage,
    uint32_t immutable,
    uint32_t blob,
    uint32_t srcOffset,
    uint32_t byteCount)
{
  if (vm == nullptr)
  {
    return 0;
  }
  // The lookup holds the blob while the create copies its bytes.
  auto found = SharedRegistry::find<SharedBlob>(blob, vm->sharedOwner());
  if (found == nullptr || uint64_t(srcOffset) + byteCount > found->size())
  {
    return 0;
  }
  return gpuBufferNewImpl(
      vm,
      usage,
      byteCount,
      immutable,
      found->bytes() + srcOffset,
      byteCount);
}

void gpuBufferReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
  releaseHostHandle(vm, handle, WasmScriptingVM::HandleTable::Tag::gpuBuffer);
}

uint32_t gpuTextureNewImpl(
    WasmScriptingVM* vm,
    const rive_gpu_texture_desc_v1* podDesc,
    uint32_t descByteCount)
{
  ore::Context* oreContext = gpuOreContext(vm);
  std::string error;
  if (oreContext == nullptr || descByteCount < sizeof(*podDesc) ||
      !validOre(
          vm,
          podDesc->format,
          ore::TextureFormat::astc8x8,
          "TextureFormat") ||
      !validOre(
          vm,
          podDesc->textureType,
          ore::TextureType::array2D,
          "TextureType") ||
      !guardPassed(
          vm,
          ore::validateSampleCount(oreContext, podDesc->sampleCount, &error),
          error) ||
      !guardPassed(
          vm,
          ore::validateTextureFormat(
              oreContext,
              (ore::TextureFormat)podDesc->format,
              &error),
          error))
  {
    return 0;
  }
  ore::TextureDesc desc;
  desc.width = podDesc->width;
  desc.height = podDesc->height;
  desc.depthOrArrayLayers = podDesc->depthOrArrayLayers;
  desc.format = (ore::TextureFormat)podDesc->format;
  desc.type = (ore::TextureType)podDesc->textureType;
  desc.renderTarget = podDesc->renderTarget != 0;
  desc.numMipmaps = podDesc->numMipmaps;
  desc.sampleCount = podDesc->sampleCount;
  oreContext->clearLastError();
  auto texture = oreContext->makeTexture(desc);
  if (texture == nullptr)
  {
    return gpuRejected(vm, oreContext, "GPUTexture");
  }
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::gpuTexture,
      new HostGpuTexture{std::move(texture)});
}

void gpuTextureUploadImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    const rive_gpu_texture_upload_v1* region,
    uint32_t regionByteCount,
    const uint8_t* data,
    uint32_t dataCount)
{
  if (vm == nullptr || regionByteCount < sizeof(*region) || dataCount == 0)
  {
    return;
  }
  auto host = static_cast<HostGpuTexture*>(vm->handles().resolve(
      handle,
      WasmScriptingVM::HandleTable::Tag::gpuTexture));
  if (host == nullptr)
  {
    return;
  }
  ore::TextureDataDesc upload;
  upload.data = data;
  upload.dataSize = dataCount;
  upload.bytesPerRow = region->bytesPerRow;
  upload.rowsPerImage = region->rowsPerImage;
  upload.mipLevel = region->mipLevel;
  upload.layer = region->layer;
  upload.x = region->x;
  upload.y = region->y;
  upload.z = region->z;
  upload.width = region->width;
  upload.height = region->height;
  upload.depth = region->depth;
  std::string error;
  if (!host->texture->upload(upload, &error))
  {
    vm->raiseModuleError(error.c_str());
  }
}

void gpuTextureReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
  releaseHostHandle(vm, handle, WasmScriptingVM::HandleTable::Tag::gpuTexture);
}

uint32_t gpuSamplerNewImpl(
    WasmScriptingVM* vm,
    const rive_gpu_sampler_desc_v1* podDesc,
    uint32_t descByteCount)
{
  ore::Context* oreContext = gpuOreContext(vm);
  if (oreContext == nullptr || descByteCount < sizeof(*podDesc) ||
      !validOre(vm, podDesc->minFilter, ore::Filter::linear, "Filter") ||
      !validOre(vm, podDesc->magFilter, ore::Filter::linear, "Filter") ||
      !validOre(vm, podDesc->mipmapFilter, ore::Filter::linear, "Filter") ||
      !validOre(vm, podDesc->wrapU, ore::WrapMode::clampToEdge, "WrapMode") ||
      !validOre(vm, podDesc->wrapV, ore::WrapMode::clampToEdge, "WrapMode") ||
      !validOre(vm, podDesc->wrapW, ore::WrapMode::clampToEdge, "WrapMode") ||
      (podDesc->compare != 0xFFFFFFFFu && !validOre(
                                              vm,
                                              podDesc->compare,
                                              ore::CompareFunction::always,
                                              "CompareFunction")))
  {
    return 0;
  }
  ore::SamplerDesc desc;
  desc.minFilter = (ore::Filter)podDesc->minFilter;
  desc.magFilter = (ore::Filter)podDesc->magFilter;
  desc.mipmapFilter = (ore::Filter)podDesc->mipmapFilter;
  desc.wrapU = (ore::WrapMode)podDesc->wrapU;
  desc.wrapV = (ore::WrapMode)podDesc->wrapV;
  desc.wrapW = (ore::WrapMode)podDesc->wrapW;
  // The guest sends ~0 for "no comparison sampler".
  desc.compare = podDesc->compare == 0xFFFFFFFFu
                     ? ore::CompareFunction::none
                     : (ore::CompareFunction)podDesc->compare;
  desc.minLod = podDesc->minLod;
  desc.maxLod = podDesc->maxLod;
  desc.maxAnisotropy = podDesc->maxAnisotropy;
  std::string error;
  if (!guardPassed(
          vm,
          ore::validateSamplerDesc(oreContext, desc, &error),
          error))
  {
    return 0;
  }
  oreContext->clearLastError();
  auto sampler = oreContext->makeSampler(desc);
  if (sampler == nullptr)
  {
    return gpuRejected(vm, oreContext, "GPUSampler");
  }
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::gpuSampler,
      new HostGpuSampler{std::move(sampler)});
}

void gpuSamplerReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
  releaseHostHandle(vm, handle, WasmScriptingVM::HandleTable::Tag::gpuSampler);
}

uint32_t gpuTextureViewNewImpl(
    WasmScriptingVM* vm,
    uint32_t textureHandle,
    const rive_gpu_texture_view_desc_v1* podDesc,
    uint32_t descByteCount)
{
  ore::Context* oreContext = gpuOreContext(vm);
  if (oreContext == nullptr || descByteCount < sizeof(*podDesc))
  {
    return 0;
  }
  auto texture = static_cast<HostGpuTexture*>(vm->handles().resolve(
      textureHandle,
      WasmScriptingVM::HandleTable::Tag::gpuTexture));
  if (texture == nullptr)
  {
    return 0;
  }
  if (!validOre(
          vm,
          podDesc->dimension,
          ore::TextureViewDimension::cubeArray,
          "TextureViewDimension") ||
      !validOre(
          vm,
          podDesc->aspect,
          ore::TextureAspect::stencilOnly,
          "TextureAspect"))
  {
    return 0;
  }
  ore::TextureViewDesc desc;
  desc.texture = texture->texture.get();
  desc.dimension = (ore::TextureViewDimension)podDesc->dimension;
  desc.aspect = (ore::TextureAspect)podDesc->aspect;
  desc.baseMipLevel = podDesc->baseMipLevel;
  desc.mipCount = podDesc->mipCount;
  desc.baseLayer = podDesc->baseLayer;
  desc.layerCount = podDesc->layerCount;
  oreContext->clearLastError();
  auto view = oreContext->makeTextureView(desc);
  if (view == nullptr)
  {
    return gpuRejected(vm, oreContext, "GPUTextureView");
  }
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::gpuTextureView,
      new HostGpuTextureView{std::move(view)});
}

void gpuTextureViewReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
  releaseHostHandle(
      vm,
      handle,
      WasmScriptingVM::HandleTable::Tag::gpuTextureView);
}

uint32_t gpuShaderTargetImpl(WasmScriptingVM* vm)
{
  ore::Context* oreContext = gpuOreContext(vm);
  return oreContext != nullptr ? (uint32_t)oreContext->shaderTarget()
                               : (uint32_t)ore::ShaderTarget::wgsl;
}

uint32_t gpuShaderAssetBytesImpl(
    WasmScriptingVM* vm,
    uint32_t objectHandle,
    const char* name,
    uint32_t nameLength,
    uint8_t* out,
    uint32_t outCount)
{
  if (vm == nullptr)
  {
    return 0;
  }
#ifdef WITH_RIVE_TOOLS
  // Editor path first, matching the Luau lane: RSTBs compiled during
  // requestWasmVM shadow whatever a file asset carries.
  {
    const std::vector<uint8_t>* rstb = vm->findShaderRstb(
        std::string(name, nameLength));
    if (rstb != nullptr)
    {
      if (rstb->size() <= outCount)
      {
        memcpy(out, rstb->data(), rstb->size());
      }
      return (uint32_t)rstb->size();
    }
  }
#endif
  auto asset = findFileAsset<ShaderAsset>(vm, objectHandle, name, nameLength);
  if (asset == nullptr)
  {
    return 0;
  }
  auto rstb = asset->rstb();
  if (rstb.size() <= outCount)
  {
    memcpy(out, rstb.data(), rstb.size());
  }
  return (uint32_t)rstb.size();
}

uint32_t gpuShaderAssetIdImpl(
    WasmScriptingVM* vm,
    uint32_t objectHandle,
    const char* name,
    uint32_t nameLength)
{
  auto asset = findFileAsset<ShaderAsset>(vm, objectHandle, name, nameLength);
  return asset != nullptr ? asset->assetId() : 0;
}

uint32_t mintShaderModule(
    WasmScriptingVM* vm,
    ore::Context* oreContext,
    const ore::ShaderModuleDesc& desc)
{
  oreContext->clearLastError();
  auto shaderModule = oreContext->makeShaderModule(desc);
  if (shaderModule == nullptr)
  {
    return gpuRejected(vm, oreContext, "Shader");
  }
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::gpuShaderModule,
      new HostGpuShaderModule{std::move(shaderModule)});
}

// The module serving entry, built as every lane builds an asset's modules.
static uint32_t mintAssetEntryModule(
    WasmScriptingVM* vm,
    ore::Context* oreContext,
    const ShaderAsset& asset,
    uint32_t entry)
{
  uint32_t handle = 0;
  ore::visitShaderAssetModules(
      oreContext->shaderTarget(),
      asset,
      [&](const ore::ShaderModuleDesc& desc,
          uint32_t first,
          uint32_t count,
          const std::vector<ore::RstbEntryView>&) {
        if (entry < first || entry - first >= count)
        {
          return true;
        }
        handle = mintShaderModule(vm, oreContext, desc);
        return false;
      });
  return handle;
}

#ifdef WITH_RIVE_TOOLS
// The same from a bare RSTB, which only edit time hosts take.
static uint32_t mintRstbEntryModule(
    WasmScriptingVM* vm,
    ore::Context* oreContext,
    const uint8_t* rstb,
    uint32_t rstbCount,
    uint32_t assetId,
    uint32_t entry)
{
  ShaderAsset asset;
  if (!ore::decodeBareRstb(asset, rstb, rstbCount))
  {
    return 0;
  }
  asset.assetId(assetId);
  return mintAssetEntryModule(vm, oreContext, asset, entry);
}
#endif

uint32_t gpuShaderModuleFromAssetImpl(
    WasmScriptingVM* vm,
    uint32_t objectHandle,
    const char* name,
    uint32_t nameLength,
    uint32_t entry)
{
  ore::Context* oreContext = gpuOreContext(vm);
  if (oreContext == nullptr)
  {
    return 0;
  }
  const ShaderAsset* fileAsset = findFileAsset<ShaderAsset>(
      vm,
      objectHandle,
      name,
      nameLength);
#ifdef WITH_RIVE_TOOLS
  // Editor path first, matching shader_asset_bytes.
  if (const std::vector<uint8_t>* rstb = vm->findShaderRstb(
          std::string(name, nameLength)))
  {
    return mintRstbEntryModule(
        vm,
        oreContext,
        rstb->data(),
        (uint32_t)rstb->size(),
        fileAsset != nullptr ? fileAsset->assetId() : 0,
        entry);
  }
#endif
  if (fileAsset == nullptr)
  {
    return 0;
  }
  return mintAssetEntryModule(vm, oreContext, *fileAsset, entry);
}

#ifdef WITH_RIVE_TOOLS
uint32_t gpuShaderModuleFromRstbImpl(
    WasmScriptingVM* vm,
    const uint8_t* rstb,
    uint32_t rstbCount,
    uint32_t assetId,
    uint32_t entry)
{
  ore::Context* oreContext = gpuOreContext(vm);
  if (oreContext == nullptr)
  {
    return 0;
  }
  return mintRstbEntryModule(vm, oreContext, rstb, rstbCount, assetId, entry);
}
#endif

void gpuShaderModuleReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
  releaseHostHandle(
      vm,
      handle,
      WasmScriptingVM::HandleTable::Tag::gpuShaderModule);
}

uint32_t gpuBindGroupLayoutNewImpl(
    WasmScriptingVM* vm,
    uint32_t groupIndex,
    const rive_gpu_bind_group_layout_entry_v1* entries,
    uint32_t entryByteCount)
{
  ore::Context* oreContext = gpuOreContext(vm);
  if (oreContext == nullptr || entryByteCount % sizeof(*entries) != 0)
  {
    return 0;
  }
  uint32_t count = entryByteCount / (uint32_t)sizeof(*entries);
  std::vector<ore::BindGroupLayoutEntry> resolved(count);
  for (uint32_t i = 0; i < count; i++)
  {
    auto& out = resolved[i];
    const auto& in = entries[i];
    if (!validOre(
            vm,
            in.kind,
            ore::BindingKind::comparisonSampler,
            "BindingKind") ||
        !validOrdinal(
            vm,
            in.visibility,
            in.visibility <= (ore::StageVisibility::kVertex |
                              ore::StageVisibility::kFragment |
                              ore::StageVisibility::kCompute),
            "StageVisibility") ||
        !validOre(
            vm,
            in.textureViewDim,
            ore::TextureViewDimension::cubeArray,
            "TextureViewDimension") ||
        !validOre(
            vm,
            in.textureSampleType,
            ore::BindGroupLayoutEntry::SampleType::uint,
            "SampleType"))
    {
      return 0;
    }
    out.binding = in.binding;
    out.kind = (ore::BindingKind)in.kind;
    out.visibility.mask = (uint8_t)in.visibility;
    out.hasDynamicOffset = in.hasDynamicOffset != 0;
    out.textureViewDim = (ore::TextureViewDimension)in.textureViewDim;
    out.textureSampleType = (ore::BindGroupLayoutEntry::SampleType)
                                in.textureSampleType;
    out.textureMultisampled = in.textureMultisampled != 0;
    out.minBindingSize = in.minBindingSize;
    out.nativeSlotVS = in.nativeSlotVS;
    out.nativeSlotFS = in.nativeSlotFS;
    out.nativeSlotCS = in.nativeSlotCS;
    out.samplerNonFiltering = in.samplerNonFiltering != 0;
  }
  ore::BindGroupLayoutDesc desc;
  desc.groupIndex = groupIndex;
  desc.entries = resolved.data();
  desc.entryCount = count;
  oreContext->clearLastError();
  auto layout = oreContext->makeBindGroupLayout(desc);
  if (layout == nullptr)
  {
    return gpuRejected(vm, oreContext, "GPUBindGroupLayout");
  }
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::gpuBindGroupLayout,
      new HostGpuBindGroupLayout{std::move(layout)});
}

void gpuBindGroupLayoutReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
  releaseHostHandle(
      vm,
      handle,
      WasmScriptingVM::HandleTable::Tag::gpuBindGroupLayout);
}

uint32_t gpuBindGroupLayoutFromShaderImpl(
    WasmScriptingVM* vm,
    uint32_t shaderModule,
    uint32_t groupIndex,
    const uint32_t* dynamicUBOs,
    uint32_t dynamicUBOCount)
{
  ore::Context* oreContext = gpuOreContext(vm);
  if (oreContext == nullptr)
  {
    return 0;
  }
  auto host = static_cast<HostGpuShaderModule*>(vm->handles().resolve(
      shaderModule,
      WasmScriptingVM::HandleTable::Tag::gpuShaderModule));
  if (host == nullptr)
  {
    return 0;
  }
  auto layout = ore::makeBindGroupLayoutFromShader(
      *oreContext,
      host->shaderModule.get(),
      groupIndex,
      dynamicUBOs,
      dynamicUBOCount);
  if (layout == nullptr)
  {
    return 0;
  }
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::gpuBindGroupLayout,
      new HostGpuBindGroupLayout{std::move(layout)});
}

uint32_t gpuBindGroupLayoutFromShadersImpl(
    WasmScriptingVM* vm,
    uint32_t vertexModule,
    uint32_t fragmentModule,
    uint32_t groupIndex,
    const uint32_t* dynamicUBOs,
    uint32_t dynamicUBOCount)
{
  ore::Context* oreContext = gpuOreContext(vm);
  if (oreContext == nullptr)
  {
    return 0;
  }
  auto resolveModule = [vm](uint32_t handle) -> ore::ShaderModule* {
    auto host = static_cast<HostGpuShaderModule*>(vm->handles().resolve(
        handle,
        WasmScriptingVM::HandleTable::Tag::gpuShaderModule));
    return host != nullptr ? host->shaderModule.get() : nullptr;
  };
  ore::ShaderModule* vertex = resolveModule(vertexModule);
  ore::ShaderModule* fragment = resolveModule(fragmentModule);
  if (vertex == nullptr || (fragmentModule != 0 && fragment == nullptr))
  {
    return 0;
  }
  auto layout = ore::makeBindGroupLayoutFromBindingMap(
      *oreContext,
      ore::bindingMapForStages(vertex, fragment),
      groupIndex,
      dynamicUBOs,
      dynamicUBOCount,
      vertex,
      fragment);
  if (layout == nullptr)
  {
    return 0;
  }
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::gpuBindGroupLayout,
      new HostGpuBindGroupLayout{std::move(layout)});
}

uint32_t gpuBindGroupLayoutFromEntriesImpl(
    WasmScriptingVM* vm,
    uint32_t vertexModule,
    const char* vertexEntry,
    uint32_t vertexEntryLength,
    uint32_t fragmentModule,
    const char* fragmentEntry,
    uint32_t fragmentEntryLength,
    uint32_t groupIndex)
{
  ore::Context* oreContext = gpuOreContext(vm);
  if (oreContext == nullptr)
  {
    return 0;
  }
  auto resolveModule = [vm](uint32_t handle) -> ore::ShaderModule* {
    auto host = static_cast<HostGpuShaderModule*>(vm->handles().resolve(
        handle,
        WasmScriptingVM::HandleTable::Tag::gpuShaderModule));
    return host != nullptr ? host->shaderModule.get() : nullptr;
  };
  ore::ShaderModule* vertex = resolveModule(vertexModule);
  ore::ShaderModule* fragment = resolveModule(fragmentModule);
  if (vertex == nullptr || (fragmentModule != 0 && fragment == nullptr))
  {
    return 0;
  }
  const std::string vertexName(vertexEntry, vertexEntryLength);
  const std::string fragmentName(fragmentEntry, fragmentEntryLength);
  const ore::BindingMap map = ore::bindingMapForEntries(
      vertex,
      vertexName.c_str(),
      fragment,
      fragmentName.c_str());
  bool bound = false;
  for (size_t i = 0; i < map.size() && !bound; ++i)
  {
    bound = map.at(i).group == groupIndex;
  }
  if (!bound)
  {
    return 0;
  }
  auto layout = ore::makeBindGroupLayoutFromBindingMap(
      *oreContext,
      map,
      groupIndex,
      nullptr,
      0,
      vertex,
      fragment,
      vertexName.c_str(),
      fragmentName.c_str());
  if (layout == nullptr)
  {
    return 0;
  }
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::gpuBindGroupLayout,
      new HostGpuBindGroupLayout{std::move(layout)});
}

uint32_t gpuBindGroupNewImpl(
    WasmScriptingVM* vm,
    uint32_t layoutHandle,
    const rive_gpu_bind_group_ubo_v1* ubos,
    uint32_t uboByteCount,
    const rive_gpu_bind_group_texture_v1* textures,
    uint32_t textureByteCount,
    const rive_gpu_bind_group_sampler_v1* samplers,
    uint32_t samplerByteCount)
{
  ore::Context* oreContext = gpuOreContext(vm);
  if (oreContext == nullptr || uboByteCount % sizeof(*ubos) != 0 ||
      textureByteCount % sizeof(*textures) != 0 ||
      samplerByteCount % sizeof(*samplers) != 0)
  {
    return 0;
  }
  auto layout = static_cast<HostGpuBindGroupLayout*>(vm->handles().resolve(
      layoutHandle,
      WasmScriptingVM::HandleTable::Tag::gpuBindGroupLayout));
  if (layout == nullptr)
  {
    return 0;
  }
  ore::BindGroupDesc desc;
  desc.layout = layout->layout.get();

  uint32_t uboCount = uboByteCount / (uint32_t)sizeof(*ubos);
  std::vector<ore::BindGroupDesc::UBOEntry> uboEntries(uboCount);
  for (uint32_t i = 0; i < uboCount; i++)
  {
    auto buffer = static_cast<HostGpuBuffer*>(vm->handles().resolve(
        ubos[i].buffer,
        WasmScriptingVM::HandleTable::Tag::gpuBuffer));
    if (buffer == nullptr)
    {
      return 0;
    }
    uboEntries[i].slot = ubos[i].slot;
    uboEntries[i].buffer = buffer->buffer.get();
    uboEntries[i].offset = ubos[i].offset;
    uboEntries[i].size = ubos[i].size;
  }
  desc.ubos = uboEntries.data();
  desc.uboCount = uboCount;

  uint32_t texCount = textureByteCount / (uint32_t)sizeof(*textures);
  std::vector<ore::BindGroupDesc::TexEntry> texEntries(texCount);
  for (uint32_t i = 0; i < texCount; i++)
  {
    auto view = static_cast<HostGpuTextureView*>(vm->handles().resolve(
        textures[i].view,
        WasmScriptingVM::HandleTable::Tag::gpuTextureView));
    if (view == nullptr)
    {
      return 0;
    }
    texEntries[i].slot = textures[i].slot;
    texEntries[i].view = view->view.get();
  }
  desc.textures = texEntries.data();
  desc.textureCount = texCount;

  uint32_t sampCount = samplerByteCount / (uint32_t)sizeof(*samplers);
  std::vector<ore::BindGroupDesc::SampEntry> sampEntries(sampCount);
  for (uint32_t i = 0; i < sampCount; i++)
  {
    auto sampler = static_cast<HostGpuSampler*>(vm->handles().resolve(
        samplers[i].sampler,
        WasmScriptingVM::HandleTable::Tag::gpuSampler));
    if (sampler == nullptr)
    {
      return 0;
    }
    sampEntries[i].slot = samplers[i].slot;
    sampEntries[i].sampler = sampler->sampler.get();
  }
  desc.samplers = sampEntries.data();
  desc.samplerCount = sampCount;

  oreContext->clearLastError();
  auto bindGroup = oreContext->makeBindGroup(desc);
  if (bindGroup == nullptr)
  {
    return gpuRejected(vm, oreContext, "GPUBindGroup");
  }
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::gpuBindGroup,
      new HostGpuBindGroup{std::move(bindGroup)});
}

void gpuBindGroupReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
  releaseHostHandle(
      vm,
      handle,
      WasmScriptingVM::HandleTable::Tag::gpuBindGroup);
}

uint32_t gpuPipelineNewImpl(
    WasmScriptingVM* vm,
    const rive_gpu_pipeline_desc_v1* podDesc,
    uint32_t descByteCount,
    const uint8_t* blob,
    uint32_t blobCount)
{
  ore::Context* oreContext = gpuOreContext(vm);
  if (oreContext == nullptr || descByteCount < sizeof(*podDesc))
  {
    return 0;
  }
  uint64_t total = uint64_t(podDesc->vertexEntrySize) +
                   podDesc->fragmentEntrySize +
                   uint64_t(podDesc->colorCount) *
                       sizeof(rive_gpu_color_target_v1) +
                   uint64_t(podDesc->vertexBufferCount) *
                       sizeof(rive_gpu_vertex_buffer_layout_v1) +
                   uint64_t(podDesc->attributeCount) *
                       sizeof(rive_gpu_vertex_attribute_v1) +
                   uint64_t(podDesc->bindGroupLayoutCount) * sizeof(uint32_t);
  if (total > blobCount || podDesc->colorCount > 4 ||
      podDesc->bindGroupLayoutCount > ore::kMaxBindGroups)
  {
    return 0;
  }
  const uint8_t* cursor = blob;
  auto slice = [&cursor](uint32_t size) {
    const uint8_t* begin = cursor;
    cursor += size;
    return size != 0 ? begin : nullptr;
  };

  ore::PipelineDesc desc;
  auto vertexModule = static_cast<HostGpuShaderModule*>(vm->handles().resolve(
      podDesc->vertexModule,
      WasmScriptingVM::HandleTable::Tag::gpuShaderModule));
  auto fragmentModule = static_cast<HostGpuShaderModule*>(vm->handles().resolve(
      podDesc->fragmentModule,
      WasmScriptingVM::HandleTable::Tag::gpuShaderModule));
  if (vertexModule == nullptr)
  {
    vm->raiseModuleError("GPUPipeline.new: 'vertex' is required");
    return 0;
  }
  desc.vertexModule = vertexModule->shaderModule.get();
  desc.fragmentModule = fragmentModule != nullptr
                            ? fragmentModule->shaderModule.get()
                            : nullptr;

  std::string vertexEntry;
  std::string fragmentEntry;
  if (const uint8_t* bytes = slice(podDesc->vertexEntrySize))
  {
    vertexEntry.assign((const char*)bytes, podDesc->vertexEntrySize);
    desc.vertexEntryPoint = vertexEntry.c_str();
  }
  if (const uint8_t* bytes = slice(podDesc->fragmentEntrySize))
  {
    fragmentEntry.assign((const char*)bytes, podDesc->fragmentEntrySize);
    desc.fragmentEntryPoint = fragmentEntry.c_str();
  }

  auto colorTargets = (const rive_gpu_color_target_v1*)slice(
      podDesc->colorCount * (uint32_t)sizeof(rive_gpu_color_target_v1));
  desc.colorCount = podDesc->colorCount;
  for (uint32_t i = 0; i < podDesc->colorCount; i++)
  {
    const auto& in = colorTargets[i];
    if (!validOre(
            vm,
            in.format,
            ore::TextureFormat::astc8x8,
            "TextureFormat") ||
        !validOre(
            vm,
            in.srcColor,
            ore::BlendFactor::oneMinusBlendColor,
            "BlendFactor") ||
        !validOre(
            vm,
            in.dstColor,
            ore::BlendFactor::oneMinusBlendColor,
            "BlendFactor") ||
        !validOre(vm, in.colorOp, ore::BlendOp::max, "BlendOp") ||
        !validOre(
            vm,
            in.srcAlpha,
            ore::BlendFactor::oneMinusBlendColor,
            "BlendFactor") ||
        !validOre(
            vm,
            in.dstAlpha,
            ore::BlendFactor::oneMinusBlendColor,
            "BlendFactor") ||
        !validOre(vm, in.alphaOp, ore::BlendOp::max, "BlendOp") ||
        !validOre(vm, in.writeMask, ore::ColorWriteMask::all, "ColorWriteMask"))
    {
      return 0;
    }
    auto& out = desc.colorTargets[i];
    out.format = (ore::TextureFormat)colorTargets[i].format;
    out.blendEnabled = colorTargets[i].blendEnabled != 0;
    out.blend.srcColor = (ore::BlendFactor)colorTargets[i].srcColor;
    out.blend.dstColor = (ore::BlendFactor)colorTargets[i].dstColor;
    out.blend.colorOp = (ore::BlendOp)colorTargets[i].colorOp;
    out.blend.srcAlpha = (ore::BlendFactor)colorTargets[i].srcAlpha;
    out.blend.dstAlpha = (ore::BlendFactor)colorTargets[i].dstAlpha;
    out.blend.alphaOp = (ore::BlendOp)colorTargets[i].alphaOp;
    out.writeMask = (ore::ColorWriteMask)colorTargets[i].writeMask;
  }

  auto bufferPods = (const rive_gpu_vertex_buffer_layout_v1*)slice(
      podDesc->vertexBufferCount *
      (uint32_t)sizeof(rive_gpu_vertex_buffer_layout_v1));
  auto attributePods = (const rive_gpu_vertex_attribute_v1*)slice(
      podDesc->attributeCount * (uint32_t)sizeof(rive_gpu_vertex_attribute_v1));
  std::vector<ore::VertexAttribute> attributes(podDesc->attributeCount);
  for (uint32_t i = 0; i < podDesc->attributeCount; i++)
  {
    if (!validOre(
            vm,
            attributePods[i].format,
            ore::VertexFormat::uint32,
            "VertexFormat"))
    {
      return 0;
    }
    attributes[i].format = (ore::VertexFormat)attributePods[i].format;
    attributes[i].offset = attributePods[i].offset;
    attributes[i].shaderSlot = attributePods[i].shaderSlot;
  }
  std::vector<ore::VertexBufferLayout> buffers(podDesc->vertexBufferCount);
  uint32_t attributeCursor = 0;
  for (uint32_t i = 0; i < podDesc->vertexBufferCount; i++)
  {
    uint32_t count = bufferPods[i].attributeCount;
    if (uint64_t(attributeCursor) + count > podDesc->attributeCount ||
        !validOre(
            vm,
            bufferPods[i].stepMode,
            ore::VertexStepMode::instance,
            "VertexStepMode"))
    {
      return 0;
    }
    buffers[i].stride = bufferPods[i].stride;
    buffers[i].stepMode = (ore::VertexStepMode)bufferPods[i].stepMode;
    buffers[i].attributes = attributes.data() + attributeCursor;
    buffers[i].attributeCount = count;
    attributeCursor += count;
  }
  desc.vertexBuffers = buffers.data();
  desc.vertexBufferCount = podDesc->vertexBufferCount;

  auto layoutHandles = (const uint32_t*)slice(
      podDesc->bindGroupLayoutCount * (uint32_t)sizeof(uint32_t));
  ore::BindGroupLayout* layouts[ore::kMaxBindGroups] = {};
  for (uint32_t i = 0; i < podDesc->bindGroupLayoutCount; i++)
  {
    if (layoutHandles[i] == 0)
    {
      continue;
    }
    auto layout = static_cast<HostGpuBindGroupLayout*>(vm->handles().resolve(
        layoutHandles[i],
        WasmScriptingVM::HandleTable::Tag::gpuBindGroupLayout));
    if (layout == nullptr)
    {
      return 0;
    }
    layouts[i] = layout->layout.get();
  }
  desc.bindGroupLayouts = layouts;
  desc.bindGroupLayoutCount = podDesc->bindGroupLayoutCount;

  std::string error;
  if (!validOre(
          vm,
          podDesc->topology,
          ore::PrimitiveTopology::triangleStrip,
          "PrimitiveTopology") ||
      !validOre(
          vm,
          podDesc->indexFormat,
          ore::IndexFormat::uint32,
          "IndexFormat") ||
      !validOre(vm, podDesc->cullMode, ore::CullMode::back, "CullMode") ||
      !validOre(
          vm,
          podDesc->winding,
          ore::FaceWinding::counterClockwise,
          "FaceWinding") ||
      !validOre(
          vm,
          podDesc->depthFormat,
          ore::TextureFormat::astc8x8,
          "TextureFormat") ||
      !validOre(
          vm,
          podDesc->depthCompare,
          ore::CompareFunction::always,
          "CompareFunction") ||
      !validOre(
          vm,
          podDesc->stencilFrontCompare,
          ore::CompareFunction::always,
          "CompareFunction") ||
      !validOre(
          vm,
          podDesc->stencilFrontFailOp,
          ore::StencilOp::decrementWrap,
          "StencilOp") ||
      !validOre(
          vm,
          podDesc->stencilFrontDepthFailOp,
          ore::StencilOp::decrementWrap,
          "StencilOp") ||
      !validOre(
          vm,
          podDesc->stencilFrontPassOp,
          ore::StencilOp::decrementWrap,
          "StencilOp") ||
      !validOre(
          vm,
          podDesc->stencilBackCompare,
          ore::CompareFunction::always,
          "CompareFunction") ||
      !validOre(
          vm,
          podDesc->stencilBackFailOp,
          ore::StencilOp::decrementWrap,
          "StencilOp") ||
      !validOre(
          vm,
          podDesc->stencilBackDepthFailOp,
          ore::StencilOp::decrementWrap,
          "StencilOp") ||
      !validOre(
          vm,
          podDesc->stencilBackPassOp,
          ore::StencilOp::decrementWrap,
          "StencilOp") ||
      !guardPassed(
          vm,
          ore::validateSampleCount(oreContext, podDesc->sampleCount, &error),
          error) ||
      !guardPassed(
          vm,
          ore::validateVertexLayout(oreContext, desc, &error),
          error))
  {
    return 0;
  }
  desc.topology = (ore::PrimitiveTopology)podDesc->topology;
  desc.indexFormat = (ore::IndexFormat)podDesc->indexFormat;
  desc.cullMode = (ore::CullMode)podDesc->cullMode;
  desc.winding = (ore::FaceWinding)podDesc->winding;
  desc.depthStencil.format = (ore::TextureFormat)podDesc->depthFormat;
  desc.depthStencil.depthCompare = (ore::CompareFunction)podDesc->depthCompare;
  desc.depthStencil.depthWriteEnabled = podDesc->depthWriteEnabled != 0;
  desc.depthStencil.depthBias = (int32_t)podDesc->depthBias;
  desc.depthStencil.depthBiasSlopeScale = podDesc->depthBiasSlopeScale;
  desc.depthStencil.depthBiasClamp = podDesc->depthBiasClamp;
  desc.stencilFront.compare = (ore::CompareFunction)
                                  podDesc->stencilFrontCompare;
  desc.stencilFront.failOp = (ore::StencilOp)podDesc->stencilFrontFailOp;
  desc.stencilFront.depthFailOp = (ore::StencilOp)
                                      podDesc->stencilFrontDepthFailOp;
  desc.stencilFront.passOp = (ore::StencilOp)podDesc->stencilFrontPassOp;
  desc.stencilBack.compare = (ore::CompareFunction)podDesc->stencilBackCompare;
  desc.stencilBack.failOp = (ore::StencilOp)podDesc->stencilBackFailOp;
  desc.stencilBack.depthFailOp = (ore::StencilOp)
                                     podDesc->stencilBackDepthFailOp;
  desc.stencilBack.passOp = (ore::StencilOp)podDesc->stencilBackPassOp;
  desc.stencilReadMask = (uint8_t)podDesc->stencilReadMask;
  desc.stencilWriteMask = (uint8_t)podDesc->stencilWriteMask;
  desc.sampleCount = podDesc->sampleCount;

  oreContext->clearLastError();
  auto pipeline = oreContext->makePipeline(desc);
  if (pipeline == nullptr)
  {
    return gpuRejected(vm, oreContext, "GPUPipeline");
  }
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::gpuPipeline,
      new HostGpuPipeline{std::move(pipeline)});
}

void gpuPipelineReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
  releaseHostHandle(vm, handle, WasmScriptingVM::HandleTable::Tag::gpuPipeline);
}
#else
// Builds without the canvas renderer keep the namespace linkable; scripts
// see the same nil/no-op surface the Luau backend presents there.
uint32_t gpuCanvasNewImpl(WasmScriptingVM*, uint32_t, uint32_t) { return 0; }
void gpuCanvasReleaseImpl(WasmScriptingVM*, uint32_t) {}
uint32_t gpuCanvasColorViewImpl(WasmScriptingVM*, uint32_t, uint32_t*, uint32_t)
{
  return 0;
}
uint32_t gpuCanvasImageImpl(WasmScriptingVM*, uint32_t) { return 0; }
uint32_t gpuCanvasResizeImpl(
    WasmScriptingVM*,
    uint32_t,
    uint32_t,
    uint32_t,
    uint32_t*,
    uint32_t)
{
  return 0;
}
uint32_t gpuTargetViewImpl(WasmScriptingVM*, uint32_t, uint32_t*, uint32_t)
{
  return 0;
}
uint32_t gpuFeaturesImpl(WasmScriptingVM*, uint32_t*, uint32_t) { return 0; }
uint32_t gpuPassBeginImpl(
    WasmScriptingVM*,
    const rive_gpu_pass_desc_v1*,
    uint32_t,
    const uint8_t*,
    uint32_t)
{
  return 0;
}
void gpuPassSetPipelineImpl(WasmScriptingVM*, uint32_t, uint32_t) {}
void gpuPassSetVertexBufferImpl(
    WasmScriptingVM*,
    uint32_t,
    uint32_t,
    uint32_t,
    uint32_t)
{}
void gpuPassSetIndexBufferImpl(
    WasmScriptingVM*,
    uint32_t,
    uint32_t,
    uint32_t,
    uint32_t)
{}
void gpuPassSetBindGroupImpl(
    WasmScriptingVM*,
    uint32_t,
    uint32_t,
    uint32_t,
    const uint32_t*,
    uint32_t)
{}
void gpuPassSetViewportImpl(
    WasmScriptingVM*,
    uint32_t,
    float,
    float,
    float,
    float,
    float,
    float)
{}
void gpuPassSetScissorImpl(
    WasmScriptingVM*,
    uint32_t,
    uint32_t,
    uint32_t,
    uint32_t,
    uint32_t)
{}
void gpuPassSetStencilReferenceImpl(WasmScriptingVM*, uint32_t, uint32_t) {}
void gpuPassSetBlendColorImpl(
    WasmScriptingVM*,
    uint32_t,
    float,
    float,
    float,
    float)
{}
void gpuPassDrawImpl(
    WasmScriptingVM*,
    uint32_t,
    uint32_t,
    uint32_t,
    uint32_t,
    uint32_t)
{}
void gpuPassDrawIndexedImpl(
    WasmScriptingVM*,
    uint32_t,
    uint32_t,
    uint32_t,
    uint32_t,
    int32_t,
    uint32_t)
{}
void gpuPassFinishImpl(WasmScriptingVM*, uint32_t) {}
void gpuPassReleaseImpl(WasmScriptingVM*, uint32_t) {}
uint32_t gpuImageViewImpl(WasmScriptingVM*, uint32_t, uint32_t, uint32_t)
{
  return 0;
}
uint32_t gpuBufferNewImpl(
    WasmScriptingVM*,
    uint32_t,
    uint32_t,
    uint32_t,
    const uint8_t*,
    uint32_t)
{
  return 0;
}
void gpuBufferUpdateImpl(
    WasmScriptingVM*,
    uint32_t,
    uint32_t,
    const uint8_t*,
    uint32_t)
{}
void gpuBufferUpdateBlobImpl(
    WasmScriptingVM*,
    uint32_t,
    uint32_t,
    uint32_t,
    uint32_t,
    uint32_t)
{}
uint32_t gpuBufferNewBlobImpl(
    WasmScriptingVM*,
    uint32_t,
    uint32_t,
    uint32_t,
    uint32_t,
    uint32_t)
{
  return 0;
}
void gpuBufferReleaseImpl(WasmScriptingVM*, uint32_t) {}
uint32_t gpuTextureNewImpl(
    WasmScriptingVM*,
    const rive_gpu_texture_desc_v1*,
    uint32_t)
{
  return 0;
}
void gpuTextureUploadImpl(
    WasmScriptingVM*,
    uint32_t,
    const rive_gpu_texture_upload_v1*,
    uint32_t,
    const uint8_t*,
    uint32_t)
{}
void gpuTextureReleaseImpl(WasmScriptingVM*, uint32_t) {}
uint32_t gpuSamplerNewImpl(
    WasmScriptingVM*,
    const rive_gpu_sampler_desc_v1*,
    uint32_t)
{
  return 0;
}
void gpuSamplerReleaseImpl(WasmScriptingVM*, uint32_t) {}
uint32_t gpuTextureViewNewImpl(
    WasmScriptingVM*,
    uint32_t,
    const rive_gpu_texture_view_desc_v1*,
    uint32_t)
{
  return 0;
}
void gpuTextureViewReleaseImpl(WasmScriptingVM*, uint32_t) {}
uint32_t gpuShaderTargetImpl(WasmScriptingVM*) { return 0; }
uint32_t gpuShaderAssetBytesImpl(
    WasmScriptingVM*,
    uint32_t,
    const char*,
    uint32_t,
    uint8_t*,
    uint32_t)
{
  return 0;
}
uint32_t gpuShaderAssetIdImpl(WasmScriptingVM*, uint32_t, const char*, uint32_t)
{
  return 0;
}
uint32_t gpuShaderModuleFromAssetImpl(
    WasmScriptingVM*,
    uint32_t,
    const char*,
    uint32_t,
    uint32_t)
{
  return 0;
}
#ifdef WITH_RIVE_TOOLS
uint32_t gpuShaderModuleFromRstbImpl(
    WasmScriptingVM*,
    const uint8_t*,
    uint32_t,
    uint32_t,
    uint32_t)
{
  return 0;
}
#endif
void gpuShaderModuleReleaseImpl(WasmScriptingVM*, uint32_t) {}
uint32_t gpuBindGroupLayoutNewImpl(
    WasmScriptingVM*,
    uint32_t,
    const rive_gpu_bind_group_layout_entry_v1*,
    uint32_t)
{
  return 0;
}
void gpuBindGroupLayoutReleaseImpl(WasmScriptingVM*, uint32_t) {}
uint32_t gpuBindGroupLayoutFromShaderImpl(
    WasmScriptingVM*,
    uint32_t,
    uint32_t,
    const uint32_t*,
    uint32_t)
{
  return 0;
}
uint32_t gpuBindGroupLayoutFromShadersImpl(
    WasmScriptingVM*,
    uint32_t,
    uint32_t,
    uint32_t,
    const uint32_t*,
    uint32_t)
{
  return 0;
}
uint32_t gpuBindGroupLayoutFromEntriesImpl(
    WasmScriptingVM*,
    uint32_t,
    const char*,
    uint32_t,
    uint32_t,
    const char*,
    uint32_t,
    uint32_t)
{
  return 0;
}
uint32_t gpuBindGroupNewImpl(
    WasmScriptingVM*,
    uint32_t,
    const rive_gpu_bind_group_ubo_v1*,
    uint32_t,
    const rive_gpu_bind_group_texture_v1*,
    uint32_t,
    const rive_gpu_bind_group_sampler_v1*,
    uint32_t)
{
  return 0;
}
void gpuBindGroupReleaseImpl(WasmScriptingVM*, uint32_t) {}
uint32_t gpuPipelineNewImpl(
    WasmScriptingVM*,
    const rive_gpu_pipeline_desc_v1*,
    uint32_t,
    const uint8_t*,
    uint32_t)
{
  return 0;
}
void gpuPipelineReleaseImpl(WasmScriptingVM*, uint32_t) {}
#endif

// A renderer the module draws through, with the saves it opened in the scope
// in flight; it may restore only those, never the host's.
struct HostRenderer
{
  Renderer* renderer = nullptr;
  int saves = 0;

  // A trapped call returns with its saves open, and hosts reuse renderers
  // frame to frame.
  void restoreSaves()
  {
    for (; saves > 0; saves--)
    {
      renderer->restore();
    }
  }
};

HostRenderer* resolveHostRenderer(WasmScriptingVM* vm, uint32_t handle)
{
  if (vm == nullptr)
  {
    return nullptr;
  }
  return static_cast<HostRenderer*>(vm->handles().resolve(
      handle,
      WasmScriptingVM::HandleTable::Tag::renderer));
}

#ifdef RIVE_CANVAS
// Frames go through the deferred canvas host, the only path that can open
// inside the screen frame already open around the draw.
struct HostCanvas
{
  rcp<gpu::RenderCanvas> canvas;
  // A size requested before the device bound, allocated on first use.
  uint32_t pendingWidth = 0;
  uint32_t pendingHeight = 0;
  cmd::DeferredCanvasHost* frameHost = nullptr;
  HostRenderer frameRenderer;
  uint32_t rendererHandle = 0;
};

HostCanvas* resolveCanvas(WasmScriptingVM* vm, uint32_t handle)
{
  if (vm == nullptr)
  {
    return nullptr;
  }
  return static_cast<HostCanvas*>(
      vm->handles().resolve(handle, WasmScriptingVM::HandleTable::Tag::canvas));
}

// Allocates a pending size once a device is there to allocate against.
// False only when a device is present and refuses.
bool canvasSatisfyPending(WasmScriptingVM* vm, HostCanvas* host)
{
  if (host->pendingWidth == 0 || host->pendingHeight == 0)
  {
    return true;
  }
  auto* renderContext = static_cast<gpu::RenderContext*>(
      vm->factory()->renderContext());
  if (renderContext == nullptr)
  {
    return true;
  }
  // A refused size leaves the previous backing in place.
  auto* deferredHost = vm->factory()->deferredCanvasHost();
  rcp<gpu::RenderCanvas> canvas = deferredHost != nullptr
                                      ? deferredHost->makeContentCanvas(
                                            host->pendingWidth,
                                            host->pendingHeight)
                                      : renderContext->makeRenderCanvas(
                                            host->pendingWidth,
                                            host->pendingHeight);
  host->pendingWidth = 0;
  host->pendingHeight = 0;
  if (canvas == nullptr)
  {
    return false;
  }
  host->canvas = std::move(canvas);
  return true;
}

uint32_t canvasNewImpl(WasmScriptingVM* vm, uint32_t width, uint32_t height)
{
  if (vm == nullptr || vm->factory() == nullptr)
  {
    return 0;
  }
  auto* host = new HostCanvas();
  if (width != 0 && height != 0)
  {
    host->pendingWidth = width;
    host->pendingHeight = height;
    // A size only waits when a recording host will bind a device later.
    if (!canvasSatisfyPending(vm, host) ||
        (host->canvas == nullptr &&
         vm->factory()->deferredCanvasHost() == nullptr))
    {
      delete host;
      return 0;
    }
  }
  return vm->handles().mint(WasmScriptingVM::HandleTable::Tag::canvas, host);
}

void canvasReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
  auto* host = resolveCanvas(vm, handle);
  if (host == nullptr)
  {
    return;
  }
  canvasEndFrameImpl(vm, handle);
  delete host;
  vm->handles().release(handle, WasmScriptingVM::HandleTable::Tag::canvas);
}

uint32_t canvasWidthImpl(WasmScriptingVM* vm, uint32_t handle)
{
  auto* host = resolveCanvas(vm, handle);
  if (host == nullptr)
  {
    return 0;
  }
  canvasSatisfyPending(vm, host);
  return host->canvas != nullptr ? host->canvas->width() : host->pendingWidth;
}

uint32_t canvasHeightImpl(WasmScriptingVM* vm, uint32_t handle)
{
  auto* host = resolveCanvas(vm, handle);
  if (host == nullptr)
  {
    return 0;
  }
  canvasSatisfyPending(vm, host);
  return host->canvas != nullptr ? host->canvas->height() : host->pendingHeight;
}

uint32_t canvasResizeImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint32_t width,
    uint32_t height)
{
  auto* host = resolveCanvas(vm, handle);
  if (host == nullptr || host->frameHost != nullptr)
  {
    return 0;
  }
  if (width == 0 || height == 0)
  {
    host->canvas = nullptr;
    host->pendingWidth = 0;
    host->pendingHeight = 0;
    return 1;
  }
  // An unchanged size would churn a new texture per frame.
  if (host->canvas != nullptr && host->canvas->width() == width &&
      host->canvas->height() == height)
  {
    return 1;
  }
  host->pendingWidth = width;
  host->pendingHeight = height;
  return canvasSatisfyPending(vm, host) ? 1 : 0;
}

uint32_t canvasImageImpl(WasmScriptingVM* vm, uint32_t handle)
{
  auto* host = resolveCanvas(vm, handle);
  if (host == nullptr)
  {
    return 0;
  }
  canvasSatisfyPending(vm, host);
  if (host->canvas == nullptr || host->canvas->renderImage() == nullptr)
  {
    return 0;
  }
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::image,
      new HostImage{
          ref_rcp(static_cast<RenderImage*>(host->canvas->renderImage())),
          host->canvas});
}

uint32_t canvasBeginFrameImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint32_t clearColor)
{
  auto* host = resolveCanvas(vm, handle);
  if (host == nullptr || host->frameHost != nullptr)
  {
    return 0;
  }
  canvasSatisfyPending(vm, host);
  cmd::DeferredCanvasHost* frameHost = vm->factory()->deferredCanvasHost();
  if (host->canvas == nullptr || frameHost == nullptr)
  {
    return 0;
  }
  Renderer* renderer = frameHost->beginCanvasContent(
      host->canvas.get(),
      clearColor);
  if (renderer == nullptr)
  {
    return 0;
  }
  host->frameHost = frameHost;
  host->frameRenderer = {renderer};
  host->rendererHandle = vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::renderer,
      &host->frameRenderer);
  vm->registerOpenCanvasFrame(handle);
  return host->rendererHandle;
}

void canvasEndFrameImpl(WasmScriptingVM* vm, uint32_t handle)
{
  auto* host = resolveCanvas(vm, handle);
  if (host == nullptr || host->frameHost == nullptr)
  {
    return;
  }
  vm->unregisterOpenCanvasFrame(handle);
  // Releasing bumps the generation so a stashed renderer goes stale.
  vm->handles().release(
      host->rendererHandle,
      WasmScriptingVM::HandleTable::Tag::renderer);
  host->rendererHandle = 0;
  host->frameRenderer.restoreSaves();
  host->frameHost->endCanvasContent(host->canvas.get());
  host->frameHost = nullptr;
}
#else
uint32_t canvasNewImpl(WasmScriptingVM*, uint32_t, uint32_t) { return 0; }
void canvasReleaseImpl(WasmScriptingVM*, uint32_t) {}
uint32_t canvasWidthImpl(WasmScriptingVM*, uint32_t) { return 0; }
uint32_t canvasHeightImpl(WasmScriptingVM*, uint32_t) { return 0; }
uint32_t canvasResizeImpl(WasmScriptingVM*, uint32_t, uint32_t, uint32_t)
{
  return 0;
}
uint32_t canvasImageImpl(WasmScriptingVM*, uint32_t) { return 0; }
uint32_t canvasBeginFrameImpl(WasmScriptingVM*, uint32_t, uint32_t)
{
  return 0;
}
void canvasEndFrameImpl(WasmScriptingVM*, uint32_t) {}
#endif

struct HostBuffer
{
  rcp<RenderBuffer> buffer;
  bool spent = false;
};

HostBuffer* resolveBuffer(WasmScriptingVM* vm, uint32_t handle)
{
  if (vm == nullptr)
  {
    return nullptr;
  }
  return static_cast<HostBuffer*>(
      vm->handles().resolve(handle, WasmScriptingVM::HandleTable::Tag::buffer));
}

uint32_t bufferNewImpl(
    WasmScriptingVM* vm,
    uint32_t bufferType,
    uint32_t flags,
    uint32_t sizeInBytes)
{
  if (vm == nullptr ||
      !validOrdinal(
          vm,
          bufferType,
          bufferType <= (uint32_t)RenderBufferType::vertex,
          "RenderBufferType") ||
      !validOrdinal(
          vm,
          flags,
          flags <= (uint32_t)RenderBufferFlags::mappedOnceAtInitialization,
          "RenderBufferFlags"))
  {
    return 0;
  }
  auto buffer = vm->factory()->makeRenderBuffer(
      (RenderBufferType)bufferType,
      (RenderBufferFlags)flags,
      sizeInBytes);
  if (buffer == nullptr)
  {
    return 0;
  }
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::buffer,
      new HostBuffer{std::move(buffer)});
}

void bufferUpdateImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    const uint8_t* bytes,
    uint32_t byteCount)
{
  auto host = resolveBuffer(vm, handle);
  if (host == nullptr || byteCount != host->buffer->sizeInBytes())
  {
    return;
  }
  if (host->spent)
  {
    vm->raiseModuleError(
        "RenderBuffer: a buffer mapped once at initialization takes one "
        "update");
    return;
  }
  host->spent = enums::is_flag_set(
      host->buffer->flags(),
      RenderBufferFlags::mappedOnceAtInitialization);
  void* data = host->buffer->map();
  if (data != nullptr)
  {
    memcpy(data, bytes, byteCount);
    host->buffer->unmap();
  }
}

void bufferReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
  releaseHostHandle(vm, handle, WasmScriptingVM::HandleTable::Tag::buffer);
}

struct HostMeshInstances
{
  rcp<ImageMeshInstances> instances;
};

HostMeshInstances* resolveMeshInstances(WasmScriptingVM* vm, uint32_t handle)
{
  if (vm == nullptr)
  {
    return nullptr;
  }
  return static_cast<HostMeshInstances*>(vm->handles().resolve(
      handle,
      WasmScriptingVM::HandleTable::Tag::meshInstances));
}

uint32_t meshInstancesNewImpl(WasmScriptingVM* vm, uint32_t count)
{
  if (vm == nullptr)
  {
    return 0;
  }
  auto instances = vm->factory()->makeImageMeshInstances(count);
  if (instances == nullptr)
  {
    return 0;
  }
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::meshInstances,
      new HostMeshInstances{std::move(instances)});
}

void meshInstancesResizeImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint32_t count)
{
  auto host = resolveMeshInstances(vm, handle);
  if (host == nullptr)
  {
    return;
  }
  host->instances->edit(count);
  host->instances->endEdit();
}

void meshInstancesUpdateImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint32_t first,
    const uint8_t* bytes,
    uint32_t byteCount)
{
  auto host = resolveMeshInstances(vm, handle);
  if (host == nullptr || byteCount % sizeof(ImageMeshInstanceData) != 0)
  {
    return;
  }
  size_t count = byteCount / sizeof(ImageMeshInstanceData);
  Span<ImageMeshInstanceData> data = host->instances->edit();
  if (first <= data.size() && count <= data.size() - first)
  {
    memcpy(data.data() + first, bytes, byteCount);
  }
  host->instances->endEdit();
}

void meshInstancesReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
  releaseHostHandle(
      vm,
      handle,
      WasmScriptingVM::HandleTable::Tag::meshInstances);
}

Renderer* resolveRenderer(WasmScriptingVM* vm, uint32_t handle)
{
  auto host = resolveHostRenderer(vm, handle);
  return host != nullptr ? host->renderer : nullptr;
}

uint32_t testBlobImpl(
    WasmScriptingVM* vm,
    const char* name,
    uint32_t nameLength,
    uint8_t* out,
    uint32_t outCount)
{
  if (vm == nullptr)
  {
    return 0;
  }
  return fillOut(vm->testBlob(std::string(name, nameLength)), out, outCount);
}

// A transition child handle points at the caller's TransitionChildRef and
// is released when the call returns.
const ScriptBackend::TransitionChildRef* resolveTransitionChild(
    WasmScriptingVM* vm,
    uint32_t handle)
{
  if (vm == nullptr)
  {
    return nullptr;
  }
  return static_cast<const ScriptBackend::TransitionChildRef*>(
      vm->handles().resolve(
          handle,
          WasmScriptingVM::HandleTable::Tag::transitionChild));
}

void transitionChildDrawImpl(
    WasmScriptingVM* vm,
    uint32_t child,
    uint32_t rendererHandle)
{
  auto ref = resolveTransitionChild(vm, child);
  Renderer* renderer = resolveRenderer(vm, rendererHandle);
  if (ref != nullptr && renderer != nullptr)
  {
    ref->draw(renderer);
  }
}

float transitionChildWidthImpl(WasmScriptingVM* vm, uint32_t child)
{
  auto ref = resolveTransitionChild(vm, child);
  return ref != nullptr ? ref->width() : 0.0f;
}

float transitionChildHeightImpl(WasmScriptingVM* vm, uint32_t child)
{
  auto ref = resolveTransitionChild(vm, child);
  return ref != nullptr ? ref->height() : 0.0f;
}

void rendererSaveImpl(WasmScriptingVM* vm, uint32_t handle)
{
  if (auto host = resolveHostRenderer(vm, handle))
  {
    host->renderer->save();
    host->saves++;
  }
}
void rendererRestoreImpl(WasmScriptingVM* vm, uint32_t handle)
{
  auto host = resolveHostRenderer(vm, handle);
  if (host == nullptr)
  {
    return;
  }
  if (host->saves == 0)
  {
    vm->raiseModuleError("Renderer save/restore stack was unbalanced by "
                         "trying to restore more times than saved.");
    return;
  }
  host->renderer->restore();
  host->saves--;
}
void rendererTransformImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    float xx,
    float xy,
    float yx,
    float yy,
    float tx,
    float ty)
{
  if (auto renderer = resolveRenderer(vm, handle))
  {
    renderer->transform(Mat2D(xx, xy, yx, yy, tx, ty));
  }
}
void rendererDrawPathImpl(
    WasmScriptingVM* vm,
    uint32_t rendererHandle,
    uint32_t pathHandle,
    uint32_t paintHandle)
{
  auto renderer = resolveRenderer(vm, rendererHandle);
  auto paint = resolvePaint(vm, paintHandle);
  if (vm == nullptr || renderer == nullptr || paint == nullptr)
  {
    return;
  }
  RenderPath* path = hostRenderPath(vm, resolvePath(vm, pathHandle));
  if (path == nullptr)
  {
    return;
  }
  renderer->drawPath(path, paint);
}
void rendererDrawImageImpl(
    WasmScriptingVM* vm,
    uint32_t rendererHandle,
    uint32_t imageHandle,
    uint32_t samplerKey,
    uint32_t blend,
    float opacity)
{
  auto renderer = resolveRenderer(vm, rendererHandle);
  auto host = resolveImage(vm, imageHandle);
  if (renderer == nullptr || host == nullptr ||
      !validSamplerKey(vm, samplerKey) || !validBlendMode(vm, blend))
  {
    return;
  }
  renderer->drawImage(
      host->image.get(),
      ImageSampler::SamplerFromKey((uint8_t)samplerKey),
      (BlendMode)blend,
      opacity);
}

void rendererDrawImageMeshImpl(
    WasmScriptingVM* vm,
    uint32_t rendererHandle,
    uint32_t imageHandle,
    uint32_t samplerKey,
    uint32_t vertexHandle,
    uint32_t uvHandle,
    uint32_t indexHandle,
    uint32_t blend,
    float opacity)
{
  auto renderer = resolveRenderer(vm, rendererHandle);
  auto image = resolveImage(vm, imageHandle);
  auto vertex = resolveBuffer(vm, vertexHandle);
  auto uv = resolveBuffer(vm, uvHandle);
  auto index = resolveBuffer(vm, indexHandle);
  if (renderer == nullptr || image == nullptr || vertex == nullptr ||
      uv == nullptr || index == nullptr || !validSamplerKey(vm, samplerKey) ||
      !validBlendMode(vm, blend))
  {
    return;
  }
  renderer->drawImageMesh(
      image->image.get(),
      ImageSampler::SamplerFromKey((uint8_t)samplerKey),
      vertex->buffer,
      uv->buffer,
      index->buffer,
      (uint32_t)(vertex->buffer->sizeInBytes() / sizeof(Vec2D)),
      (uint32_t)(index->buffer->sizeInBytes() / sizeof(uint16_t)),
      (BlendMode)blend,
      opacity);
}

void rendererDrawImageMeshInstancedImpl(
    WasmScriptingVM* vm,
    uint32_t rendererHandle,
    uint32_t imageHandle,
    uint32_t samplerKey,
    uint32_t vertexHandle,
    uint32_t uvHandle,
    uint32_t indexHandle,
    uint32_t instancesHandle)
{
  auto renderer = resolveRenderer(vm, rendererHandle);
  auto image = resolveImage(vm, imageHandle);
  auto vertex = resolveBuffer(vm, vertexHandle);
  auto uv = resolveBuffer(vm, uvHandle);
  auto index = resolveBuffer(vm, indexHandle);
  auto instances = resolveMeshInstances(vm, instancesHandle);
  if (renderer == nullptr || image == nullptr || vertex == nullptr ||
      uv == nullptr || index == nullptr || instances == nullptr ||
      !validSamplerKey(vm, samplerKey))
  {
    return;
  }
  renderer->drawImageMeshInstanced(
      image->image.get(),
      ImageSampler::SamplerFromKey((uint8_t)samplerKey),
      vertex->buffer,
      uv->buffer,
      index->buffer,
      (uint32_t)(vertex->buffer->sizeInBytes() / sizeof(Vec2D)),
      (uint32_t)(index->buffer->sizeInBytes() / sizeof(uint16_t)),
      instances->instances);
}

void rendererClipPathImpl(
    WasmScriptingVM* vm,
    uint32_t rendererHandle,
    uint32_t pathHandle)
{
  auto renderer = resolveRenderer(vm, rendererHandle);
  if (vm == nullptr || renderer == nullptr)
  {
    return;
  }
  RenderPath* path = hostRenderPath(vm, resolvePath(vm, pathHandle));
  if (path == nullptr)
  {
    return;
  }
  renderer->clipPath(path);
}

void rendererModulateOpacityImpl(
    WasmScriptingVM* vm,
    uint32_t rendererHandle,
    float opacity)
{
  auto renderer = resolveRenderer(vm, rendererHandle);
  if (renderer != nullptr)
  {
    renderer->modulateOpacity(opacity);
  }
}

void rendererModulateColorImpl(
    WasmScriptingVM* vm,
    uint32_t rendererHandle,
    uint32_t color,
    uint32_t replace)
{
  auto renderer = resolveRenderer(vm, rendererHandle);
  if (renderer != nullptr)
  {
    renderer->modulateColor(color, replace != 0);
  }
}

void rtLogImpl(
    WasmScriptingVM* vm,
    int32_t level,
    const char* message,
    uint32_t length)
{
  WasmScriptingVMNatives::print(vm, message, length);
  WasmScriptingVMNatives::print(vm, "\n", 1);
}

void rtBudgetExceededImpl(WasmScriptingVM* vm, uint32_t ms)
{
  vm->reportError(budgetExceededLine((int)ms));
  // Terminates like a trap when the native returns, so the caller's
  // failed-op handling engages.
  vm->raiseBudgetExceeded();
}

void rtErrorImpl(WasmScriptingVM* vm, const char* message, uint32_t length)
{
  vm->raiseThrown(std::string(message, length).c_str());
}

// Unfused multiply-add keeps these bit identical to the module's own f32
// math, on every tier and platform.
#pragma clang fp contract(off)
void mat4MultiplyImpl(
    WasmScriptingVM*,
    uint8_t* outBytes,
    uint32_t outByteCount,
    const uint8_t* aBytes,
    uint32_t aByteCount,
    const uint8_t* bBytes,
    uint32_t bByteCount)
{
  if (outByteCount < 64 || aByteCount < 64 || bByteCount < 64)
  {
    return;
  }
  float a[16];
  float b[16];
  memcpy(a, aBytes, sizeof(a));
  memcpy(b, bBytes, sizeof(b));
  float result[16];
  for (int col = 0; col < 4; col++)
  {
    for (int row = 0; row < 4; row++)
    {
      result[col * 4 + row] = a[row] * b[col * 4] +
                              a[4 + row] * b[col * 4 + 1] +
                              a[8 + row] * b[col * 4 + 2] +
                              a[12 + row] * b[col * 4 + 3];
    }
  }
  memcpy(outBytes, result, sizeof(result));
}

uint32_t mat4InvertImpl(
    WasmScriptingVM*,
    uint8_t* outBytes,
    uint32_t outByteCount,
    const uint8_t* srcBytes,
    uint32_t srcByteCount)
{
  if (outByteCount < 64 || srcByteCount < 64)
  {
    return 0;
  }
  float m[16];
  memcpy(m, srcBytes, sizeof(m));
  float inv[16];
  inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] +
           m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
  inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] -
           m[8] * m[7] * m[14] - m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
  inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] +
           m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
  inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] -
            m[8] * m[6] * m[13] - m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
  inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] -
           m[9] * m[3] * m[14] - m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
  inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] +
           m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
  inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] -
           m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
  inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] +
            m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
  inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] +
           m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
  inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] -
           m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
  inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] +
            m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
  inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] -
            m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
  inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] -
           m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
  inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] +
           m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
  inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] -
            m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
  inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] +
            m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
  float det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
  if (det == 0)
  {
    return 0;
  }
  float detInv = 1.0f / det;
  for (int i = 0; i < 16; i++)
  {
    inv[i] *= detInv;
  }
  memcpy(outBytes, inv, sizeof(inv));
  return 1;
}
#pragma clang fp contract(on)

uint32_t rtUtcOffsetImpl(WasmScriptingVM* vm, double epochSeconds)
{
  time_t at;
  struct tm local;
  return localTime(epochSeconds, at, local) ? (uint32_t)utcOffsetOf(local, at)
                                            : 0;
}

uint32_t rtIsDstImpl(WasmScriptingVM* vm, double epochSeconds)
{
  time_t at;
  struct tm local;
  return localTime(epochSeconds, at, local) && local.tm_isdst > 0 ? 1 : 0;
}

uint32_t rtZoneNameImpl(
    WasmScriptingVM* vm,
    double epochSeconds,
    char* buffer,
    uint32_t capacity)
{
  time_t at;
  struct tm local;
  if (!localTime(epochSeconds, at, local))
  {
    return 0;
  }
  char name[64];
  size_t length = strftime(name, sizeof(name), "%Z", &local);
  return fillModuleString(vm, std::string(name, length), buffer, capacity);
}

// Module start has no exec env to carry the vm; the probes its top level
// hits belong to the VM being booted.
static WasmDebugHooks* debugHooksFor(WasmScriptingVM*& vm)
{
  if (vm == nullptr)
  {
    vm = s_booting;
  }
  return vm != nullptr ? vm->debugHooks() : nullptr;
}

void rtDebugEnterImpl(WasmScriptingVM* vm, uint32_t function, uint32_t line)
{
  if (WasmDebugHooks* hooks = debugHooksFor(vm))
  {
    hooks->onEnter(*vm, function, line);
  }
}

uint32_t rtDebugLineImpl(WasmScriptingVM* vm, uint32_t line)
{
  WasmDebugHooks* hooks = debugHooksFor(vm);
  return hooks != nullptr && hooks->onLine(*vm, line) ? 1 : 0;
}

void rtDebugLeaveImpl(WasmScriptingVM* vm)
{
  if (WasmDebugHooks* hooks = debugHooksFor(vm))
  {
    hooks->onLeave(*vm);
  }
}

void rtMarkNeedsUpdateImpl(WasmScriptingVM* vm, uint32_t objectHandle)
{
  if (vm == nullptr)
  {
    return;
  }
  auto object = static_cast<ScriptedObject*>(vm->handles().resolve(
      objectHandle,
      WasmScriptingVM::HandleTable::Tag::object));
  if (object != nullptr)
  {
    object->markNeedsUpdate();
  }
}

// --- rive_data_v1: view model instances and their values, pinned per handle

struct HostViewModelInstance
{
  rcp<ViewModelInstance> instance;
};

// Forwards core value change notifications into the module's listener
// registry by token.
struct HostValueDelegate final : public ViewModelInstanceValueDelegate
{
  WasmScriptingVM* vm = nullptr;
  uint32_t token = 0;
  void valueChanged() override { vm->notifyDataValueChanged(token); }
};

struct HostInstanceValue
{
  // Values and list items point back at their instance raw, so the handle
  // keeps it alive.
  rcp<ViewModelInstance> owner;
  rcp<ViewModelInstanceValue> value;
  HostValueDelegate* delegate = nullptr;
  // What string_get last handed the module, for string_changed.
  std::string sent;
  bool hasSent = false;

  explicit HostInstanceValue(ViewModelInstanceValue* instanceValue)
      : owner(ref_rcp(instanceValue->viewModelInstance()))
      , value(ref_rcp(instanceValue))
  {}

  ~HostInstanceValue()
  {
    if (delegate != nullptr)
    {
      value->removeDelegate(delegate);
      delete delegate;
    }
  }
};

void dataConvertResultImpl(
    WasmScriptingVM* vm,
    uint32_t kind,
    float number,
    uint32_t booleanValue,
    uint32_t color,
    const char* value,
    uint32_t length)
{
  ScriptBackend::ScriptDataResult* out = vm != nullptr ? vm->convertResultOut()
                                                       : nullptr;
  if (out == nullptr)
  {
    return;
  }
  switch (kind)
  {
    case DataConvertWire::kindNumber:
      out->kind = ScriptBackend::ScriptDataResult::Kind::number;
      out->number = number;
      break;
    case DataConvertWire::kindString:
      out->kind = ScriptBackend::ScriptDataResult::Kind::string;
      out->string.assign(value, length);
      break;
    case DataConvertWire::kindBoolean:
      out->kind = ScriptBackend::ScriptDataResult::Kind::boolean;
      out->boolean = booleanValue != 0;
      break;
    case DataConvertWire::kindColor:
      out->kind = ScriptBackend::ScriptDataResult::Kind::color;
      out->color = (int)color;
      break;
    default:
      break;
  }
}

uint32_t dataViewModelImpl(WasmScriptingVM* vm, uint32_t objectHandle)
{
  if (vm == nullptr)
  {
    return 0;
  }
  auto object = static_cast<ScriptedObject*>(vm->handles().resolve(
      objectHandle,
      WasmScriptingVM::HandleTable::Tag::object));
  auto instance = object != nullptr && object->dataContext() != nullptr
                      ? object->dataContext()->mainViewModelInstance()
                      : nullptr;
  if (instance == nullptr)
  {
    vm->noteMissingRequestedData();
    return 0;
  }
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::viewModelInstance,
      new HostViewModelInstance{std::move(instance)});
}

void dataVmiReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
  releaseHostHandle(
      vm,
      handle,
      WasmScriptingVM::HandleTable::Tag::viewModelInstance);
}

uint32_t mintInstanceValue(
    WasmScriptingVM* vm,
    uint32_t vmiHandle,
    const char* name,
    uint32_t length,
    uint16_t typeKey)
{
  if (vm == nullptr)
  {
    return 0;
  }
  auto host = static_cast<HostViewModelInstance*>(vm->handles().resolve(
      vmiHandle,
      WasmScriptingVM::HandleTable::Tag::viewModelInstance));
  if (host == nullptr)
  {
    return 0;
  }
  auto value = host->instance->propertyValue(std::string(name, length));
  if (value == nullptr || !value->isTypeOf(typeKey))
  {
    return 0;
  }
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::instanceValue,
      new HostInstanceValue(value));
}

template <typename T>
uint32_t mintInstanceValue(
    WasmScriptingVM* vm,
    uint32_t vmiHandle,
    const char* name,
    uint32_t length)
{
  return mintInstanceValue(vm, vmiHandle, name, length, T::typeKey);
}

ViewModel* findViewModel(WasmScriptingVM* vm, const char* name, uint32_t length)
{
  if (vm == nullptr || vm->viewModels() == nullptr)
  {
    return nullptr;
  }
  std::string key(name, length);
  for (ViewModel* viewModel : *vm->viewModels())
  {
    if (viewModel != nullptr && viewModel->name() == key)
    {
      return viewModel;
    }
  }
  return nullptr;
}

uint32_t dataHasViewModelImpl(
    WasmScriptingVM* vm,
    const char* name,
    uint32_t nameLength)
{
  return findViewModel(vm, name, nameLength) != nullptr ? 1 : 0;
}

uint32_t dataNewViewModelImpl(
    WasmScriptingVM* vm,
    const char* name,
    uint32_t nameLength,
    const char* templateName,
    uint32_t templateLength)
{
  ViewModel* viewModel = findViewModel(vm, name, nameLength);
  if (vm == nullptr || viewModel == nullptr)
  {
    return 0;
  }
  auto instance = templateLength != 0
                      ? viewModel->createFromInstance(
                            std::string(templateName, templateLength))
                      : viewModel->createInstance();
  if (instance == nullptr)
  {
    return 0;
  }
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::viewModelInstance,
      new HostViewModelInstance{std::move(instance)});
}

uint32_t dataVmiNumberImpl(
    WasmScriptingVM* vm,
    uint32_t vmiHandle,
    const char* name,
    uint32_t length)
{
  return mintInstanceValue<ViewModelInstanceNumber>(
      vm,
      vmiHandle,
      name,
      length);
}

uint32_t dataVmiBooleanImpl(
    WasmScriptingVM* vm,
    uint32_t vmiHandle,
    const char* name,
    uint32_t length)
{
  return mintInstanceValue<ViewModelInstanceBoolean>(
      vm,
      vmiHandle,
      name,
      length);
}

uint32_t dataVmiStringImpl(
    WasmScriptingVM* vm,
    uint32_t vmiHandle,
    const char* name,
    uint32_t length)
{
  return mintInstanceValue<ViewModelInstanceString>(
      vm,
      vmiHandle,
      name,
      length);
}

uint32_t dataVmiTriggerImpl(
    WasmScriptingVM* vm,
    uint32_t vmiHandle,
    const char* name,
    uint32_t length)
{
  return mintInstanceValue<ViewModelInstanceTrigger>(
      vm,
      vmiHandle,
      name,
      length);
}

uint32_t dataVmiColorImpl(
    WasmScriptingVM* vm,
    uint32_t vmiHandle,
    const char* name,
    uint32_t length)
{
  return mintInstanceValue<ViewModelInstanceColor>(vm, vmiHandle, name, length);
}

uint32_t dataVmiViewModelImpl(
    WasmScriptingVM* vm,
    uint32_t vmiHandle,
    const char* name,
    uint32_t length)
{
  return mintInstanceValue<ViewModelInstanceViewModel>(
      vm,
      vmiHandle,
      name,
      length);
}

void dataPropReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
  releaseHostHandle(
      vm,
      handle,
      WasmScriptingVM::HandleTable::Tag::instanceValue);
}

// A ref, so the value outlives a native whose change notifies listeners that
// release its last handle.
rcp<ViewModelInstanceValue> resolveInstanceValue(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint16_t typeKey)
{
  if (vm == nullptr)
  {
    return nullptr;
  }
  auto host = static_cast<HostInstanceValue*>(vm->handles().resolve(
      handle,
      WasmScriptingVM::HandleTable::Tag::instanceValue));
  if (host == nullptr || !host->value->isTypeOf(typeKey))
  {
    return nullptr;
  }
  return host->value;
}

template <typename T>
rcp<T> resolveInstanceValue(WasmScriptingVM* vm, uint32_t handle)
{
  return static_rcp_cast<T>(resolveInstanceValue(vm, handle, T::typeKey));
}

float dataNumberGetImpl(WasmScriptingVM* vm, uint32_t handle)
{
  auto number = resolveInstanceValue<ViewModelInstanceNumber>(vm, handle);
  return number != nullptr ? number->propertyValue() : 0.0f;
}

void dataNumberSetImpl(WasmScriptingVM* vm, uint32_t handle, float value)
{
  auto number = resolveInstanceValue<ViewModelInstanceNumber>(vm, handle);
  if (number != nullptr)
  {
    number->propertyValue(value);
  }
}

uint32_t dataBooleanGetImpl(WasmScriptingVM* vm, uint32_t handle)
{
  auto boolean = resolveInstanceValue<ViewModelInstanceBoolean>(vm, handle);
  return boolean != nullptr && boolean->propertyValue() ? 1 : 0;
}

void dataBooleanSetImpl(WasmScriptingVM* vm, uint32_t handle, uint32_t value)
{
  auto boolean = resolveInstanceValue<ViewModelInstanceBoolean>(vm, handle);
  if (boolean != nullptr)
  {
    boolean->propertyValue(value != 0);
  }
}

uint32_t dataStringGetImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    char* buffer,
    uint32_t capacity)
{
  auto string = resolveInstanceValue<ViewModelInstanceString>(vm, handle);
  if (string == nullptr)
  {
    return 0;
  }
  uint32_t length = fillModuleString(
      vm,
      string->propertyValue(),
      buffer,
      capacity);
  if (length <= capacity)
  {
    auto host = static_cast<HostInstanceValue*>(vm->handles().resolve(
        handle,
        WasmScriptingVM::HandleTable::Tag::instanceValue));
    host->sent = string->propertyValue();
    host->hasSent = true;
  }
  return length;
}

uint32_t dataStringChangedImpl(WasmScriptingVM* vm, uint32_t handle)
{
  auto string = resolveInstanceValue<ViewModelInstanceString>(vm, handle);
  if (string == nullptr)
  {
    return 0;
  }
  auto host = static_cast<HostInstanceValue*>(vm->handles().resolve(
      handle,
      WasmScriptingVM::HandleTable::Tag::instanceValue));
  if (host->hasSent && host->sent == string->propertyValue())
  {
    return ~0u;
  }
  return (uint32_t)fillModuleString(vm, string->propertyValue(), nullptr, 0);
}

void dataStringSetImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    const char* value,
    uint32_t length)
{
  auto string = resolveInstanceValue<ViewModelInstanceString>(vm, handle);
  if (string != nullptr)
  {
    string->propertyValue(std::string(value, length));
  }
}

void dataTriggerFireImpl(WasmScriptingVM* vm, uint32_t handle)
{
  auto trigger = resolveInstanceValue<ViewModelInstanceTrigger>(vm, handle);
  if (trigger != nullptr)
  {
    trigger->trigger();
  }
}

uint32_t dataColorGetImpl(WasmScriptingVM* vm, uint32_t handle)
{
  auto color = resolveInstanceValue<ViewModelInstanceColor>(vm, handle);
  return color != nullptr ? (uint32_t)color->propertyValue() : 0;
}

void dataColorSetImpl(WasmScriptingVM* vm, uint32_t handle, uint32_t value)
{
  auto color = resolveInstanceValue<ViewModelInstanceColor>(vm, handle);
  if (color != nullptr)
  {
    color->propertyValue((int)value);
  }
}

uint32_t dataVmiEnumImpl(
    WasmScriptingVM* vm,
    uint32_t vmiHandle,
    const char* name,
    uint32_t length)
{
  return mintInstanceValue<ViewModelInstanceEnum>(vm, vmiHandle, name, length);
}

uint32_t dataEnumGetImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    char* buffer,
    uint32_t capacity)
{
  auto value = resolveInstanceValue<ViewModelInstanceEnum>(vm, handle);
  if (value == nullptr || value->viewModelProperty() == nullptr ||
      !value->viewModelProperty()->is<ViewModelPropertyEnum>())
  {
    return 0;
  }
  auto dataEnum = value->viewModelProperty()
                      ->as<ViewModelPropertyEnum>()
                      ->dataEnum();
  if (dataEnum == nullptr)
  {
    return 0;
  }
  auto values = dataEnum->values();
  uint32_t index = value->propertyValue();
  if (index >= values.size())
  {
    return 0;
  }
  return fillModuleString(vm, values[index]->key(), buffer, capacity);
}

void dataEnumSetImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    const char* value,
    uint32_t length)
{
  auto instanceValue = resolveInstanceValue<ViewModelInstanceEnum>(vm, handle);
  if (instanceValue != nullptr)
  {
    instanceValue->value(std::string(value, length));
  }
}

uint32_t dataVmiListImpl(
    WasmScriptingVM* vm,
    uint32_t vmiHandle,
    const char* name,
    uint32_t length)
{
  return mintInstanceValue<ViewModelInstanceList>(vm, vmiHandle, name, length);
}

uint32_t dataListLengthImpl(WasmScriptingVM* vm, uint32_t handle)
{
  auto list = resolveInstanceValue<ViewModelInstanceList>(vm, handle);
  return list != nullptr ? (uint32_t)list->listItems().size() : 0;
}

void dataListPushImpl(WasmScriptingVM* vm, uint32_t handle, uint32_t vmiHandle)
{
  auto list = resolveInstanceValue<ViewModelInstanceList>(vm, handle);
  if (vm == nullptr || list == nullptr)
  {
    return;
  }
  auto host = static_cast<HostViewModelInstance*>(vm->handles().resolve(
      vmiHandle,
      WasmScriptingVM::HandleTable::Tag::viewModelInstance));
  if (host == nullptr)
  {
    return;
  }
  auto item = make_rcp<ViewModelInstanceListItem>();
  item->viewModelInstance(host->instance);
  list->addItem(std::move(item));
}

uint32_t mintRemovedItem(
    WasmScriptingVM* vm,
    rcp<ViewModelInstanceListItem> item)
{
  if (vm == nullptr || item == nullptr || item->viewModelInstance() == nullptr)
  {
    return 0;
  }
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::viewModelInstance,
      new HostViewModelInstance{item->viewModelInstance()});
}

uint32_t dataListPopImpl(WasmScriptingVM* vm, uint32_t handle)
{
  auto list = resolveInstanceValue<ViewModelInstanceList>(vm, handle);
  return list != nullptr ? mintRemovedItem(vm, list->pop()) : 0;
}

uint32_t dataListShiftImpl(WasmScriptingVM* vm, uint32_t handle)
{
  auto list = resolveInstanceValue<ViewModelInstanceList>(vm, handle);
  return list != nullptr ? mintRemovedItem(vm, list->shift()) : 0;
}

void dataListClearImpl(WasmScriptingVM* vm, uint32_t handle)
{
  auto list = resolveInstanceValue<ViewModelInstanceList>(vm, handle);
  if (list != nullptr)
  {
    list->removeAllItems();
  }
}

void dataListSwapImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint32_t index1,
    uint32_t index2)
{
  auto list = resolveInstanceValue<ViewModelInstanceList>(vm, handle);
  if (list != nullptr)
  {
    list->swap(index1, index2);
  }
}

rcp<ViewModelInstance> resolveVmiArg(WasmScriptingVM* vm, uint32_t vmiHandle)
{
  if (vm == nullptr)
  {
    return nullptr;
  }
  auto host = static_cast<HostViewModelInstance*>(vm->handles().resolve(
      vmiHandle,
      WasmScriptingVM::HandleTable::Tag::viewModelInstance));
  return host != nullptr ? host->instance : nullptr;
}

void dataListInsertImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint32_t vmiHandle,
    uint32_t index)
{
  auto list = resolveInstanceValue<ViewModelInstanceList>(vm, handle);
  auto instance = resolveVmiArg(vm, vmiHandle);
  if (list == nullptr || instance == nullptr)
  {
    return;
  }
  auto item = make_rcp<ViewModelInstanceListItem>();
  item->viewModelInstance(std::move(instance));
  list->addItemAt(std::move(item), (int)index);
}

void dataListRemoveImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint32_t vmiHandle)
{
  auto list = resolveInstanceValue<ViewModelInstanceList>(vm, handle);
  auto instance = resolveVmiArg(vm, vmiHandle);
  if (list == nullptr || instance == nullptr)
  {
    return;
  }
  for (const auto& item : list->listItems())
  {
    if (item->viewModelInstance() == instance)
    {
      list->removeItem(item);
      break;
    }
  }
}

void dataListRemoveAtImpl(WasmScriptingVM* vm, uint32_t handle, uint32_t index)
{
  auto list = resolveInstanceValue<ViewModelInstanceList>(vm, handle);
  if (list != nullptr)
  {
    list->removeItem((int)index);
  }
}

void dataViewModelSetImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint32_t vmiHandle)
{
  auto value = resolveInstanceValue<ViewModelInstanceViewModel>(vm, handle);
  auto instance = resolveVmiArg(vm, vmiHandle);
  if (value == nullptr || instance == nullptr ||
      value->parentViewModelInstance() == nullptr)
  {
    return;
  }
  // The swap notifies, and a listener can release the parent's last handle.
  auto parent = ref_rcp(value->parentViewModelInstance());
  parent->replaceViewModelByProperty(value.get(), std::move(instance));
}

uint32_t dataViewModelGetImpl(WasmScriptingVM* vm, uint32_t handle)
{
  auto value = resolveInstanceValue<ViewModelInstanceViewModel>(vm, handle);
  if (vm == nullptr || value == nullptr)
  {
    return 0;
  }
  auto reference = value->referenceViewModelInstance();
  if (reference == nullptr)
  {
    return 0;
  }
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::viewModelInstance,
      new HostViewModelInstance{std::move(reference)});
}

uint32_t dataVmiPropertyImpl(
    WasmScriptingVM* vm,
    uint32_t vmiHandle,
    const char* name,
    uint32_t length,
    uint32_t* kindOut,
    uint32_t kindCount)
{
  if (kindCount > 0)
  {
    kindOut[0] = DataPropertyWire::kindNone;
  }
  if (vm == nullptr)
  {
    return 0;
  }
  auto host = static_cast<HostViewModelInstance*>(vm->handles().resolve(
      vmiHandle,
      WasmScriptingVM::HandleTable::Tag::viewModelInstance));
  if (host == nullptr)
  {
    return 0;
  }
  auto value = host->instance->propertyValue(std::string(name, length));
  if (value == nullptr)
  {
    return 0;
  }
  uint32_t kind = DataPropertyWire::kindNone;
  switch (value->coreType())
  {
    case ViewModelInstanceNumberBase::typeKey:
      kind = DataPropertyWire::kindNumber;
      break;
    case ViewModelInstanceBooleanBase::typeKey:
      kind = DataPropertyWire::kindBoolean;
      break;
    case ViewModelInstanceStringBase::typeKey:
      kind = DataPropertyWire::kindString;
      break;
    case ViewModelInstanceTriggerBase::typeKey:
      kind = DataPropertyWire::kindTrigger;
      break;
    case ViewModelInstanceColorBase::typeKey:
      kind = DataPropertyWire::kindColor;
      break;
    case ViewModelInstanceViewModelBase::typeKey:
      kind = DataPropertyWire::kindViewModel;
      break;
    case ViewModelInstanceListBase::typeKey:
      kind = DataPropertyWire::kindList;
      break;
    case ViewModelInstanceEnumBase::typeKey:
      kind = DataPropertyWire::kindEnum;
      break;
    case ViewModelInstanceAssetImageBase::typeKey:
      kind = DataPropertyWire::kindImage;
      break;
    case ViewModelInstanceAssetFontBase::typeKey:
      kind = DataPropertyWire::kindFont;
      break;
    case ViewModelInstanceAssetBlobBase::typeKey:
      kind = DataPropertyWire::kindBlob;
      break;
    case ViewModelInstanceSymbolListIndexBase::typeKey:
      if (kindCount > 0)
      {
        kindOut[0] = DataPropertyWire::kindSymbolListIndex;
      }
      if (kindCount > 1)
      {
        kindOut[1] = (uint32_t)value->as<ViewModelInstanceSymbolListIndex>()
                         ->propertyValue();
      }
      return 0;
    default:
      return 0;
  }
  if (kindCount > 0)
  {
    kindOut[0] = kind;
  }
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::instanceValue,
      new HostInstanceValue(value));
}

uint32_t dataVmiInstanceImpl(
    WasmScriptingVM* vm,
    uint32_t vmiHandle,
    const char* name,
    uint32_t nameLength)
{
  if (vm == nullptr)
  {
    return 0;
  }
  auto host = static_cast<HostViewModelInstance*>(vm->handles().resolve(
      vmiHandle,
      WasmScriptingVM::HandleTable::Tag::viewModelInstance));
  if (host == nullptr || host->instance->viewModel() == nullptr)
  {
    return 0;
  }
  ViewModel* viewModel = host->instance->viewModel();
  auto instance = nameLength != 0 ? viewModel->createFromInstance(
                                        std::string(name, nameLength))
                                  : nullptr;
  if (instance == nullptr)
  {
    instance = viewModel->createInstance();
  }
  if (instance == nullptr)
  {
    return 0;
  }
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::viewModelInstance,
      new HostViewModelInstance{std::move(instance)});
}

uint32_t dataVmiSymbolIndexImpl(WasmScriptingVM* vm, uint32_t vmiHandle)
{
  if (vm == nullptr)
  {
    return ~0u;
  }
  auto host = static_cast<HostViewModelInstance*>(vm->handles().resolve(
      vmiHandle,
      WasmScriptingVM::HandleTable::Tag::viewModelInstance));
  if (host == nullptr)
  {
    return ~0u;
  }
  auto prop = host->instance->propertyValue(SymbolType::itemIndex);
  if (prop != nullptr && prop->is<ViewModelInstanceSymbolListIndex>())
  {
    return (uint32_t)prop->as<ViewModelInstanceSymbolListIndex>()
        ->propertyValue();
  }
  return ~0u;
}

uint32_t dataVmiEqualImpl(WasmScriptingVM* vm, uint32_t a, uint32_t b)
{
  auto lhs = resolveVmiArg(vm, a);
  auto rhs = resolveVmiArg(vm, b);
  return lhs != nullptr && lhs == rhs ? 1 : 0;
}

void dataListRemoveAllOfImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint32_t vmiHandle)
{
  auto list = resolveInstanceValue<ViewModelInstanceList>(vm, handle);
  auto instance = resolveVmiArg(vm, vmiHandle);
  if (list != nullptr && instance != nullptr)
  {
    list->removeAllItemsWithViewModelInstance(instance.get());
  }
}

uint32_t dataListGetImpl(WasmScriptingVM* vm, uint32_t handle, uint32_t index)
{
  auto list = resolveInstanceValue<ViewModelInstanceList>(vm, handle);
  if (list == nullptr || index >= list->listItems().size())
  {
    return 0;
  }
  return mintRemovedItem(vm, list->listItems()[index]);
}

uint32_t dataEnumValuesImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    char* buffer,
    uint32_t capacity)
{
  auto value = resolveInstanceValue<ViewModelInstanceEnum>(vm, handle);
  if (value == nullptr || value->viewModelProperty() == nullptr ||
      !value->viewModelProperty()->is<ViewModelPropertyEnum>())
  {
    return 0;
  }
  auto dataEnum = value->viewModelProperty()
                      ->as<ViewModelPropertyEnum>()
                      ->dataEnum();
  if (dataEnum == nullptr)
  {
    return 0;
  }
  std::string joined;
  for (auto& entry : dataEnum->values())
  {
    if (!joined.empty())
    {
      joined += '\n';
    }
    joined += entry->key();
  }
  return fillModuleString(vm, joined, buffer, capacity);
}

uint32_t dataRootViewModelImpl(WasmScriptingVM* vm, uint32_t objectHandle)
{
  if (vm == nullptr)
  {
    return 0;
  }
  auto object = static_cast<ScriptedObject*>(vm->handles().resolve(
      objectHandle,
      WasmScriptingVM::HandleTable::Tag::object));
  auto instance = object != nullptr && object->dataContext() != nullptr
                      ? object->dataContext()->rootViewModelInstance()
                      : nullptr;
  if (instance == nullptr)
  {
    vm->noteMissingRequestedData();
    return 0;
  }
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::viewModelInstance,
      new HostViewModelInstance{std::move(instance)});
}

uint32_t dataGlobalViewModelImpl(
    WasmScriptingVM* vm,
    uint32_t objectHandle,
    const char* name,
    uint32_t nameLength)
{
  if (vm == nullptr)
  {
    return 0;
  }
  auto object = static_cast<ScriptedObject*>(vm->handles().resolve(
      objectHandle,
      WasmScriptingVM::HandleTable::Tag::object));
  if (object == nullptr || object->scriptAsset() == nullptr ||
      object->scriptAsset()->file() == nullptr ||
      object->dataContext() == nullptr)
  {
    return 0;
  }
  std::string key(name, nameLength);
  auto instance = object->dataContext()->resolveGlobalViewModel(
      object->scriptAsset()->file(),
      key.c_str());
  if (instance == nullptr)
  {
    return 0;
  }
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::viewModelInstance,
      new HostViewModelInstance{std::move(instance)});
}

uint32_t dataGlobalViewModelNamesImpl(
    WasmScriptingVM* vm,
    uint32_t objectHandle,
    char* buffer,
    uint32_t capacity)
{
  if (vm == nullptr)
  {
    return 0;
  }
  auto object = static_cast<ScriptedObject*>(vm->handles().resolve(
      objectHandle,
      WasmScriptingVM::HandleTable::Tag::object));
  if (object == nullptr || object->scriptAsset() == nullptr ||
      object->scriptAsset()->file() == nullptr)
  {
    return 0;
  }
  std::string joined;
  for (auto& name : object->scriptAsset()->file()->globalViewModelNames())
  {
    if (!joined.empty())
    {
      joined += '\n';
    }
    joined += name;
  }
  return fillModuleString(vm, joined, buffer, capacity);
}

struct HostDataContext
{
  rcp<DataContext> context;
};

uint32_t dataContextImpl(WasmScriptingVM* vm, uint32_t objectHandle)
{
  if (vm == nullptr)
  {
    return 0;
  }
  auto object = static_cast<ScriptedObject*>(vm->handles().resolve(
      objectHandle,
      WasmScriptingVM::HandleTable::Tag::object));
  if (object == nullptr || object->dataContext() == nullptr)
  {
    vm->noteMissingRequestedData();
    return 0;
  }
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::dataContext,
      new HostDataContext{object->dataContext()});
}

uint32_t dataContextParentImpl(WasmScriptingVM* vm, uint32_t handle)
{
  if (vm == nullptr)
  {
    return 0;
  }
  auto host = static_cast<HostDataContext*>(vm->handles().resolve(
      handle,
      WasmScriptingVM::HandleTable::Tag::dataContext));
  if (host == nullptr || host->context->parent() == nullptr)
  {
    return 0;
  }
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::dataContext,
      new HostDataContext{host->context->parent()});
}

uint32_t dataContextViewModelImpl(WasmScriptingVM* vm, uint32_t handle)
{
  if (vm == nullptr)
  {
    return 0;
  }
  auto host = static_cast<HostDataContext*>(vm->handles().resolve(
      handle,
      WasmScriptingVM::HandleTable::Tag::dataContext));
  if (host == nullptr)
  {
    return 0;
  }
  auto instance = host->context->mainViewModelInstance();
  if (instance == nullptr)
  {
    return 0;
  }
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::viewModelInstance,
      new HostViewModelInstance{std::move(instance)});
}

void dataContextReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
  releaseHostHandle(vm, handle, WasmScriptingVM::HandleTable::Tag::dataContext);
}

// --- rive_artboard_v1: host-owned artboard inputs ---------------------------

// Mirrors ScriptReffedArtboard: the host owns the instance, its default state
// machine, and the bound view model instance; module userdata share it by ref.
struct HostArtboard : public RefCnt<HostArtboard>
{
  File* file = nullptr;
  std::unique_ptr<ArtboardInstance> artboard;
  std::unique_ptr<StateMachineInstance> stateMachine;
  rcp<ViewModelInstance> viewModelInstance;
  rcp<DataContext> parentDataContext;

  HostArtboard(
      File* fileValue,
      std::unique_ptr<ArtboardInstance>&& artboardInstance,
      rcp<ViewModelInstance> boundInstance,
      rcp<DataContext> parent)
      : file(fileValue)
      , artboard(std::move(artboardInstance))
      , stateMachine(artboard->defaultStateMachine())
      , parentDataContext(std::move(parent))
  {
    // A scripted artboard is a root: nothing hosts it in another
    // artboard's focus tree, so it owns its FocusManager and builds its
    // own focus tree.
    artboard->buildFocusTree(artboard->ensureFocusManager(), nullptr);
    viewModelInstance = boundInstance != nullptr
                            ? std::move(boundInstance)
                            : file->createViewModelInstance(artboard.get());
    if (stateMachine != nullptr && viewModelInstance != nullptr)
    {
      if (parentDataContext != nullptr)
      {
        auto dataContext = make_rcp<DataContext>(viewModelInstance);
        dataContext->parent(parentDataContext);
        stateMachine->bindDataContext(dataContext);
      }
      else
      {
        stateMachine->bindViewModelInstance(viewModelInstance);
      }
    }
  }

  ~HostArtboard()
  {
    // State machine before artboard; its destructor touches the artboard.
    stateMachine = nullptr;
    artboard = nullptr;
  }

  bool advance(float seconds)
  {
    if (stateMachine != nullptr)
    {
      // Bound view models advance with the host frame, not here.
      return stateMachine->advanceAndApply(seconds, false);
    }
    return artboard->advance(seconds);
  }
};

// A ref, so the artboard outlives a native whose script callbacks release it.
rcp<HostArtboard> resolveArtboard(WasmScriptingVM* vm, uint32_t handle)
{
  if (vm == nullptr)
  {
    return nullptr;
  }
  return ref_rcp(static_cast<HostArtboard*>(vm->handles().resolve(
      handle,
      WasmScriptingVM::HandleTable::Tag::artboard)));
}

void artboardReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
  auto host = resolveArtboard(vm, handle);
  if (vm == nullptr || host == nullptr)
  {
    return;
  }
  vm->handles().release(handle, WasmScriptingVM::HandleTable::Tag::artboard);
  host->unref();
}

uint32_t artboardAdvanceImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    float seconds)
{
  auto host = resolveArtboard(vm, handle);
  return host != nullptr && host->advance(seconds) ? 1 : 0;
}

void artboardDrawImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint32_t rendererHandle)
{
  auto host = resolveArtboard(vm, handle);
  auto renderer = resolveRenderer(vm, rendererHandle);
  if (host != nullptr && renderer != nullptr)
  {
#ifdef WITH_RIVE_TOOLS
    host->artboard->settleIfImagesRevealed();
#endif
    host->artboard->drawInternal(renderer);
  }
}

namespace
{
struct WasmDrawVisit
{
  WasmScriptingVM* vm;
  // Resolved around each visit, which may end the canvas frame behind it.
  uint32_t renderer;
  bool failed = false;
};

void wasmDrawVisitor(void* context, Drawable* drawable, Renderer* renderer)
{
  auto visit = static_cast<WasmDrawVisit*>(context);
  if (visit->failed)
  {
    drawable->draw(renderer);
    return;
  }
  auto& handles = visit->vm->handles();
  uint32_t handle = handles.mint(
      WasmScriptingVM::HandleTable::Tag::drawable,
      drawable);
  // The host has the artboard's own saves open here, so each visit gets
  // a scope of its own and leaves the renderer as it found it.
  HostRenderer* host = resolveHostRenderer(visit->vm, visit->renderer);
  int outer = host != nullptr ? host->saves : 0;
  if (host != nullptr)
  {
    host->saves = 0;
  }
  visit->failed = !visit->vm->notifyDrawVisit(handle);
  host = resolveHostRenderer(visit->vm, visit->renderer);
  if (host != nullptr)
  {
    host->restoreSaves();
    host->saves = outer;
  }
  handles.release(handle, WasmScriptingVM::HandleTable::Tag::drawable);
}

Drawable* resolveDrawable(WasmScriptingVM* vm, uint32_t handle)
{
  return vm == nullptr ? nullptr
                       : static_cast<Drawable*>(vm->handles().resolve(
                             handle,
                             WasmScriptingVM::HandleTable::Tag::drawable));
}

CustomProperty* resolveDrawableProperty(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint32_t key)
{
  auto drawable = resolveDrawable(vm, handle);
  return drawable != nullptr ? drawable->customProperty(key) : nullptr;
}
} // namespace

void artboardDrawVisitImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint32_t rendererHandle)
{
  auto host = resolveArtboard(vm, handle);
  auto renderer = resolveHostRenderer(vm, rendererHandle);
  if (host == nullptr || renderer == nullptr)
  {
    return;
  }
  WasmDrawVisit visit = {vm, rendererHandle};
#ifdef WITH_RIVE_TOOLS
  host->artboard->settleIfImagesRevealed();
#endif
  host->artboard
      ->drawInternal(renderer->renderer, wasmDrawVisitor, &visit, host->file);
}

void artboardDrawModulatedImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint32_t rendererHandle,
    uint32_t key)
{
  auto host = resolveArtboard(vm, handle);
  auto renderer = resolveRenderer(vm, rendererHandle);
  if (host != nullptr && renderer != nullptr)
  {
#ifdef WITH_RIVE_TOOLS
    host->artboard->settleIfImagesRevealed();
#endif
    host->artboard->drawModulated(renderer, key, host->file);
  }
}

void artboardDrawableDrawImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint32_t rendererHandle)
{
  auto drawable = resolveDrawable(vm, handle);
  auto renderer = resolveRenderer(vm, rendererHandle);
  if (drawable != nullptr && renderer != nullptr)
  {
    drawable->draw(renderer);
  }
}

uint32_t artboardPropertyKeyImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    const char* name,
    uint32_t length)
{
  auto host = resolveArtboard(vm, handle);
  return File::customPropertyKey(
      host != nullptr ? host->file : nullptr,
      name,
      length);
}

uint32_t artboardDrawableValueImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint32_t key,
    uint32_t* out,
    uint32_t outCount)
{
  auto property = resolveDrawableProperty(vm, handle, key);
  if (property == nullptr)
  {
    return 0;
  }
  CustomPropertyKind kind = property->kind();
  uint32_t bits = 0;
  switch (kind)
  {
    case CustomPropertyKind::number:
    {
      float value = property->as<CustomPropertyNumber>()->propertyValue();
      memcpy(&bits, &value, sizeof(bits));
      break;
    }
    case CustomPropertyKind::boolean:
      bits = property->as<CustomPropertyBoolean>()->propertyValue();
      break;
    case CustomPropertyKind::color:
      bits = property->as<CustomPropertyColor>()->propertyValue();
      break;
    default:
      // No reader takes the others' bits, the kind answers has().
      break;
  }
  if (outCount > 0)
  {
    out[0] = bits;
  }
  return (uint32_t)kind + 1;
}

uint32_t artboardDrawableStringImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint32_t key,
    char* out,
    uint32_t outCount)
{
  auto property = resolveDrawableProperty(vm, handle, key);
  if (property == nullptr || !property->is<CustomPropertyString>())
  {
    return ~0u;
  }
  return fillModuleString(
      vm,
      property->as<CustomPropertyString>()->propertyValue(),
      out,
      outCount);
}

#ifdef WITH_RIVE_TOOLS
uint32_t artboardDrawablePropertiesImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    char* out,
    uint32_t outCount)
{
  auto drawable = resolveDrawable(vm, handle);
  auto file = drawable != nullptr ? drawable->artboard()->drawVisitorFile()
                                  : nullptr;
  auto manifest = file != nullptr ? file->manifest() : nullptr;
  if (manifest == nullptr)
  {
    return 0;
  }
  std::string packed;
  for (auto child : drawable->children())
  {
    auto property = CustomProperty::tagging(child);
    if (property == nullptr)
    {
      continue;
    }
    packed.push_back((char)property->kind());
    packed.append(manifest->resolveName(property->nameId()));
    packed.push_back('\0');
  }
  memcpy(out, packed.data(), std::min<size_t>(packed.size(), outCount));
  return (uint32_t)packed.size();
}
#endif

uint32_t artboardInstanceImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint32_t vmiHandle)
{
  auto host = resolveArtboard(vm, handle);
  if (vm == nullptr || host == nullptr)
  {
    return 0;
  }
  auto clone = host->artboard->instance();
  clone->frameOrigin(false);
  auto vmi = vmiHandle != 0 ? resolveVmiArg(vm, vmiHandle) : nullptr;
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::artboard,
      new HostArtboard(
          host->file,
          std::move(clone),
          std::move(vmi),
          host->parentDataContext));
}

uint32_t artboardDataImpl(WasmScriptingVM* vm, uint32_t handle)
{
  auto host = resolveArtboard(vm, handle);
  if (vm == nullptr || host == nullptr || host->viewModelInstance == nullptr)
  {
    return 0;
  }
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::viewModelInstance,
      new HostViewModelInstance{host->viewModelInstance});
}

float artboardWidthImpl(WasmScriptingVM* vm, uint32_t handle)
{
  auto host = resolveArtboard(vm, handle);
  return host != nullptr ? host->artboard->width() : 0.0f;
}

float artboardHeightImpl(WasmScriptingVM* vm, uint32_t handle)
{
  auto host = resolveArtboard(vm, handle);
  return host != nullptr ? host->artboard->height() : 0.0f;
}

void artboardSetWidthImpl(WasmScriptingVM* vm, uint32_t handle, float value)
{
  auto host = resolveArtboard(vm, handle);
  if (host != nullptr)
  {
    host->artboard->width(value);
  }
}

void artboardSetHeightImpl(WasmScriptingVM* vm, uint32_t handle, float value)
{
  auto host = resolveArtboard(vm, handle);
  if (host != nullptr)
  {
    host->artboard->height(value);
  }
}

uint32_t artboardFrameOriginImpl(WasmScriptingVM* vm, uint32_t handle)
{
  auto host = resolveArtboard(vm, handle);
  return host != nullptr && host->artboard->frameOrigin() ? 1 : 0;
}

void artboardSetFrameOriginImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint32_t value)
{
  auto host = resolveArtboard(vm, handle);
  if (host != nullptr)
  {
    host->artboard->frameOrigin(value != 0);
  }
}

void artboardBoundsImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    float* out,
    uint32_t outCount)
{
  auto host = resolveArtboard(vm, handle);
  if (host == nullptr || outCount < 4)
  {
    return;
  }
  const AABB& bounds = host->artboard->bounds();
  out[0] = bounds.min().x;
  out[1] = bounds.min().y;
  out[2] = bounds.max().x;
  out[3] = bounds.max().y;
}

uint32_t artboardPointerEventImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint32_t kind,
    uint32_t pointerId,
    float x,
    float y)
{
  auto host = resolveArtboard(vm, handle);
  if (host == nullptr || host->stateMachine == nullptr)
  {
    return 0;
  }
  Vec2D position(x, y);
  switch (kind)
  {
    case ArtboardWire::pointerDown:
      return (uint32_t)(int)host->stateMachine->pointerDown(
          position,
          (uint8_t)pointerId);
    case ArtboardWire::pointerMove:
      return (uint32_t)(int)
          host->stateMachine->pointerMove(position, 0, (uint8_t)pointerId);
    case ArtboardWire::pointerUp:
      return (uint32_t)(int)host->stateMachine->pointerUp(
          position,
          (uint8_t)pointerId);
    case ArtboardWire::pointerExit:
      return (uint32_t)(int)host->stateMachine->pointerExit(
          position,
          (uint8_t)pointerId);
  }
  return 0;
}

uint32_t artboardScrollEventImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint32_t pointerId,
    float x,
    float y,
    float dx,
    float dy,
    uint32_t phase,
    uint32_t precise,
    float timeStamp)
{
  auto host = resolveArtboard(vm, handle);
  if (host == nullptr || host->stateMachine == nullptr ||
      phase > (uint32_t)ScrollPhase::inertiaCancel)
  {
    return 0;
  }
  ScrollEvent event{Vec2D(dx, dy), (ScrollPhase)phase, precise != 0};
  return (uint32_t)(int)host->stateMachine
      ->pointerScroll(Vec2D(x, y), event, timeStamp, (int)pointerId);
}

// Animations pin their artboard so the instance they play into outlives them.
struct HostAnimation
{
  rcp<HostArtboard> owner;
  std::unique_ptr<LinearAnimationInstance> animation;

  float duration() const
  {
    return (float)animation->duration() / (float)animation->fps();
  }
};

uint32_t artboardAnimationImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    const char* name,
    uint32_t length)
{
  auto host = resolveArtboard(vm, handle);
  if (vm == nullptr || host == nullptr)
  {
    return 0;
  }
  auto animation = host->artboard->animationNamed(std::string(name, length));
  if (animation == nullptr)
  {
    return 0;
  }
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::animation,
      new HostAnimation{host, std::move(animation)});
}

HostAnimation* resolveAnimation(WasmScriptingVM* vm, uint32_t handle)
{
  if (vm == nullptr)
  {
    return nullptr;
  }
  return static_cast<HostAnimation*>(vm->handles().resolve(
      handle,
      WasmScriptingVM::HandleTable::Tag::animation));
}

void artboardAnimationReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
  auto host = resolveAnimation(vm, handle);
  if (vm == nullptr || host == nullptr)
  {
    return;
  }
  vm->handles().release(handle, WasmScriptingVM::HandleTable::Tag::animation);
  delete host;
}

float artboardAnimationDurationImpl(WasmScriptingVM* vm, uint32_t handle)
{
  auto host = resolveAnimation(vm, handle);
  return host != nullptr ? host->duration() : 0.0f;
}

uint32_t artboardAnimationAdvanceImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    float seconds)
{
  auto host = resolveAnimation(vm, handle);
  if (host == nullptr)
  {
    return 0;
  }
  bool advanced = host->animation->advance(seconds);
  host->animation->apply();
  return advanced ? 1 : 0;
}

void artboardAnimationSetTimeImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    float value,
    uint32_t mode)
{
  auto host = resolveAnimation(vm, handle);
  if (host == nullptr)
  {
    return;
  }
  float seconds = value;
  if (mode == ArtboardWire::timeFrames)
  {
    seconds = value / (float)host->animation->fps();
  }
  else if (mode == ArtboardWire::timePercentage)
  {
    seconds = value * host->duration();
  }
  host->animation->time(
      host->animation->animation()->globalToLocalSeconds(seconds));
  host->animation->apply();
}

// Nodes pin their artboard; the component pointer lives inside its instance.
// Without an owner the node borrows a path effect's shape for that update.
struct HostNode
{
  rcp<HostArtboard> owner;
  TransformComponent* component;
};

uint32_t mintNode(
    WasmScriptingVM* vm,
    rcp<HostArtboard> owner,
    TransformComponent* component)
{
  bool borrowed = owner == nullptr;
  uint32_t handle = vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::node,
      new HostNode{std::move(owner), component});
  if (borrowed)
  {
    vm->scopedNodes().push_back(handle);
  }
  return handle;
}

uint32_t artboardNodeImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    const char* name,
    uint32_t length)
{
  auto host = resolveArtboard(vm, handle);
  if (vm == nullptr || host == nullptr)
  {
    return 0;
  }
  auto component = host->artboard->find<TransformComponent>(
      std::string(name, length));
  if (component == nullptr)
  {
    return 0;
  }
  return mintNode(vm, host, component);
}

void artboardAddToPathImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint32_t pathHandle,
    float xx,
    float xy,
    float yx,
    float yy,
    float tx,
    float ty)
{
  auto host = resolveArtboard(vm, handle);
  auto hostPath = resolvePath(vm, pathHandle);
  if (host == nullptr || hostPath == nullptr)
  {
    return;
  }
  Mat2D transform(xx, xy, yx, yy, tx, ty);
  host->artboard->addToRawPath(hostPath->raw, &transform);
  hostPath->renderDirty = true;
}

uint32_t artboardGamepadEventImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    const uint8_t* payload,
    uint32_t byteCount)
{
  auto host = resolveArtboard(vm, handle);
  GamepadWire wire;
  if (host == nullptr || host->stateMachine == nullptr ||
      !decodeGamepadWire(payload, byteCount, wire))
  {
    return 0;
  }
  auto invocation = decodeGamepadInvocation(wire, payload);
  if (!invocation)
  {
    return 0;
  }
  // The Luau artboard:gamepad* path: focus first, then every scripted
  // drawable that did not already take it.
  ScriptedDrawable* dispatched = nullptr;
  if (auto* focus = host->stateMachine->focusManager())
  {
    (void)focus->gamepadDispatch(*invocation, &dispatched);
  }
  return (uint32_t)host->stateMachine->broadcastGamepadToScriptedDrawables(
      *invocation,
      dispatched);
}

HostNode* resolveNode(WasmScriptingVM* vm, uint32_t handle)
{
  if (vm == nullptr)
  {
    return nullptr;
  }
  return static_cast<HostNode*>(
      vm->handles().resolve(handle, WasmScriptingVM::HandleTable::Tag::node));
}

void artboardNodeReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
  auto host = resolveNode(vm, handle);
  // A borrowed node is released when its path effect update returns.
  if (host == nullptr || host->owner == nullptr)
  {
    return;
  }
  vm->handles().release(handle, WasmScriptingVM::HandleTable::Tag::node);
  delete host;
}

void artboardNodeTransformImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    float* out,
    uint32_t outCount)
{
  if (outCount < 5)
  {
    return;
  }
  auto host = resolveNode(vm, handle);
  // A stale node reads as the identity, the same on every lane.
  if (host == nullptr)
  {
    const float identity[5] = {0, 0, 0, 1, 1};
    memcpy(out, identity, sizeof(identity));
    return;
  }
  out[0] = host->component->x();
  out[1] = host->component->y();
  out[2] = host->component->rotation();
  out[3] = host->component->scaleX();
  out[4] = host->component->scaleY();
}

// x/y writes only land on Node and RootBone, like the Luau lane.
void nodeSetX(TransformComponent* component, float value)
{
  if (component->is<Node>())
  {
    component->as<Node>()->x(value);
  }
  else if (component->is<RootBone>())
  {
    component->as<RootBone>()->x(value);
  }
}

void nodeSetY(TransformComponent* component, float value)
{
  if (component->is<Node>())
  {
    component->as<Node>()->y(value);
  }
  else if (component->is<RootBone>())
  {
    component->as<RootBone>()->y(value);
  }
}

void artboardNodeSetImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint32_t field,
    float v0,
    float v1)
{
  auto host = resolveNode(vm, handle);
  if (host == nullptr)
  {
    return;
  }
  TransformComponent* component = host->component;
  switch (field)
  {
    case ArtboardWire::nodeX:
      nodeSetX(component, v0);
      break;
    case ArtboardWire::nodeY:
      nodeSetY(component, v0);
      break;
    case ArtboardWire::nodeRotation:
      component->rotation(v0);
      break;
    case ArtboardWire::nodeScaleX:
      component->scaleX(v0);
      break;
    case ArtboardWire::nodeScaleY:
      component->scaleY(v0);
      break;
    case ArtboardWire::nodePosition:
      nodeSetX(component, v0);
      nodeSetY(component, v1);
      break;
    case ArtboardWire::nodeScale:
      component->scaleX(v0);
      component->scaleY(v1);
      break;
  }
}

void artboardNodeWorldTransformImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    float* out,
    uint32_t outCount)
{
  if (outCount < 6)
  {
    return;
  }
  auto host = resolveNode(vm, handle);
  Mat2D world = host != nullptr ? host->component->worldTransform() : Mat2D();
  for (int i = 0; i < 6; i++)
  {
    out[i] = world[i];
  }
}

void artboardNodeSetWorldTransformImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    const float* values,
    uint32_t floatCount)
{
  auto host = resolveNode(vm, handle);
  if (host == nullptr || floatCount < 6)
  {
    return;
  }
  host->component->mutableWorldTransform() = Mat2D(
      values[0],
      values[1],
      values[2],
      values[3],
      values[4],
      values[5]);
}

void artboardNodeDecomposeImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    const float* values,
    uint32_t floatCount)
{
  auto host = resolveNode(vm, handle);
  if (host == nullptr || floatCount < 6)
  {
    return;
  }
  TransformComponent* component = host->component;
  Mat2D world = getParentWorld(*component).invertOrIdentity() * Mat2D(
                                                                    values[0],
                                                                    values[1],
                                                                    values[2],
                                                                    values[3],
                                                                    values[4],
                                                                    values[5]);
  TransformComponents components = world.decompose();
  nodeSetX(component, components.x());
  nodeSetY(component, components.y());
  component->scaleX(components.scaleX());
  component->scaleY(components.scaleY());
  component->rotation(components.rotation());
}

uint32_t artboardNodePathVerbsImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint8_t* out,
    uint32_t outCount)
{
  auto host = resolveNode(vm, handle);
  if (host == nullptr || !host->component->is<Path>())
  {
    return ~0u;
  }
  return fillOut(
      pathVerbBytes(host->component->as<Path>()->rawPath()),
      out,
      outCount);
}

uint32_t artboardNodePathPointsImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    float* out,
    uint32_t outCount)
{
  auto host = resolveNode(vm, handle);
  if (host == nullptr || !host->component->is<Path>())
  {
    return ~0u;
  }
  return fillOut(
      pathFloats(host->component->as<Path>()->rawPath()),
      out,
      outCount);
}

uint32_t artboardNodePaintImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint32_t* out,
    uint32_t outCount)
{
  auto host = resolveNode(vm, handle);
  if (host == nullptr || !host->component->is<ShapePaint>() || outCount < 7)
  {
    return 0;
  }
  // The same snapshot ScriptedPaintData(const ShapePaint*) takes.
  auto shapePaint = host->component->as<ShapePaint>();
  uint32_t style = (uint32_t)RenderPaintStyle::fill;
  uint32_t join = (uint32_t)StrokeJoin::miter;
  uint32_t cap = (uint32_t)StrokeCap::butt;
  float thickness = 1.0f;
  float featherStrength = 0.0f;
  uint32_t color = 0xFF000000;
  if (shapePaint->is<Stroke>())
  {
    auto stroke = shapePaint->as<Stroke>();
    style = (uint32_t)RenderPaintStyle::stroke;
    thickness = stroke->thickness();
    cap = stroke->cap();
    join = stroke->join();
  }
  for (auto& child : shapePaint->children())
  {
    if (child->is<SolidColor>())
    {
      color = child->as<SolidColor>()->colorValue();
      break;
    }
  }
  if (shapePaint->feather() != nullptr)
  {
    featherStrength = shapePaint->feather()->strength();
  }
  out[0] = style;
  out[1] = join;
  out[2] = cap;
  out[3] = (uint32_t)shapePaint->blendModeValue();
  out[4] = color;
  memcpy(&out[5], &thickness, sizeof(float));
  memcpy(&out[6], &featherStrength, sizeof(float));
  return 1;
}

uint32_t artboardNodeChildrenImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint32_t* out,
    uint32_t outCount)
{
  auto host = resolveNode(vm, handle);
  if (host == nullptr || !host->component->is<ContainerComponent>())
  {
    return 0;
  }
  auto& children = host->component->as<ContainerComponent>()->children();
  uint32_t count = 0;
  for (auto child : children)
  {
    if (child->is<TransformComponent>())
    {
      count++;
    }
  }
  if (count > outCount)
  {
    return count;
  }
  uint32_t index = 0;
  for (auto child : children)
  {
    if (child->is<TransformComponent>())
    {
      out[index++] = mintNode(vm, host->owner, child->as<TransformComponent>());
    }
  }
  return count;
}

uint32_t artboardNodeParentImpl(WasmScriptingVM* vm, uint32_t handle)
{
  auto host = resolveNode(vm, handle);
  if (host == nullptr)
  {
    return 0;
  }
  auto parent = host->component->parent();
  if (parent == nullptr || !parent->is<TransformComponent>())
  {
    return 0;
  }
  return mintNode(vm, host->owner, parent->as<TransformComponent>());
}

// --- rive_data_v1 asset properties (image/font/blob) ------------------------

struct HostFont
{
  rcp<Font> font;
#ifdef WITH_RIVE_TEXT
  RawPath glyphPath;
  uint32_t glyphPathId = 0;
  bool hasGlyphPath = false;
#endif
};

uint32_t dataVmiImageImpl(
    WasmScriptingVM* vm,
    uint32_t vmiHandle,
    const char* name,
    uint32_t length)
{
  return mintInstanceValue<ViewModelInstanceAssetImage>(
      vm,
      vmiHandle,
      name,
      length);
}

uint32_t dataVmiFontImpl(
    WasmScriptingVM* vm,
    uint32_t vmiHandle,
    const char* name,
    uint32_t length)
{
  return mintInstanceValue<ViewModelInstanceAssetFont>(
      vm,
      vmiHandle,
      name,
      length);
}

uint32_t dataVmiBlobImpl(
    WasmScriptingVM* vm,
    uint32_t vmiHandle,
    const char* name,
    uint32_t length)
{
  return mintInstanceValue<ViewModelInstanceAssetBlob>(
      vm,
      vmiHandle,
      name,
      length);
}

// Mirrors ScriptedPropertyImage::pushValue: the instance's embedded asset
// first, else the id-bound asset through the file registry.
uint32_t dataImageGetImpl(WasmScriptingVM* vm, uint32_t handle)
{
  auto value = resolveInstanceValue<ViewModelInstanceAssetImage>(vm, handle);
  if (vm == nullptr || value == nullptr)
  {
    return 0;
  }
  RenderImage* renderImage = nullptr;
  if (auto asset = value->asset())
  {
    renderImage = asset->renderImage();
  }
  if (renderImage == nullptr && vm->file() != nullptr)
  {
    auto fileAsset = vm->file()->asset(value->propertyValue());
    if (fileAsset != nullptr && fileAsset->is<ImageAsset>())
    {
      renderImage = fileAsset->as<ImageAsset>()->renderImage();
    }
  }
  if (renderImage == nullptr)
  {
    return 0;
  }
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::image,
      new HostImage{ref_rcp(renderImage)});
}

void dataImageSetImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint32_t imageHandle)
{
  auto value = resolveInstanceValue<ViewModelInstanceAssetImage>(vm, handle);
  if (value == nullptr)
  {
    return;
  }
  auto host = resolveImage(vm, imageHandle);
  value->value(host != nullptr ? host->image.get() : nullptr);
}

uint32_t dataFontGetImpl(WasmScriptingVM* vm, uint32_t handle)
{
  auto value = resolveInstanceValue<ViewModelInstanceAssetFont>(vm, handle);
  if (vm == nullptr || value == nullptr)
  {
    return 0;
  }
  rcp<Font> font;
  if (auto asset = value->asset())
  {
    font = asset->font();
  }
  if (font == nullptr && vm->file() != nullptr)
  {
    auto fileAsset = vm->file()->asset(value->propertyValue());
    if (fileAsset != nullptr && fileAsset->is<FontAsset>())
    {
      font = fileAsset->as<FontAsset>()->font();
    }
  }
  if (font == nullptr)
  {
    return 0;
  }
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::font,
      new HostFont{std::move(font)});
}

void dataFontSetImpl(WasmScriptingVM* vm, uint32_t handle, uint32_t fontHandle)
{
  auto value = resolveInstanceValue<ViewModelInstanceAssetFont>(vm, handle);
  if (vm == nullptr || value == nullptr)
  {
    return;
  }
  auto host = static_cast<HostFont*>(vm->handles().resolve(
      fontHandle,
      WasmScriptingVM::HandleTable::Tag::font));
  value->value(host != nullptr ? host->font.get() : nullptr);
}

HostFont* resolveFont(WasmScriptingVM* vm, uint32_t handle);

void dataFontReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
  // A font the teardown sweep already freed resolves to nothing here.
  releaseHostHandle(vm, handle, WasmScriptingVM::HandleTable::Tag::font);
}

// --- rive_font_v1 / rive_text_v1
// ----------------------------------------------

HostFont* resolveFont(WasmScriptingVM* vm, uint32_t handle)
{
  if (vm == nullptr)
  {
    return nullptr;
  }
  return static_cast<HostFont*>(
      vm->handles().resolve(handle, WasmScriptingVM::HandleTable::Tag::font));
}

uint32_t mintFont(WasmScriptingVM* vm, rcp<Font> font)
{
  if (font == nullptr)
  {
    return 0;
  }
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::font,
      new HostFont{std::move(font)});
}

uint32_t floatBits(float value)
{
  uint32_t bits;
  memcpy(&bits, &value, sizeof(bits));
  return bits;
}

float bitsFloat(uint32_t bits)
{
  float value;
  memcpy(&value, &bits, sizeof(value));
  return value;
}

uint32_t fontFromAssetImpl(
    WasmScriptingVM* vm,
    uint32_t objectHandle,
    const char* name,
    uint32_t length)
{
  auto asset = findFileAsset<FontAsset>(vm, objectHandle, name, length);
  return asset == nullptr ? 0 : mintFont(vm, asset->font());
}

void fontReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
  dataFontReleaseImpl(vm, handle);
}

void fontMetricsImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    float* out,
    uint32_t outCount)
{
  auto host = resolveFont(vm, handle);
  if (host == nullptr || out == nullptr || outCount < 4)
  {
    return;
  }
  const Font::LineMetrics& metrics = host->font->lineMetrics();
  out[0] = metrics.ascent;
  out[1] = metrics.descent;
  out[2] = metrics.capHeight;
  out[3] = metrics.xHeight;
}

uint32_t fontWeightImpl(WasmScriptingVM* vm, uint32_t handle)
{
  auto host = resolveFont(vm, handle);
  return host == nullptr ? 0 : host->font->getWeight();
}

uint32_t fontIsItalicImpl(WasmScriptingVM* vm, uint32_t handle)
{
  auto host = resolveFont(vm, handle);
  return host != nullptr && host->font->isItalic() ? 1 : 0;
}

uint32_t fontAxisCountImpl(WasmScriptingVM* vm, uint32_t handle)
{
  auto host = resolveFont(vm, handle);
  return host == nullptr ? 0 : host->font->getAxisCount();
}

void fontAxisImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint32_t index,
    float* out,
    uint32_t outCount)
{
  auto host = resolveFont(vm, handle);
  if (host == nullptr || out == nullptr || outCount < 4 ||
      index >= host->font->getAxisCount())
  {
    return;
  }
  Font::Axis axis = host->font->getAxis((uint16_t)index);
  out[0] = bitsFloat(axis.tag);
  out[1] = axis.min;
  out[2] = axis.def;
  out[3] = axis.max;
}

float fontAxisValueImpl(WasmScriptingVM* vm, uint32_t handle, uint32_t tag)
{
  auto host = resolveFont(vm, handle);
  return host == nullptr ? 0.0f : host->font->getAxisValue(tag);
}

uint32_t fontFeaturesImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint32_t* out,
    uint32_t outCount)
{
  auto host = resolveFont(vm, handle);
  if (host == nullptr)
  {
    return 0;
  }
  SimpleArray<uint32_t> features = host->font->features();
  return fillOut(
      Span<const uint32_t>(features.data(), features.size()),
      out,
      outCount);
}

uint32_t fontHasGlyphImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint32_t codepoint)
{
  auto host = resolveFont(vm, handle);
  return host != nullptr && host->font->hasGlyph(codepoint) ? 1 : 0;
}

uint32_t fontWithOptionsImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    const uint32_t* coords,
    uint32_t coordCount,
    const uint32_t* features,
    uint32_t featureCount)
{
  auto host = resolveFont(vm, handle);
  if (host == nullptr)
  {
    return 0;
  }
  std::vector<Font::Coord> axes;
  for (uint32_t i = 0; i + 1 < coordCount; i += 2)
  {
    axes.push_back({coords[i], bitsFloat(coords[i + 1])});
  }
  std::vector<Font::Feature> settings;
  for (uint32_t i = 0; i + 1 < featureCount; i += 2)
  {
    settings.push_back({features[i], features[i + 1]});
  }
  return mintFont(vm, host->font->withOptions(axes, settings));
}

#ifdef WITH_RIVE_TEXT
uint32_t fontDecodeImpl(
    WasmScriptingVM* vm,
    const uint8_t* bytes,
    uint32_t byteCount)
{
  if (vm == nullptr || bytes == nullptr)
  {
    return 0;
  }
  return mintFont(vm, HBFont::Decode(Span<const uint8_t>(bytes, byteCount)));
}

// The outline crosses in two calls, so the last one is kept for the second.
const RawPath& fontGlyph(HostFont* host, uint32_t glyph)
{
  if (!host->hasGlyphPath || host->glyphPathId != glyph)
  {
    host->glyphPath = RawText::glyphPath(*host->font, (GlyphID)glyph);
    host->glyphPathId = glyph;
    host->hasGlyphPath = true;
  }
  return host->glyphPath;
}

uint32_t fontGlyphVerbsImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint32_t glyph,
    uint8_t* out,
    uint32_t outCount)
{
  auto host = resolveFont(vm, handle);
  if (host == nullptr)
  {
    return 0;
  }
  return fillOut(pathVerbBytes(fontGlyph(host, glyph)), out, outCount);
}

uint32_t fontGlyphPointsImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint32_t glyph,
    float* out,
    uint32_t outCount)
{
  auto host = resolveFont(vm, handle);
  if (host == nullptr)
  {
    return 0;
  }
  return fillOut(pathFloats(fontGlyph(host, glyph)), out, outCount);
}

// A Text builtin: the layout plus the flat records the module reads it
// through, rebuilt after any change.
struct HostText
{
  HostText(Factory* factory) : text(factory) {}
  RawText text;
  std::vector<const GlyphRun*> runs;
  std::vector<uint32_t> lineWords;
  std::vector<uint32_t> runWords;
  std::vector<uint32_t> glyphWords;
  // Fonts the layout produced, fallbacks included, handed to the module as
  // handles this text owns.
  std::unordered_map<const Font*, uint32_t> fontHandles;
  // RawText revision the records were built from; ~0u before the first.
  uint32_t snapshotRevision = ~0u;
};

HostText* resolveText(WasmScriptingVM* vm, uint32_t handle)
{
  if (vm == nullptr)
  {
    return nullptr;
  }
  return static_cast<HostText*>(
      vm->handles().resolve(handle, WasmScriptingVM::HandleTable::Tag::text));
}

void deleteHostText(WasmScriptingVM* vm, HostText* host)
{
  if (host == nullptr)
  {
    return;
  }
  for (auto& pair : host->fontHandles)
  {
    dataFontReleaseImpl(vm, pair.second);
  }
  delete host;
}

void buildTextSnapshot(WasmScriptingVM* vm, HostText* host)
{
  if (host->snapshotRevision == host->text.revision())
  {
    return;
  }
  host->snapshotRevision = host->text.revision();
  host->runs.clear();
  host->lineWords.clear();
  host->runWords.clear();
  host->glyphWords.clear();

  RawText& text = host->text;
  const GlyphLookup& lookup = text.layoutView().glyphLookup();
  std::unordered_map<const GlyphRun*, uint32_t> runIndices;
  uint32_t glyphCount = 0;
  for (uint32_t line = 0; line < text.lineCount(); line++)
  {
    const OrderedLine& orderedLine = text.orderedLines()[line];
    uint32_t glyphStart = glyphCount;
    text.forEachGlyph(
        line,
        [&](const GlyphRun& run, uint32_t glyphIndex, Vec2D position) {
          auto indexed = runIndices.emplace(&run, (uint32_t)host->runs.size());
          if (indexed.second)
          {
            host->runs.push_back(&run);
          }
          glyphCount++;
          GlyphID glyphId = run.glyphs[glyphIndex];
          uint32_t flags = (run.level & 1) |
                           (run.font->isColorGlyph(glyphId) ? 2 : 0);
          host->glyphWords.insert(
              host->glyphWords.end(),
              {glyphId,
               run.textIndices[glyphIndex],
               indexed.first->second,
               flags,
               floatBits(position.x),
               floatBits(position.y),
               floatBits(run.advances[glyphIndex])});
          return true;
        });
    host->lineWords.insert(
        host->lineWords.end(),
        {glyphStart,
         glyphCount - glyphStart,
         (uint32_t)text.lineDirection(line),
         orderedLine.firstCodePointIndex(lookup),
         orderedLine.lastCodePointIndex(lookup),
         floatBits(orderedLine.glyphLine().startX),
         floatBits(text.lineTop(line)),
         floatBits(orderedLine.y()),
         floatBits(orderedLine.bottom())});
  }
  // A font no run uses any more gives its handle back, or a text relaid
  // out with a fresh font variant every frame would keep one per frame.
  std::unordered_map<const Font*, uint32_t> fontHandles;
  for (const GlyphRun* run : host->runs)
  {
    auto found = fontHandles.find(run->font.get());
    if (found == fontHandles.end())
    {
      auto kept = host->fontHandles.find(run->font.get());
      uint32_t handle;
      if (kept != host->fontHandles.end())
      {
        handle = kept->second;
        host->fontHandles.erase(kept);
      }
      else
      {
        handle = mintFont(vm, run->font);
      }
      found = fontHandles.emplace(run->font.get(), handle).first;
    }
    host->runWords.insert(
        host->runWords.end(),
        {found->second,
         (uint32_t)(run->level & 1),
         floatBits(run->size),
         floatBits(run->lineHeight),
         floatBits(run->letterSpacing)});
  }
  for (auto& pair : host->fontHandles)
  {
    dataFontReleaseImpl(vm, pair.second);
  }
  host->fontHandles = std::move(fontHandles);
}

uint32_t textNewImpl(WasmScriptingVM* vm)
{
  if (vm == nullptr)
  {
    return 0;
  }
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::text,
      new HostText(vm->factory()));
}

void textReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
  releaseHostHandle(vm, handle, WasmScriptingVM::HandleTable::Tag::text);
}

void textAppendImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    const char* chars,
    uint32_t length,
    uint32_t fontHandle,
    uint32_t paintHandle,
    float size,
    float lineHeight,
    float letterSpacing,
    uint32_t foreground)
{
  auto host = resolveText(vm, handle);
  auto font = resolveFont(vm, fontHandle);
  if (host == nullptr || font == nullptr)
  {
    return;
  }
  RenderPaint* paint = resolvePaint(vm, paintHandle);
  host->text.append(
      std::string(chars, length),
      paint != nullptr ? ref_rcp(paint) : nullptr,
      font->font,
      size,
      lineHeight,
      letterSpacing,
      foreground);
}

void textClearImpl(WasmScriptingVM* vm, uint32_t handle)
{
  if (auto host = resolveText(vm, handle))
  {
    host->text.clear();
  }
}

void textLayoutImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    const rive_text_layout_desc_v1* desc,
    uint32_t descByteCount)
{
  auto host = resolveText(vm, handle);
  if (host == nullptr || desc == nullptr ||
      descByteCount < sizeof(rive_text_layout_desc_v1))
  {
    return;
  }
  RawText& text = host->text;
  text.sizingIndex((int)desc->sizing);
  text.overflowIndex((int)desc->overflow);
  text.alignIndex((int)desc->align);
  text.wrapIndex((int)desc->wrap);
  text.wordBreakIndex((int)desc->wordBreak);
  text.originIndex((int)desc->origin);
  text.directionIndex((int)desc->direction);
  text.maxWidth(desc->maxWidth);
  text.maxHeight(desc->maxHeight);
  text.paragraphSpacing(desc->paragraphSpacing);
}

void textDrawImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint32_t rendererHandle,
    uint32_t paintHandle)
{
  auto host = resolveText(vm, handle);
  auto renderer = resolveRenderer(vm, rendererHandle);
  if (host == nullptr || renderer == nullptr)
  {
    return;
  }
  RenderPaint* paint = resolvePaint(vm, paintHandle);
  host->text.render(renderer, paint != nullptr ? ref_rcp(paint) : nullptr);
}

void textBoundsImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    float* out,
    uint32_t outCount)
{
  auto host = resolveText(vm, handle);
  if (host == nullptr || out == nullptr || outCount < 4)
  {
    return;
  }
  AABB bounds = host->text.bounds();
  out[0] = bounds.minX;
  out[1] = bounds.minY;
  out[2] = bounds.maxX;
  out[3] = bounds.maxY;
}

uint32_t textLengthImpl(WasmScriptingVM* vm, uint32_t handle)
{
  auto host = resolveText(vm, handle);
  return host == nullptr ? 0 : (uint32_t)host->text.length();
}

uint32_t textLinesImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint32_t* out,
    uint32_t outCount)
{
  auto host = resolveText(vm, handle);
  if (host == nullptr)
  {
    return 0;
  }
  buildTextSnapshot(vm, host);
  return fillOut(host->lineWords, out, outCount);
}

uint32_t textRunsImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint32_t* out,
    uint32_t outCount)
{
  auto host = resolveText(vm, handle);
  if (host == nullptr)
  {
    return 0;
  }
  buildTextSnapshot(vm, host);
  return fillOut(host->runWords, out, outCount);
}

uint32_t textGlyphsImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint32_t* out,
    uint32_t outCount)
{
  auto host = resolveText(vm, handle);
  if (host == nullptr)
  {
    return 0;
  }
  buildTextSnapshot(vm, host);
  return fillOut(host->glyphWords, out, outCount);
}

uint32_t textHitTestImpl(WasmScriptingVM* vm, uint32_t handle, float x, float y)
{
  auto host = resolveText(vm, handle);
  if (host == nullptr)
  {
    return 0;
  }
  TextLayoutView view = host->text.layoutView();
  return CursorPosition::fromTranslation(Vec2D(x, y), view).codePointIndex();
}

uint32_t textCaretImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint32_t index,
    float* out,
    uint32_t outCount)
{
  auto host = resolveText(vm, handle);
  if (host == nullptr || out == nullptr || outCount < 3)
  {
    return 0;
  }
  TextLayoutView view = host->text.layoutView();
  auto visual = CursorPosition::atIndex(index, view).visualPosition(view);
  if (!visual.found())
  {
    return 0;
  }
  out[0] = visual.x();
  out[1] = visual.top();
  out[2] = visual.bottom();
  return 1;
}

uint32_t textSelectionRectsImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint32_t from,
    uint32_t to,
    float* out,
    uint32_t outCount)
{
  auto host = resolveText(vm, handle);
  if (host == nullptr)
  {
    return 0;
  }
  TextLayoutView view = host->text.layoutView();
  Cursor cursor(
      CursorPosition::atIndex(from, view),
      CursorPosition::atIndex(to, view));
  std::vector<AABB> rects;
  cursor.selectionRects(rects, view);
  std::vector<float> words;
  for (const AABB& rect : rects)
  {
    words.insert(words.end(), {rect.minX, rect.minY, rect.maxX, rect.maxY});
  }
  return fillOut(words, out, outCount);
}
#else
struct HostText
{};
void deleteHostText(WasmScriptingVM*, HostText*) {}
#endif

// Mirrors ScriptedPropertyBlob::pushValue: a non-null instance asset (even
// empty) wins, else the id-bound asset through the file registry.
rcp<FileAsset> resolveBlobAsset(WasmScriptingVM* vm, uint32_t handle)
{
  auto value = resolveInstanceValue<ViewModelInstanceAssetBlob>(vm, handle);
  if (vm == nullptr || value == nullptr)
  {
    return nullptr;
  }
  if (auto asset = value->asset())
  {
    return asset;
  }
  if (vm->file() != nullptr)
  {
    auto fileAsset = vm->file()->asset(value->propertyValue());
    if (fileAsset != nullptr && fileAsset->is<BlobAsset>())
    {
      return fileAsset;
    }
  }
  return nullptr;
}

uint32_t blobAssetBytesImpl(
    WasmScriptingVM* vm,
    uint32_t objectHandle,
    const char* name,
    uint32_t nameLength,
    uint8_t* out,
    uint32_t outCount)
{
  auto asset = findFileAsset<BlobAsset>(
      vm,
      objectHandle,
      name,
      nameLength,
      [](FileAsset* blob) { return !blob->as<BlobAsset>()->bytes().empty(); });
  if (asset == nullptr)
  {
    return 0;
  }
  auto bytes = asset->bytes();
  if (bytes.size() <= outCount)
  {
    memcpy(out, bytes.data(), bytes.size());
  }
  return (uint32_t)bytes.size();
}

// --- rive_file_v1 and artboard properties: context:decodeFile ---------------
//
// The Luau lane's decodeFile, RiveFile, BindableArtboard and
// Property<BindableArtboard> (lua_rive_file.cpp, ScriptedPropertyArtboard),
// with the file and its bindables held host side. Like the Luau lane they
// exist only where fetch() does; elsewhere every op answers none.

struct HostRiveFile
{
  rcp<File> file;
};

using HostBindableArtboard = DecodedBindable;

HostRiveFile* resolveRiveFile(WasmScriptingVM* vm, uint32_t handle)
{
  return vm == nullptr ? nullptr
                       : static_cast<HostRiveFile*>(vm->handles().resolve(
                             handle,
                             WasmScriptingVM::HandleTable::Tag::riveFile));
}

HostBindableArtboard* resolveBindableArtboard(
    WasmScriptingVM* vm,
    uint32_t handle)
{
  return vm == nullptr
             ? nullptr
             : static_cast<HostBindableArtboard*>(vm->handles().resolve(
                   handle,
                   WasmScriptingVM::HandleTable::Tag::bindableArtboard));
}

// The VM decoding a file, which the file's own VMs collect with.
thread_local WasmScriptingVM* s_decodingParent = nullptr;

uint32_t fileDecodeImpl(
    WasmScriptingVM* vm,
    const uint8_t* bytes,
    uint32_t byteCount,
    uint32_t* statusOut,
    uint32_t statusCount)
{
  auto status = [&](uint32_t value) {
    if (statusCount > 0)
    {
      statusOut[0] = value;
    }
  };
#ifdef WITH_RIVE_SCRIPTNET
  if (vm == nullptr || vm->factory() == nullptr)
  {
    status(FileDecodeWire::statusNoFactory);
    return 0;
  }
  ImportResult result = ImportResult::malformed;
  WasmScriptingVM* outer = s_decodingParent;
  s_decodingParent = vm;
  auto file = byteCount == 0 ? nullptr
                             : importDecodedFile(
                                   Span<const uint8_t>(bytes, byteCount),
                                   vm->factory(),
                                   &result);
  s_decodingParent = outer;
  if (file == nullptr)
  {
    status(
        result == ImportResult::unsupportedVersion
            ? FileDecodeWire::statusUnsupportedVersion
            : FileDecodeWire::statusMalformed);
    return 0;
  }
  status(FileDecodeWire::statusOk);
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::riveFile,
      new HostRiveFile{std::move(file)});
#else
  (void)vm;
  (void)bytes;
  (void)byteCount;
  status(FileDecodeWire::statusUnavailable);
  return 0;
#endif
}

void fileReleaseImpl(WasmScriptingVM* vm, uint32_t file)
{
  releaseHostHandle(vm, file, WasmScriptingVM::HandleTable::Tag::riveFile);
}

uint32_t fileArtboardCountImpl(WasmScriptingVM* vm, uint32_t file)
{
  auto host = resolveRiveFile(vm, file);
  return host == nullptr ? 0 : (uint32_t)host->file->artboardCount();
}

// One name at a time: names are file data, so no separator is safe.
uint32_t fileArtboardNameImpl(
    WasmScriptingVM* vm,
    uint32_t file,
    uint32_t index,
    char* buffer,
    uint32_t capacity)
{
  auto host = resolveRiveFile(vm, file);
  if (host == nullptr || index >= host->file->artboardCount())
  {
    return 0;
  }
  return fillModuleString(
      vm,
      host->file->artboard(index)->name(),
      buffer,
      capacity);
}

uint32_t fileBindableImpl(
    WasmScriptingVM* vm,
    uint32_t file,
    const char* name,
    uint32_t nameLength,
    uint32_t useDefault)
{
  auto host = resolveRiveFile(vm, file);
  if (host == nullptr)
  {
    return 0;
  }
  std::string named(name, nameLength);
  DecodedBindable decoded = decodedBindable(
      *host->file,
      useDefault != 0 ? nullptr : named.c_str());
  if (decoded.artboard == nullptr)
  {
    return 0;
  }
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::bindableArtboard,
      new HostBindableArtboard(std::move(decoded)));
}

void fileBindableReleaseImpl(WasmScriptingVM* vm, uint32_t bindable)
{
  releaseHostHandle(
      vm,
      bindable,
      WasmScriptingVM::HandleTable::Tag::bindableArtboard);
}

uint32_t fileBindableNameImpl(
    WasmScriptingVM* vm,
    uint32_t bindable,
    char* buffer,
    uint32_t capacity)
{
  auto host = resolveBindableArtboard(vm, bindable);
  if (host == nullptr)
  {
    return 0;
  }
  return fillModuleString(
      vm,
      host->artboard->artboard()->name(),
      buffer,
      capacity);
}

uint32_t fileBindableDataImpl(WasmScriptingVM* vm, uint32_t bindable)
{
  auto host = resolveBindableArtboard(vm, bindable);
  if (host == nullptr || host->viewModel == nullptr)
  {
    return 0;
  }
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::viewModelInstance,
      new HostViewModelInstance{host->viewModel});
}

uint32_t fileBindableEqualImpl(WasmScriptingVM* vm, uint32_t a, uint32_t b)
{
  auto first = resolveBindableArtboard(vm, a);
  auto second = resolveBindableArtboard(vm, b);
  return first != nullptr && second != nullptr &&
                 first->artboard == second->artboard
             ? 1
             : 0;
}

uint32_t dataVmiArtboardImpl(
    WasmScriptingVM* vm,
    uint32_t vmiHandle,
    const char* name,
    uint32_t length)
{
#ifdef WITH_RIVE_SCRIPTNET
  return mintInstanceValue<ViewModelInstanceArtboard>(
      vm,
      vmiHandle,
      name,
      length);
#else
  (void)vm;
  (void)vmiHandle;
  (void)name;
  (void)length;
  return 0;
#endif
}

// Mirrors ScriptedPropertyArtboard::pushValue: nil while the property names
// one of the host file's own artboards.
uint32_t dataArtboardGetImpl(WasmScriptingVM* vm, uint32_t handle)
{
  auto value = resolveInstanceValue<ViewModelInstanceArtboard>(vm, handle);
  if (value == nullptr || value->asset() == nullptr)
  {
    return 0;
  }
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::bindableArtboard,
      new HostBindableArtboard{
          value->asset(),
          value->boundViewModelInstance()});
}

// Mirrors ScriptedPropertyArtboard::setValue: the artboard's own view model
// goes in first, since asset() notifies and whatever mounts the artboard
// binds it to that instance.
void dataArtboardSetImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint32_t bindable)
{
  auto value = resolveInstanceValue<ViewModelInstanceArtboard>(vm, handle);
  if (value == nullptr)
  {
    return;
  }
  auto host = resolveBindableArtboard(vm, bindable);
  value->boundViewModelInstance(host != nullptr ? host->viewModel : nullptr);
  value->asset(host != nullptr ? host->artboard : nullptr);
}

// --- rive_net_v1: fetch() --------------------------------------------------

uint32_t netFetchImpl(
    WasmScriptingVM* vm,
    const uint8_t* request,
    uint32_t requestCount,
    uint32_t token)
{
  if (vm == nullptr)
  {
    return 0;
  }
  return vm->startFetch(Span<const uint8_t>(request, requestCount), token) ? 1
                                                                           : 0;
}

void netFetchCancelImpl(WasmScriptingVM* vm, uint32_t token)
{
  if (vm != nullptr)
  {
    vm->cancelFetch(token);
  }
}

// --- rive_audio_v1 ---------------------------------------------------------

#ifdef WITH_RIVE_AUDIO
struct HostAudioSource
{
  rcp<AudioSource> source;
};

struct HostAudioSound
{
  rcp<AudioSound> sound;
};

HostAudioSource* resolveAudioSource(WasmScriptingVM* vm, uint32_t handle)
{
  if (vm == nullptr)
  {
    return nullptr;
  }
  return static_cast<HostAudioSource*>(vm->handles().resolve(
      handle,
      WasmScriptingVM::HandleTable::Tag::audioSource));
}

HostAudioSound* resolveAudioSound(WasmScriptingVM* vm, uint32_t handle)
{
  if (vm == nullptr)
  {
    return nullptr;
  }
  return static_cast<HostAudioSound*>(vm->handles().resolve(
      handle,
      WasmScriptingVM::HandleTable::Tag::audioSound));
}

// The Luau play and playFrame shapes: relative times offset from the
// engine clock, the sound minted at the artboard-free volume of 1. Tools
// builds refuse to play while playback is paused.
uint32_t audioPlaySound(WasmScriptingVM* vm, rcp<AudioSound> sound)
{
  if (sound == nullptr)
  {
    return 0;
  }
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::audioSound,
      new HostAudioSound{std::move(sound)});
}

rcp<AudioEngine> audioPlayEngine(WasmScriptingVM* vm, HostAudioSource* host)
{
  if (host == nullptr)
  {
    return nullptr;
  }
#ifdef WITH_RIVE_TOOLS
  if (!vm->isPlaying())
  {
    return nullptr;
  }
#endif
  return AudioEngine::RuntimeEngine(true);
}

uint32_t audioPlaySeconds(
    WasmScriptingVM* vm,
    uint32_t sourceHandle,
    float seconds,
    bool relative)
{
  auto host = resolveAudioSource(vm, sourceHandle);
  auto engine = audioPlayEngine(vm, host);
  if (engine == nullptr)
  {
    return 0;
  }
  if (relative)
  {
    seconds += engine->timeInSeconds();
  }
  return audioPlaySound(vm, engine->playSeconds(host->source, seconds, 0, 0));
}

uint32_t audioPlayFrames(
    WasmScriptingVM* vm,
    uint32_t sourceHandle,
    double frames,
    bool relative)
{
  auto host = resolveAudioSource(vm, sourceHandle);
  auto engine = audioPlayEngine(vm, host);
  if (engine == nullptr)
  {
    return 0;
  }
  uint64_t startTime = (uint64_t)frames;
  if (relative)
  {
    startTime += engine->timeInFrames();
  }
  return audioPlaySound(vm, engine->play(host->source, startTime, 0, 0));
}
#endif

uint32_t audioSourceImpl(
    WasmScriptingVM* vm,
    uint32_t objectHandle,
    const char* name,
    uint32_t nameLength)
{
#ifdef WITH_RIVE_AUDIO
  auto asset = findFileAsset<AudioAsset>(
      vm,
      objectHandle,
      name,
      nameLength,
      [](FileAsset* audio) {
        return audio->as<AudioAsset>()->audioSource() != nullptr;
      });
  if (asset == nullptr)
  {
    return 0;
  }
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::audioSource,
      new HostAudioSource{asset->audioSource()});
#endif
  return 0;
}

void audioSourceReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
#ifdef WITH_RIVE_AUDIO
  auto host = resolveAudioSource(vm, handle);
  if (host == nullptr)
  {
    return;
  }
  vm->handles().release(handle, WasmScriptingVM::HandleTable::Tag::audioSource);
  delete host;
#endif
}

float audioSourceDurationImpl(WasmScriptingVM* vm, uint32_t handle)
{
#ifdef WITH_RIVE_AUDIO
  auto host = resolveAudioSource(vm, handle);
  return host != nullptr ? host->source->duration() : 0.0f;
#else
  return 0.0f;
#endif
}

uint32_t audioSourceSampleRateImpl(WasmScriptingVM* vm, uint32_t handle)
{
#ifdef WITH_RIVE_AUDIO
  auto host = resolveAudioSource(vm, handle);
  return host != nullptr ? host->source->sampleRate() : 0;
#else
  return 0;
#endif
}

uint32_t audioSourceChannelsImpl(WasmScriptingVM* vm, uint32_t handle)
{
#ifdef WITH_RIVE_AUDIO
  auto host = resolveAudioSource(vm, handle);
  return host != nullptr ? host->source->channels() : 0;
#else
  return 0;
#endif
}

uint32_t audioPlayImpl(WasmScriptingVM* vm, uint32_t source)
{
#ifdef WITH_RIVE_AUDIO
  return audioPlaySeconds(vm, source, 0.0f, true);
#else
  return 0;
#endif
}

uint32_t audioPlayAtTimeImpl(
    WasmScriptingVM* vm,
    uint32_t source,
    float seconds)
{
#ifdef WITH_RIVE_AUDIO
  return audioPlaySeconds(vm, source, seconds, false);
#else
  return 0;
#endif
}

uint32_t audioPlayInTimeImpl(
    WasmScriptingVM* vm,
    uint32_t source,
    float seconds)
{
#ifdef WITH_RIVE_AUDIO
  return audioPlaySeconds(vm, source, seconds, true);
#else
  return 0;
#endif
}

uint32_t audioPlayAtFrameImpl(
    WasmScriptingVM* vm,
    uint32_t source,
    double frame)
{
#ifdef WITH_RIVE_AUDIO
  return audioPlayFrames(vm, source, frame, false);
#else
  return 0;
#endif
}

uint32_t audioPlayInFrameImpl(
    WasmScriptingVM* vm,
    uint32_t source,
    double frame)
{
#ifdef WITH_RIVE_AUDIO
  return audioPlayFrames(vm, source, frame, true);
#else
  return 0;
#endif
}

float audioTimeImpl(WasmScriptingVM* vm)
{
  uint64_t frame;
  uint32_t rate;
  if (WasmProcessors::offlineClock(frame, rate))
  {
    return (float)((double)frame / rate);
  }
#ifdef WITH_RIVE_AUDIO
  auto engine = AudioEngine::RuntimeEngine(true);
  return engine != nullptr ? engine->timeInSeconds() : 0.0f;
#else
  return 0.0f;
#endif
}

double audioTimeFrameImpl(WasmScriptingVM* vm)
{
  uint64_t frame;
  uint32_t rate;
  if (WasmProcessors::offlineClock(frame, rate))
  {
    return (double)frame;
  }
#ifdef WITH_RIVE_AUDIO
  auto engine = AudioEngine::RuntimeEngine(true);
  return engine != nullptr ? (double)engine->timeInFrames() : 0.0;
#else
  return 0.0;
#endif
}

uint32_t audioSampleRateImpl(WasmScriptingVM* vm)
{
  uint64_t frame;
  uint32_t rate;
  if (WasmProcessors::offlineClock(frame, rate))
  {
    return rate;
  }
#ifdef WITH_RIVE_AUDIO
  auto engine = AudioEngine::RuntimeEngine(true);
  return engine != nullptr ? engine->sampleRate() : 0;
#else
  return 0;
#endif
}

void audioSoundReleaseImpl(WasmScriptingVM* vm, uint32_t handle)
{
#ifdef WITH_RIVE_AUDIO
  auto host = resolveAudioSound(vm, handle);
  if (host == nullptr)
  {
    return;
  }
  vm->handles().release(handle, WasmScriptingVM::HandleTable::Tag::audioSound);
  delete host;
#endif
}

void audioSoundPlayImpl(WasmScriptingVM* vm, uint32_t handle)
{
#ifdef WITH_RIVE_AUDIO
  auto host = resolveAudioSound(vm, handle);
  if (host != nullptr)
  {
    host->sound->play();
  }
#endif
}

void audioSoundPauseImpl(WasmScriptingVM* vm, uint32_t handle)
{
#ifdef WITH_RIVE_AUDIO
  auto host = resolveAudioSound(vm, handle);
  if (host != nullptr)
  {
    host->sound->pause();
  }
#endif
}

void audioSoundResumeImpl(WasmScriptingVM* vm, uint32_t handle)
{
#ifdef WITH_RIVE_AUDIO
  auto host = resolveAudioSound(vm, handle);
  if (host != nullptr)
  {
    host->sound->resume();
  }
#endif
}

void audioSoundStopImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint32_t fadeFrames)
{
#ifdef WITH_RIVE_AUDIO
  auto host = resolveAudioSound(vm, handle);
  if (host != nullptr)
  {
    host->sound->stop(fadeFrames);
  }
#endif
}

uint32_t audioSoundSeekImpl(WasmScriptingVM* vm, uint32_t handle, float seconds)
{
#ifdef WITH_RIVE_AUDIO
  auto host = resolveAudioSound(vm, handle);
  return host != nullptr && host->sound->seekSeconds(seconds) ? 1 : 0;
#else
  return 0;
#endif
}

uint32_t audioSoundSeekFrameImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    double frame)
{
#ifdef WITH_RIVE_AUDIO
  auto host = resolveAudioSound(vm, handle);
  return host != nullptr && host->sound->seek((uint64_t)frame) ? 1 : 0;
#else
  return 0;
#endif
}

uint32_t audioSoundCompletedImpl(WasmScriptingVM* vm, uint32_t handle)
{
#ifdef WITH_RIVE_AUDIO
  auto host = resolveAudioSound(vm, handle);
  return host != nullptr && host->sound->completed() ? 1 : 0;
#else
  return 0;
#endif
}

float audioSoundTimeImpl(WasmScriptingVM* vm, uint32_t handle)
{
#ifdef WITH_RIVE_AUDIO
  auto host = resolveAudioSound(vm, handle);
  return host != nullptr ? host->sound->timeInSeconds() : 0.0f;
#else
  return 0.0f;
#endif
}

double audioSoundTimeFrameImpl(WasmScriptingVM* vm, uint32_t handle)
{
#ifdef WITH_RIVE_AUDIO
  auto host = resolveAudioSound(vm, handle);
  return host != nullptr ? (double)host->sound->timeInFrames() : 0.0;
#else
  return 0.0;
#endif
}

float audioSoundVolumeImpl(WasmScriptingVM* vm, uint32_t handle)
{
#ifdef WITH_RIVE_AUDIO
  auto host = resolveAudioSound(vm, handle);
  return host != nullptr ? host->sound->volume() : 0.0f;
#else
  return 0.0f;
#endif
}

void audioSoundSetVolumeImpl(WasmScriptingVM* vm, uint32_t handle, float value)
{
#ifdef WITH_RIVE_AUDIO
  auto host = resolveAudioSound(vm, handle);
  if (host != nullptr)
  {
    host->sound->volume(value);
  }
#endif
}

uint32_t dataBlobPresentImpl(WasmScriptingVM* vm, uint32_t handle)
{
  return resolveBlobAsset(vm, handle) != nullptr ? 1 : 0;
}

uint32_t dataBlobGetImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    uint8_t* buffer,
    uint32_t capacity)
{
  auto asset = resolveBlobAsset(vm, handle);
  if (asset == nullptr)
  {
    return 0;
  }
  auto bytes = asset->as<BlobAsset>()->bytes();
  size_t copied = bytes.size() < capacity ? bytes.size() : capacity;
  memcpy(buffer, bytes.data(), copied);
  return (uint32_t)bytes.size();
}

uint32_t dataBlobNameImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    char* buffer,
    uint32_t capacity)
{
  auto asset = resolveBlobAsset(vm, handle);
  if (asset == nullptr)
  {
    return 0;
  }
  return fillModuleString(vm, asset->name(), buffer, capacity);
}

void dataBlobSetImpl(
    WasmScriptingVM* vm,
    uint32_t handle,
    const uint8_t* bytes,
    uint32_t byteCount)
{
  auto value = resolveInstanceValue<ViewModelInstanceAssetBlob>(vm, handle);
  if (value == nullptr)
  {
    return;
  }
  auto asset = make_rcp<BlobAsset>();
  SimpleArray<uint8_t> data(bytes, byteCount);
  asset->decode(data, nullptr);
  value->value(asset.get());
}

void dataBlobClearImpl(WasmScriptingVM* vm, uint32_t handle)
{
  auto value = resolveInstanceValue<ViewModelInstanceAssetBlob>(vm, handle);
  if (value != nullptr)
  {
    value->value(nullptr);
  }
}

HostInstanceValue* resolveHostValue(WasmScriptingVM* vm, uint32_t handle)
{
  if (vm == nullptr)
  {
    return nullptr;
  }
  return static_cast<HostInstanceValue*>(vm->handles().resolve(
      handle,
      WasmScriptingVM::HandleTable::Tag::instanceValue));
}

void dataWatchImpl(WasmScriptingVM* vm, uint32_t handle, uint32_t token)
{
  auto host = resolveHostValue(vm, handle);
  if (host == nullptr || host->delegate != nullptr)
  {
    return;
  }
  host->delegate = new HostValueDelegate();
  host->delegate->vm = vm;
  host->delegate->token = token;
  host->value->addDelegate(host->delegate);
}

void dataUnwatchImpl(WasmScriptingVM* vm, uint32_t handle)
{
  auto host = resolveHostValue(vm, handle);
  if (host == nullptr || host->delegate == nullptr)
  {
    return;
  }
  host->value->removeDelegate(host->delegate);
  delete host->delegate;
  host->delegate = nullptr;
}

#ifndef __EMSCRIPTEN__
bool initRuntime()
{
  RuntimeInitArgs args;
  memset(&args, 0, sizeof(args));
  args.mem_alloc_type = Alloc_With_System_Allocator;
  if (!wasm_runtime_full_init(&args))
  {
    return false;
  }
  // Memory grows on demand, so a failed grow is the only sign of a
  // ceiling or device limit before the module traps.
  wasm_runtime_set_enlarge_mem_error_callback(
      [](uint32_t incPages,
         uint64_t currentBytes,
         uint32_t,
         enlarge_memory_error_reason_t reason,
         wasm_module_inst_t,
         wasm_exec_env_t env,
         void*) {
        reportScriptError(
            env != nullptr ? bootVmFromEnv(env) : nullptr,
            "wasm memory: growing " + std::to_string(currentBytes >> 20) +
                " MB by " + std::to_string(incPages) + " pages failed: " +
                (reason == MAX_SIZE_REACHED
                     ? "the module hit its wasmMaxPages ceiling"
                     : "the device could not provide the memory"));
      },
      nullptr);
  wasm_runtime_register_natives(
      "env",
      kEnvNatives,
      sizeof(kEnvNatives) / sizeof(NativeSymbol));
  wasm_runtime_register_natives(
      "wasi_snapshot_preview1",
      kWasiNatives,
      sizeof(kWasiNatives) / sizeof(NativeSymbol));
  if (!registerRiveBindingNatives())
  {
    return false;
  }
  registerLaneNatives();
  registerJobNatives();
  registerWorkerNatives();
  return true;
}

bool ensureRuntime()
{
  // Hosts boot on more than one thread, so a boot waits out another's init.
  static bool ok = initRuntime();
  return ok;
}
#endif

// --- rive_shared_v1: memory every lane shares -----------------------------

using shared_layout::kSharedMaxBytes;

// The lane a native runs on, null on the main lane.
WasmLane* callingLane(WasmScriptingVM* vm)
{
  return vm == nullptr ? WasmLane::current() : nullptr;
}

WasmLane* callingProcessor(WasmScriptingVM* vm)
{
#if RIVE_PROCESSOR_AUDIO()
  WasmLane* lane = callingLane(vm);
  return lane != nullptr && lane->kind() == LaneKind::processor ? lane
                                                                : nullptr;
#else
  return nullptr;
#endif
}

// Whose holds a call takes and gives back: the VM's, or its lane's.
uint64_t sharedHolder(WasmScriptingVM* vm)
{
  WasmLane* lane = callingLane(vm);
  return vm != nullptr     ? vm->sharedOwner()
         : lane != nullptr ? lane->sharedOwner()
                           : 0;
}

template <typename T> rcp<T> sharedFind(WasmScriptingVM* vm, uint32_t id)
{
  return SharedRegistry::find<T>(id, sharedHolder(vm));
}

bool isSharedId(uint32_t id) { return (id & shared_layout::kIdBit) != 0; }

// The creator, charged size; else 0 with the create's answer in refusal, as
// a processor never may create and a job makes only blobs.
uint64_t sharedCreator(
    WasmScriptingVM* vm,
    uint64_t size,
    bool allowEmpty,
    bool blob,
    uint32_t& refusal)
{
  WasmLane* lane = callingLane(vm);
  bool may = lane == nullptr || lane->kind() == LaneKind::worker ||
             (blob && lane->kind() == LaneKind::job);
  uint64_t owner = may ? sharedHolder(vm) : 0;
  refusal = shared_layout::kCreateRefused;
  if (owner != 0 && ((size == 0 && !allowEmpty) || size > kSharedMaxBytes))
  {
    refusal = shared_layout::kCreateBadSize;
    owner = 0;
  }
  if (owner != 0 && !SharedRegistry::charge(owner, size))
  {
    refusal = shared_layout::kCreateNoMemory;
    owner = 0;
  }
  return owner;
}

// What a create answers for object, made for owner with size charged: its
// id once in the registry, else why not, its charge refunded.
uint32_t sharedCreated(
    WasmScriptingVM* vm,
    uint64_t owner,
    uint64_t size,
    rcp<SharedObject> object)
{
  if (!object->made())
  {
    SharedRegistry::refund(owner, size);
    return shared_layout::kCreateNoMemory;
  }
  object->charge = size;
  uint32_t id = SharedRegistry::add(std::move(object), owner);
  if (!isSharedId(id))
  {
    SharedRegistry::refund(owner, size);
    return id;
  }
  if (vm == nullptr)
  {
    WasmLane::current()->addCreated(id);
  }
  return id;
}

// Whether a misuse warns: once per object, never on a processor, which may
// be on the audio thread, so the text is built only for a warning.
bool sharedWarns(WasmScriptingVM* vm, SharedObject& object)
{
  WasmLane* lane = callingLane(vm);
  return (vm != nullptr ||
          (lane != nullptr && lane->kind() != LaneKind::processor)) &&
         object.firstWarning();
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 2, 3)))
#endif
void sharedWarn(WasmScriptingVM* vm, const char* format, ...)
{
  char text[256];
  va_list args;
  va_start(args, format);
  vsnprintf(text, sizeof(text), format, args);
  va_end(args);
  if (vm != nullptr)
  {
    rtLogImpl(vm, 0, text, (uint32_t)strlen(text));
  }
  else
  {
    callingLane(vm)->log(text);
  }
}

// A processor may be on the audio thread, where taking or giving back a hold
// could lock or free, so a value naming shared objects fails it instead.
using lane_layout::kProcessorHolds;

uint32_t processorRead(WasmLane* processor, uint32_t got)
{
  if (got != SharedObject::kCarriesHolds)
  {
    return got;
  }
  processor->fail(kProcessorHolds);
  return 0;
}

uint32_t sharedRegionCreateImpl(WasmScriptingVM* vm, uint32_t size)
{
  uint32_t refusal;
  uint64_t owner = sharedCreator(vm, size, false, false, refusal);
  if (owner == 0)
  {
    return refusal;
  }
  return sharedCreated(vm, owner, size, make_rcp<SharedRegion>(size));
}

uint32_t sharedRegionSizeImpl(WasmScriptingVM* vm, uint32_t region)
{
  auto found = sharedFind<SharedRegion>(vm, region);
  return found != nullptr ? found->size() : 0;
}

uint32_t sharedRegionLockImpl(
    WasmScriptingVM* vm,
    uint32_t region,
    double timeoutSeconds)
{
  auto found = sharedFind<SharedRegion>(vm, region);
  if (found == nullptr)
  {
    return shared_layout::kRegionLockGone;
  }
  // One region at a time keeps lock order acyclic.
  if (sharedHeldRegion() != 0)
  {
    return shared_layout::kRegionLockNested;
  }
  bool locked = false;
  {
    WasmScriptingVM::WaitScope wait(vm);
    // A lock begun in the time kept for the script gets nothing back, so
    // it spins for half of what is left, as a timed wait may oversleep.
    if (wait.usable() <= 0)
    {
      Clock::time_point until = deadlineAfter(
          std::min(timeoutSeconds, callSecondsLeft() / 2));
      spinUntil([&]() {
        locked = found->lock(0);
        return locked || Clock::now() >= until;
      });
    }
    else
    {
      locked = found->lock(timeoutSeconds);
    }
  }
  if (!locked)
  {
    return shared_layout::kRegionLockTimedOut;
  }
  setSharedHeldRegion(region, std::move(found));
  return shared_layout::kRegionLockAcquired;
}

void sharedRegionUnlockImpl(WasmScriptingVM* vm, uint32_t region)
{
  if (SharedRegion* held = sharedHeld(region))
  {
    held->unlock();
    setSharedHeldRegion(0, nullptr);
  }
}

uint32_t sharedRegionReadImpl(
    WasmScriptingVM* vm,
    uint32_t region,
    uint32_t offset,
    uint8_t* out,
    uint32_t outByteCount)
{
  SharedRegion* found = sharedHeld(region);
  if (found == nullptr || (uint64_t)offset + outByteCount > found->size())
  {
    return 0;
  }
  if (outByteCount != 0)
  {
    memcpy(out, found->bytes() + offset, outByteCount);
  }
  return 1;
}

uint32_t sharedRegionWriteImpl(
    WasmScriptingVM* vm,
    uint32_t region,
    uint32_t offset,
    const uint8_t* data,
    uint32_t dataByteCount)
{
  SharedRegion* found = sharedHeld(region);
  if (found == nullptr || (uint64_t)offset + dataByteCount > found->size())
  {
    return 0;
  }
  if (dataByteCount != 0)
  {
    memcpy(found->bytes() + offset, data, dataByteCount);
  }
  return 1;
}

uint32_t sharedRegionPoisonedImpl(WasmScriptingVM* vm, uint32_t region)
{
  auto found = sharedFind<SharedRegion>(vm, region);
  return found != nullptr && found->poisoned() ? 1 : 0;
}

// -1 for a word outside the region or a region gone, which the std throws
// for.
template <typename Use>
double sharedAtomic(
    WasmScriptingVM* vm,
    uint32_t region,
    uint32_t offset,
    Use use)
{
  auto found = sharedFind<SharedRegion>(vm, region);
  std::atomic<uint32_t>* word = found != nullptr ? found->word(offset)
                                                 : nullptr;
  return word != nullptr ? (double)use(*word) : -1.0;
}

double sharedAtomicLoadImpl(
    WasmScriptingVM* vm,
    uint32_t region,
    uint32_t offset)
{
  return sharedAtomic(vm, region, offset, [](std::atomic<uint32_t>& word) {
    return word.load();
  });
}

double sharedAtomicStoreImpl(
    WasmScriptingVM* vm,
    uint32_t region,
    uint32_t offset,
    uint32_t value)
{
  return sharedAtomic(vm, region, offset, [value](std::atomic<uint32_t>& word) {
    word.store(value);
    return 0u;
  });
}

double sharedAtomicAddImpl(
    WasmScriptingVM* vm,
    uint32_t region,
    uint32_t offset,
    uint32_t value)
{
  return sharedAtomic(vm, region, offset, [value](std::atomic<uint32_t>& word) {
    return word.fetch_add(value);
  });
}

double sharedAtomicCasImpl(
    WasmScriptingVM* vm,
    uint32_t region,
    uint32_t offset,
    uint32_t expected,
    uint32_t desired)
{
  return sharedAtomic(
      vm,
      region,
      offset,
      [expected, desired](std::atomic<uint32_t>& word) {
        uint32_t held = expected;
        compareExchangeStrong(word, held, desired);
        return held;
      });
}

// The holds a value in flight carries, retained here for it; an id already
// gone is left out.
#if RIVE_ONE_THREAD()
[[clang::no_destroy]] std::vector<uint32_t> t_carried;
#else
thread_local std::vector<uint32_t> t_carried;
#endif

Span<const uint32_t> retainCarried(uint64_t owner, Span<const uint32_t> ids)
{
  t_carried.clear();
  for (uint32_t id : ids)
  {
    if (SharedRegistry::retain(id, owner))
    {
      t_carried.push_back(id);
    }
  }
  return t_carried;
}

void releaseAll(uint64_t owner, Span<const uint32_t> ids)
{
  for (uint32_t id : ids)
  {
    SharedRegistry::release(id, owner);
  }
}

// The main lane keeps a read's holds while its std decodes; a lane keeps
// each until it ends or releases it.
void handHolds(WasmScriptingVM* vm, Span<const uint32_t> ids)
{
  if (ids.empty())
  {
    return;
  }
  if (vm != nullptr)
  {
    vm->keepReadHolds(ids);
  }
  else if (WasmLane* lane = callingLane(vm))
  {
    lane->adoptShared(ids);
  }
}

// What a read takes holds into before handing them over.
#if RIVE_ONE_THREAD()
[[clang::no_destroy]] std::vector<uint32_t> t_taken;
#else
thread_local std::vector<uint32_t> t_taken;
#endif

uint32_t sharedRingCreateImpl(WasmScriptingVM* vm, uint32_t capacity)
{
  uint32_t refusal;
  uint64_t owner = sharedCreator(vm, capacity, false, false, refusal);
  if (owner == 0)
  {
    return refusal;
  }
  auto ring = make_rcp<SharedRing>(capacity);
  if (!ring->made())
  {
    return sharedCreated(vm, owner, capacity, ring);
  }
  uint32_t data = SharedRegistry::add(ring->dataSignal(), owner);
  uint32_t space = SharedRegistry::add(ring->spaceSignal(), owner);
  ring->setSignalIds(data, space);
  // A signal that failed answers why.
  uint32_t id = isSharedId(data) ? space : data;
  if (isSharedId(id))
  {
    id = sharedCreated(vm, owner, capacity, ring);
  }
  else
  {
    SharedRegistry::refund(owner, capacity);
  }
  if (!isSharedId(id))
  {
    SharedRegistry::release(data, owner);
    SharedRegistry::release(space, owner);
  }
  return id;
}

uint32_t sharedRingWriteImpl(
    WasmScriptingVM* vm,
    uint32_t ring,
    const uint8_t* data,
    uint32_t dataByteCount,
    const uint32_t* holds,
    uint32_t holdCount)
{
  auto found = sharedFind<SharedRing>(vm, ring);
  if (found == nullptr)
  {
    return shared_layout::kRingWriteFull;
  }
  if (WasmLane* processor = callingProcessor(vm);
      processor != nullptr && holdCount != 0)
  {
    processor->fail(kProcessorHolds);
    return shared_layout::kRingWriteFull;
  }
  uint64_t owner = sharedHolder(vm);
  auto carried = retainCarried(owner, Span<const uint32_t>(holds, holdCount));
  if (found->write(Span<const uint8_t>(data, dataByteCount), carried))
  {
    return shared_layout::kRingWriteWritten;
  }
  releaseAll(owner, carried);
  uint64_t bytes = SharedRing::recordBytes(dataByteCount, holdCount);
  uint32_t capacity = found->block()->capacity;
  if (bytes > capacity)
  {
    if (sharedWarns(vm, *found))
    {
      sharedWarn(
          vm,
          "Ring.send, Ring.writeF32 or Port.post: a %u byte record and its "
          "%llu %s never fit this %u byte ring; create it larger, or send "
          "large data as a Blob, which crosses by id",
          dataByteCount,
          (unsigned long long)(bytes - dataByteCount),
          holdCount == 0 ? "byte length" : "bytes of length and holds",
          capacity);
    }
    return shared_layout::kRingWriteNeverFits;
  }
  return shared_layout::kRingWriteFull;
}

uint32_t sharedRingReadImpl(
    WasmScriptingVM* vm,
    uint32_t ring,
    uint8_t* out,
    uint32_t outByteCount)
{
  auto found = sharedFind<SharedRing>(vm, ring);
  if (found == nullptr)
  {
    return 0;
  }
  if (WasmLane* processor = callingProcessor(vm))
  {
    return processorRead(processor, found->read(out, outByteCount, nullptr));
  }
  t_taken.clear();
  uint32_t got = found->read(out, outByteCount, &t_taken);
  handHolds(vm, t_taken);
  return got;
}

uint32_t sharedRingDropImpl(
    WasmScriptingVM* vm,
    uint32_t ring,
    uint32_t capacity)
{
  auto found = sharedFind<SharedRing>(vm, ring);
  if (found == nullptr)
  {
    return 0;
  }
  if (WasmLane* processor = callingProcessor(vm))
  {
    return processorRead(processor, found->drop(capacity));
  }
  t_taken.clear();
  uint32_t dropped = found->drop(capacity, &t_taken);
  releaseAll(sharedHolder(vm), t_taken);
  return dropped;
}

uint32_t sharedRingUsedImpl(WasmScriptingVM* vm, uint32_t ring)
{
  auto found = sharedFind<SharedRing>(vm, ring);
  return found != nullptr ? found->used() : 0;
}

uint32_t sharedRingSignalImpl(
    WasmScriptingVM* vm,
    uint32_t ring,
    uint32_t space)
{
  auto found = sharedFind<SharedRing>(vm, ring);
  if (found == nullptr)
  {
    return 0;
  }
  return space != 0 ? found->spaceSignalId() : found->dataSignalId();
}

uint32_t sharedSnapshotCreateImpl(WasmScriptingVM* vm, uint32_t capacity)
{
  uint32_t refusal;
  uint64_t owner = sharedCreator(
      vm,
      (uint64_t)capacity * SharedSnapshot::kCopies,
      true,
      false,
      refusal);
  if (owner == 0)
  {
    return refusal;
  }
  return sharedCreated(
      vm,
      owner,
      (uint64_t)capacity * SharedSnapshot::kCopies,
      make_rcp<SharedSnapshot>(capacity));
}

uint32_t sharedSnapshotPublishImpl(
    WasmScriptingVM* vm,
    uint32_t snapshot,
    const uint8_t* data,
    uint32_t dataByteCount,
    const uint32_t* holds,
    uint32_t holdCount)
{
  auto found = sharedFind<SharedSnapshot>(vm, snapshot);
  if (found == nullptr)
  {
    return 0;
  }
  Span<const uint8_t> value(data, dataByteCount);
  if (WasmLane* processor = callingProcessor(vm))
  {
    if (holdCount == 0 && found->publish(value))
    {
      return 1;
    }
    if (holdCount != 0 || dataByteCount <= found->block()->capacity)
    {
      processor->fail(kProcessorHolds);
    }
    return 0;
  }
  uint64_t owner = sharedHolder(vm);
  auto carried = retainCarried(owner, Span<const uint32_t>(holds, holdCount));
  t_taken.clear();
  if (found->publish(value, carried, &t_taken))
  {
    releaseAll(owner, t_taken);
    return 1;
  }
  releaseAll(owner, carried);
  if (sharedWarns(vm, *found))
  {
    sharedWarn(
        vm,
        "Snapshot.publish: a %u byte value does not fit this %u byte "
        "snapshot, so readers keep the last value that fit; create the "
        "snapshot larger, or send large data as a Blob, which crosses by id",
        dataByteCount,
        found->block()->capacity);
  }
  return 0;
}

uint32_t sharedSnapshotReadImpl(
    WasmScriptingVM* vm,
    uint32_t snapshot,
    uint8_t* out,
    uint32_t outByteCount)
{
  auto found = sharedFind<SharedSnapshot>(vm, snapshot);
  if (found == nullptr)
  {
    return 0;
  }
  if (WasmLane* processor = callingProcessor(vm))
  {
    return processorRead(processor, found->read(out, outByteCount, nullptr));
  }
  uint64_t owner = sharedHolder(vm);
  // A publish gives back the holds of the value it replaced once seq moves,
  // so holds taken before seq moved are on live objects.
  for (;;)
  {
    uint64_t seq = found->seq();
    t_taken.clear();
    uint32_t got = found->read(out, outByteCount, &t_taken);
    auto taken = retainCarried(owner, t_taken);
    if (t_taken.empty() || found->seq() == seq)
    {
      handHolds(vm, taken);
      return got;
    }
    releaseAll(owner, taken);
  }
}

double sharedSnapshotSeqImpl(WasmScriptingVM* vm, uint32_t snapshot)
{
  auto found = sharedFind<SharedSnapshot>(vm, snapshot);
  return found != nullptr ? (double)found->seq() : 0.0;
}

uint32_t sharedSignalCreateImpl(WasmScriptingVM* vm)
{
  uint32_t refusal;
  uint64_t owner = sharedCreator(vm, 0, true, false, refusal);
  if (owner == 0)
  {
    return refusal;
  }
  return sharedCreated(vm, owner, 0, make_rcp<SharedSignal>());
}

void sharedSignalNotifyImpl(WasmScriptingVM* vm, uint32_t signal)
{
  if (auto found = sharedFind<SharedSignal>(vm, signal))
  {
    found->notify();
  }
}

double sharedSignalSeqImpl(WasmScriptingVM* vm, uint32_t signal)
{
  auto found = sharedFind<SharedSignal>(vm, signal);
  return found != nullptr ? (double)found->seq() : 0.0;
}

uint32_t sharedBlobCreateImpl(
    WasmScriptingVM* vm,
    const uint8_t* data,
    uint32_t dataByteCount)
{
  uint32_t refusal;
  uint64_t owner = sharedCreator(vm, dataByteCount, true, true, refusal);
  if (owner == 0)
  {
    return refusal;
  }
  return sharedCreated(
      vm,
      owner,
      dataByteCount,
      make_rcp<SharedBlob>(Span<const uint8_t>(data, dataByteCount)));
}

uint32_t sharedBlobSizeImpl(WasmScriptingVM* vm, uint32_t blob)
{
  auto found = sharedFind<SharedBlob>(vm, blob);
  return found != nullptr ? found->size() : 0;
}

uint32_t sharedBlobReadImpl(
    WasmScriptingVM* vm,
    uint32_t blob,
    uint32_t offset,
    uint8_t* out,
    uint32_t outByteCount)
{
  auto found = sharedFind<SharedBlob>(vm, blob);
  return found != nullptr ? found->read(offset, out, outByteCount) : 0;
}

void sharedReadDoneImpl(WasmScriptingVM* vm)
{
  if (vm != nullptr)
  {
    vm->releaseReadHolds();
  }
}

uint32_t sharedRetainImpl(WasmScriptingVM* vm, uint32_t object)
{
  return vm != nullptr && SharedRegistry::retain(object, vm->sharedOwner()) ? 1
                                                                            : 0;
}

// The main lane gives back a handle's hold; a lane only what it created or
// holds, and a processor nothing, as its holds go with it.
void sharedReleaseImpl(WasmScriptingVM* vm, uint32_t object)
{
  if (vm != nullptr)
  {
    SharedRegistry::release(object, vm->sharedOwner());
    return;
  }
  WasmLane* lane = WasmLane::current();
  if (lane != nullptr && lane->kind() != LaneKind::processor &&
      lane->dropShared(object))
  {
    SharedRegistry::release(object, lane->sharedOwner());
  }
}

// --- rive_lanes_v1: jobs, workers and audio processors -----------------------

struct HostLane
{
  virtual ~HostLane() = default;
  WasmLane* lane = nullptr;
};

template <class T> struct HostLaneOf : HostLane
{
  explicit HostLaneOf(rcp<T> held) : owned(std::move(held))
  {
    lane = owned.get();
  }
  rcp<T> owned;
};

WasmLane* resolveLane(WasmScriptingVM* vm, uint32_t handle)
{
  auto* host = vm != nullptr ? static_cast<HostLane*>(vm->handles().resolve(
                                   handle,
                                   WasmScriptingVM::HandleTable::Tag::lane))
                             : nullptr;
  return host != nullptr ? host->lane : nullptr;
}

WasmJob* resolveJob(WasmScriptingVM* vm, uint32_t handle)
{
  WasmLane* lane = resolveLane(vm, handle);
  return lane != nullptr && lane->kind() == LaneKind::job
             ? static_cast<WasmJob*>(lane)
             : nullptr;
}

// The encoded value a done job or worker hands its main lane.
const std::vector<uint8_t>& laneResult(WasmLane* lane)
{
  return lane->kind() == LaneKind::job
             ? static_cast<WasmJob*>(lane)->output()
             : static_cast<WasmWorker*>(lane)->result();
}

template <class T>
uint32_t mintLane(WasmScriptingVM* vm, rcp<T> lane, Span<const uint32_t> holds)
{
  if (lane == nullptr)
  {
    return 0;
  }
  // The script's handles outlive this call, so a lane already running
  // sees them alive until it holds them.
  lane->holdShared(holds);
  vm->watchLanes();
  return vm->handles().mint(
      WasmScriptingVM::HandleTable::Tag::lane,
      new HostLaneOf<T>(std::move(lane)));
}

uint32_t lanesStartImpl(
    WasmScriptingVM* vm,
    uint32_t kind,
    uint32_t entry,
    const uint8_t* input,
    uint32_t inputByteCount,
    const uint32_t* holds,
    uint32_t holdCount)
{
  if (vm == nullptr)
  {
    return 0;
  }
  if (kind < 3 && !vm->laneLoadError((LaneKind)kind).empty())
  {
    vm->refusedStart = (LaneKind)kind;
    return lane_layout::kStartFailed;
  }
  std::vector<uint8_t> bytes(input, input + inputByteCount);
  Span<const uint32_t> held(holds, holdCount);
  uint32_t pageBudgetMs = vm->pageThreadBudgetMs();
  switch ((LaneKind)kind)
  {
    case LaneKind::job:
      return vm->jobs() != nullptr
                 ? mintLane(
                       vm,
                       vm->jobs()->run(entry, std::move(bytes), pageBudgetMs),
                       held)
                 : 0;
    case LaneKind::worker:
      return vm->workers() != nullptr
                 ? mintLane(
                       vm,
                       vm->workers()
                           ->spawn(entry, std::move(bytes), pageBudgetMs),
                       held)
                 : 0;
    case LaneKind::processor:
      return vm->processors() != nullptr
                 ? mintLane(
                       vm,
                       vm->processors()->start(
                           entry,
                           std::move(bytes),
                           vm->limits().audioHardBlocks),
                       held)
                 : 0;
  }
  return 0;
}

uint32_t lanesStateImpl(WasmScriptingVM* vm, uint32_t lane)
{
  WasmLane* found = resolveLane(vm, lane);
  if (found == nullptr)
  {
    return (uint32_t)LaneState::stopped;
  }
  LaneState state = found->state();
  // What it printed lands ahead of whatever its ending makes the caller
  // print.
  if (state >= LaneState::done)
  {
    vm->deliverLaneOutput(found);
  }
  return (uint32_t)state;
}

uint32_t lanesFailedImpl(
    WasmScriptingVM* vm,
    uint32_t lane,
    char* buffer,
    uint32_t capacity)
{
  WasmLane* found = resolveLane(vm, lane);
  std::string failure;
  if (vm != nullptr && lane == lane_layout::kStartFailed)
  {
    failure = vm->laneLoadError(vm->refusedStart);
  }
  else if (found == nullptr || found->state() != LaneState::failed)
  {
    return 0;
  }
  else
  {
    failure = found->failure();
    if (failure.empty())
    {
      failure = found->name() + " failed";
    }
  }
  if (buffer != nullptr && failure.size() <= capacity)
  {
    memcpy(buffer, failure.data(), failure.size());
  }
  return (uint32_t)failure.size();
}

void lanesStopImpl(WasmScriptingVM* vm, uint32_t lane)
{
  if (WasmLane* found = resolveLane(vm, lane))
  {
    found->stop();
  }
}

void lanesReleaseImpl(WasmScriptingVM* vm, uint32_t lane)
{
  WasmLane* found = resolveLane(vm, lane);
  if (found == nullptr)
  {
    return;
  }
  // Now rather than at the next poll, so a call releasing many lanes keeps
  // none it is done with.
  vm->deliverLaneOutput(found);
  releaseHostHandle(vm, lane, WasmScriptingVM::HandleTable::Tag::lane);
  switch (found->kind())
  {
    case LaneKind::job:
      vm->jobs()->prune(found);
      break;
    case LaneKind::worker:
      vm->workers()->prune(found);
      break;
    case LaneKind::processor:
      vm->processors()->prune(found);
      break;
  }
}

uint32_t lanesResultImpl(
    WasmScriptingVM* vm,
    uint32_t lane,
    uint32_t out,
    uint32_t outCount)
{
  WasmLane* found = resolveLane(vm, lane);
  if (found == nullptr || found->state() != LaneState::done)
  {
    return 0;
  }
  const std::vector<uint8_t>& result = laneResult(found);
  if (result.size() <= outCount && !result.empty() &&
      !vm->copyToModule(out, result.data(), (uint32_t)result.size()))
  {
    vm->raiseModuleError("lane result buffer is outside module memory");
  }
  return (uint32_t)result.size();
}

uint32_t lanesWatchImpl(WasmScriptingVM* vm, uint32_t lane)
{
  return resolveLane(vm, lane) != nullptr ? vm->watchLane(lane) : 0;
}

uint32_t lanesWaitImpl(
    WasmScriptingVM* vm,
    uint32_t signal,
    double after,
    double timeoutSeconds)
{
  return vm != nullptr ? vm->waitOnMain(signal, after, timeoutSeconds) : 0;
}

void lanesCancelWaitImpl(WasmScriptingVM* vm, uint32_t token)
{
  if (vm != nullptr)
  {
    vm->cancelMainWait(token);
  }
}

double lanesWaitWithinImpl(
    WasmScriptingVM* vm,
    uint32_t signal,
    double after,
    double timeoutSeconds)
{
  if (vm == nullptr)
  {
    return lane_layout::kWaitGone;
  }
  // Waiting while holding a region could wait on a lane that waits for it.
  if (sharedHeldRegion() != 0)
  {
    return lane_layout::kWaitRefused;
  }
  // Without lanes nothing else notifies, and no call deadline is armed.
  double limit = vm->hasLanes()
                     ? std::chrono::duration<double>(waitLimit()).count()
                     : 0;
  uint64_t owner = vm->sharedOwner();
  WasmScriptingVM::WaitScope wait(vm);
  return SignalWait(
             0,
             owner,
             signal,
             SignalWait::find(signal, owner).get(),
             after,
             std::min({timeoutSeconds, wait.usable(), limit}))
      .block();
}

// --- rive_jobs_v1: script jobs ---------------------------------------------

uint32_t jobsThreadsImpl(WasmScriptingVM* vm)
{
  return WasmJobs::pool().threadCount();
}

uint32_t jobsJoinImpl(WasmScriptingVM* vm, uint32_t job, double timeoutSeconds)
{
  WasmJob* found = resolveJob(vm, job);
  if (found == nullptr)
  {
    return (uint32_t)LaneState::stopped;
  }
  // Waiting while holding a region could wait on a job that waits for it.
  if (sharedHeldRegion() != 0)
  {
    return WasmJob::kJoinRefused;
  }
  {
    WasmScriptingVM::WaitScope wait(vm);
    vm->jobs()->join(found, timeoutSeconds, wait.usable());
  }
  // Its lines land ahead of whatever the joiner prints next.
  vm->deliverLaneOutput(found);
  return (uint32_t)found->state();
}

uint32_t jobsJoinAnyImpl(
    WasmScriptingVM* vm,
    const uint32_t* jobs,
    uint32_t count,
    double timeoutSeconds)
{
  if (sharedHeldRegion() != 0)
  {
    return lane_layout::kJoinAnyRefused;
  }
  std::vector<WasmJob*> live;
  std::vector<uint32_t> indices;
  for (uint32_t i = 0; i < count; i++)
  {
    if (WasmJob* found = resolveJob(vm, jobs[i]))
    {
      live.push_back(found);
      indices.push_back(i);
    }
  }
  if (live.empty())
  {
    return 0;
  }
  int found;
  {
    WasmScriptingVM::WaitScope wait(vm);
    found = vm->jobs()->joinAny(live, timeoutSeconds, wait.usable());
  }
  if (found < 0)
  {
    return 0;
  }
  vm->deliverLaneOutput(live[found]);
  return indices[found] + 1;
}

void deleteHostObject(
    WasmScriptingVM* vm,
    WasmScriptingVM::HandleTable::Tag tag,
    void* object)
{
  using Tag = WasmScriptingVM::HandleTable::Tag;
  switch (tag)
  {
    case Tag::instanceValue:
      delete static_cast<HostInstanceValue*>(object);
      break;
    case Tag::viewModelInstance:
      delete static_cast<HostViewModelInstance*>(object);
      break;
    case Tag::path:
      delete static_cast<HostPath*>(object);
      break;
    case Tag::measure:
      delete static_cast<HostMeasure*>(object);
      break;
    case Tag::paint:
      delete static_cast<HostPaint*>(object);
      break;
    case Tag::shader:
      delete static_cast<HostShader*>(object);
      break;
    case Tag::image:
      delete static_cast<HostImage*>(object);
      break;
    case Tag::font:
      delete static_cast<HostFont*>(object);
      break;
    case Tag::text:
      deleteHostText(vm, static_cast<HostText*>(object));
      break;
    case Tag::buffer:
      delete static_cast<HostBuffer*>(object);
      break;
    case Tag::meshInstances:
      delete static_cast<HostMeshInstances*>(object);
      break;
    case Tag::dataContext:
      delete static_cast<HostDataContext*>(object);
      break;
    case Tag::artboard:
      // Refcounted: animations and nodes may still hold it.
      static_cast<HostArtboard*>(object)->unref();
      break;
    case Tag::animation:
      delete static_cast<HostAnimation*>(object);
      break;
    case Tag::node:
      delete static_cast<HostNode*>(object);
      break;
    case Tag::riveFile:
      delete static_cast<HostRiveFile*>(object);
      break;
    case Tag::bindableArtboard:
      delete static_cast<HostBindableArtboard*>(object);
      break;
#ifdef WITH_RIVE_AUDIO
    case Tag::audioSource:
      delete static_cast<HostAudioSource*>(object);
      break;
    case Tag::audioSound:
      delete static_cast<HostAudioSound*>(object);
      break;
#endif
#ifdef RIVE_CANVAS
    case Tag::canvas:
      delete static_cast<HostCanvas*>(object);
      break;
#endif
#if defined(RIVE_CANVAS) && defined(RIVE_ORE)
    case Tag::gpuPass:
      delete static_cast<HostGpuPass*>(object);
      break;
    case Tag::gpuCanvas:
      delete static_cast<HostGpuCanvas*>(object);
      break;
    case Tag::gpuBuffer:
      delete static_cast<HostGpuBuffer*>(object);
      break;
    case Tag::gpuTexture:
      delete static_cast<HostGpuTexture*>(object);
      break;
    case Tag::gpuSampler:
      delete static_cast<HostGpuSampler*>(object);
      break;
    case Tag::gpuTextureView:
      delete static_cast<HostGpuTextureView*>(object);
      break;
    case Tag::gpuShaderModule:
      delete static_cast<HostGpuShaderModule*>(object);
      break;
    case Tag::gpuBindGroupLayout:
      delete static_cast<HostGpuBindGroupLayout*>(object);
      break;
    case Tag::gpuBindGroup:
      delete static_cast<HostGpuBindGroup*>(object);
      break;
    case Tag::gpuPipeline:
      delete static_cast<HostGpuPipeline*>(object);
      break;
#endif
    case Tag::lane:
    {
      // A lane whose wrapper died is not waited for.
      auto* host = static_cast<HostLane*>(object);
      host->lane->stop();
      delete host;
      break;
    }
    default:
      break;
  }
}

} // namespace

uint32_t WasmScriptingVM::HandleTable::mint(Tag tag, void* object)
{
  uint32_t slot;
  if (!freeSlots.empty())
  {
    slot = freeSlots.back();
    freeSlots.pop_back();
  }
  else if (slots.size() < slotLimit)
  {
    slot = (uint32_t)slots.size();
    slots.push_back({});
  }
  else if (!retiredSlots.empty())
  {
    // Callers allocate before minting, so a full table reissues old
    // handles rather than fail.
    slot = retiredSlots.front();
    retiredSlots.pop_front();
  }
  else
  {
    return 0;
  }
  slots[slot].tag = tag;
  slots[slot].object = object;
  // Handle 0 is never valid; slots bias by one.
  return ((slot + 1) & 0xffffff) | ((uint32_t)slots[slot].generation << 24);
}

void* WasmScriptingVM::HandleTable::resolve(uint32_t handle, Tag tag) const
{
  uint32_t slot = (handle & 0xffffff);
  if (slot == 0 || slot > slots.size())
  {
    return nullptr;
  }
  const Slot& entry = slots[slot - 1];
  if (entry.tag != tag || entry.generation != (uint8_t)(handle >> 24))
  {
    return nullptr;
  }
  return entry.object;
}

void WasmScriptingVM::HandleTable::release(uint32_t handle, Tag tag)
{
  uint32_t slot = (handle & 0xffffff);
  if (slot == 0 || slot > slots.size())
  {
    return;
  }
  Slot& entry = slots[slot - 1];
  // Tag mismatch must not free the slot: a mistyped guest release would
  // stale the correctly-typed handle and leak its host wrapper.
  if (entry.generation != (uint8_t)(handle >> 24) || entry.tag != tag)
  {
    return;
  }
  entry.tag = Tag::empty;
  entry.object = nullptr;
  // Seven bits, so no handle sets bit 31, which marks shared ids.
  entry.generation = (entry.generation + 1) & 0x7f;
  uint32_t index = slot - 1;
  if (entry.generation == 0)
  {
    retiredSlots.push_back(index);
    if (retiredSlots.size() <= retiredLimit)
    {
      return;
    }
    index = retiredSlots.front();
    retiredSlots.pop_front();
  }
  freeSlots.push_back(index);
}

WasmScriptingVM::WasmScriptingVM() : m_decodedBy(s_decodingParent)
{
  if (m_decodedBy != nullptr)
  {
    m_decodedBy->m_decodedVMs.push_back(this);
  }
}

#ifndef __EMSCRIPTEN__
std::unique_ptr<WasmScriptingVM> WasmScriptingVM::make(
    Span<const uint8_t> module,
    Factory* factory,
    std::string& outError,
    std::function<void(const char*, size_t)> print)
{
  std::unique_ptr<WasmScriptingVM> vm(new WasmScriptingVM());
  vm->m_factory = factory;
  vm->m_print = std::move(print);
  if (s_bootHook)
  {
    s_bootHook(*vm, module);
  }
  if (!vm->init(module))
  {
    if (vm->m_error)
    {
      vm->m_error(kModuleStartFailed + vm->m_lastError);
    }
    else
    {
      outError = vm->m_lastError;
    }
    return nullptr;
  }
  return vm;
}
#endif

void WasmScriptingVM::callDraw(
    ScriptedObject* object,
    int selfRef,
    Renderer* renderer)
{
  // The handle is scoped to this call; releasing bumps the generation so a
  // stashed renderer goes stale instead of ghost drawing.
  HostRenderer host{renderer};
  uint32_t handle = m_handles.mint(HandleTable::Tag::renderer, &host);
  uint32_t args[3] = {m_L, (uint32_t)selfRef, handle};
  callModule("host_obj_draw", 3, args);
  host.restoreSaves();
  m_handles.release(handle, HandleTable::Tag::renderer);
}

WasmScriptingVM::~WasmScriptingVM()
{
  // First, so no poll reaches it while it goes.
  VMPollList::leave(m_laneList, this);
  VMPollList::leave(m_holdingList, this);
  // Workers and jobs may hold this VM's shared memory; they stop first.
  m_processors.reset();
  m_workers.reset();
  m_jobs.reset();
  if (m_sharedOwner != 0)
  {
    SharedRegistry::releaseOwner(m_sharedOwner);
  }
  if (m_decodedBy != nullptr)
  {
    auto& siblings = m_decodedBy->m_decodedVMs;
    siblings.erase(
        std::remove(siblings.begin(), siblings.end(), this),
        siblings.end());
  }
  for (WasmScriptingVM* decoded : m_decodedVMs)
  {
    decoded->m_decodedBy = nullptr;
  }
  if (m_debugHooks != nullptr)
  {
    m_debugHooks->onDetach(*this);
    m_debugHooks = nullptr;
  }
  // Flag in-flight decodes cancelled before teardown so a later poll
  // cannot call back into this dead VM, mirroring the Luau backend's
  // shutdownAsyncForState.
#ifdef __EMSCRIPTEN__
  for (const auto& pending : m_pendingDecodes)
  {
    cancelBrowserImageDecode(pending.second);
  }
#else
  if (m_decodeOwnerId != 0)
  {
    auto& pool = getGlobalWorkPoolIfExists();
    if (pool != nullptr)
    {
      pool->cancelAllForOwner(m_decodeOwnerId);
    }
  }
#endif
#ifdef WITH_RIVE_SCRIPTNET
  // Same for fetches: their listeners hold this VM.
  if (m_netOwnerId != 0)
  {
    scriptnet::cancelAllForOwner(m_netOwnerId);
  }
#endif
#ifndef __EMSCRIPTEN__
  // Coverage measurement: RIVE_WASM_EXEC_STATS=1 dumps how many frames ran
  // compiled, interpreted, and as guard-exit continuations.
  if (getenv("RIVE_WASM_EXEC_STATS") != nullptr && m_state != nullptr &&
      m_state->execEnv != nullptr)
  {
    uint32_t args[1] = {0};
    uint32_t compiled = callModule("host_exec_stats", 1, args);
    args[0] = 1;
    uint32_t interp = callModule("host_exec_stats", 1, args);
    args[0] = 2;
    uint32_t guardExits = callModule("host_exec_stats", 1, args);
    fprintf(
        stderr,
        "wasm exec stats: compiled=%u interp=%u guardExits=%u\n",
        compiled,
        interp,
        guardExits);
  }
#endif
  // No script runs from here: disposing the swept artboards' scripts would
  // mint handles into the table the sweep walks.
  m_poisoned = true;
  // The module never runs guest finalizers at teardown, so host objects
  // behind live handles are swept here; instance values in particular must
  // detach their delegates before the VM goes away.
  // Indexed, and each slot cleared before its object goes, because host
  // teardown may still mint and grow the table.
  for (size_t i = 0; i < m_handles.slots.size(); ++i)
  {
    HandleTable::Slot& slot = m_handles.slots[i];
    HandleTable::Tag tag = slot.tag;
    void* object = slot.object;
    slot.tag = HandleTable::Tag::empty;
    slot.object = nullptr;
    deleteHostObject(this, tag, object);
  }
#ifndef __EMSCRIPTEN__
  // Last, as the stats call above can still arm and register a slot.
  if (m_budgetSlot != nullptr)
  {
    ScriptWatchdog::instance().remove(m_budgetSlot.get());
  }
#endif
}

bool WasmScriptingVM::notifyDrawVisit(uint32_t drawable)
{
  uint32_t args[2] = {m_L, drawable};
  return valid() && callModule("host_draw_visit", 2, args) != 0;
}

void WasmScriptingVM::notifyDataValueChanged(uint32_t token)
{
  if (!valid())
  {
    return;
  }
  uint32_t args[2] = {m_L, token};
  callModule("host_data_value_changed", 2, args);
}

// --- context:decodeImage over the module ABI ---------------------------------

#if !defined(__EMSCRIPTEN__) && defined(RIVE_DECODERS)
namespace
{

// Mirrors the Luau backend's ImageDecodeTask (lua_image_decode.cpp): same
// Bitmap::decode, same premultiply, same error string, same WorkPool, so the
// wasm lane settles on the same advance as the Luau lane.
class WasmImageDecodeTask : public WorkTask
{
public:
  std::vector<uint8_t> m_encodedData;
  WasmScriptingVM* m_vm = nullptr;
  uint32_t m_token = 0;
  std::unique_ptr<Bitmap> m_bitmap;

  bool execute() override
  {
    m_bitmap = Bitmap::decode(m_encodedData.data(), m_encodedData.size());
    if (!m_bitmap)
    {
      m_errorMessage = "failed to decode image data";
      return false;
    }
    if (m_bitmap->pixelFormat() != Bitmap::PixelFormat::RGBAPremul)
    {
      m_bitmap->pixelFormat(Bitmap::PixelFormat::RGBAPremul);
    }
    return true;
  }

  void onComplete() override
  {
    if (m_vm != nullptr)
    {
      m_vm->resolveImageDecode(
          m_token,
          m_bitmap->width(),
          m_bitmap->height(),
          m_bitmap->bytes(),
          m_bitmap->width() * m_bitmap->height() * 4);
    }
    m_bitmap.reset();
    m_encodedData.clear();
    m_encodedData.shrink_to_fit();
  }

  void onError(const std::string& error) override
  {
    if (m_vm != nullptr)
    {
      m_vm->rejectImageDecode(m_token, error.c_str());
    }
  }

  void onCancel() override { m_vm = nullptr; }
};

} // namespace
#endif // RIVE_DECODERS

bool WasmScriptingVM::startImageDecode(
    const uint8_t* bytes,
    uint32_t byteCount,
    uint32_t token)
{
#ifdef __EMSCRIPTEN__
  // Like the Luau lane on the web, the browser decodes.
  m_pendingDecodes[token] = startBrowserImageDecode(
      bytes,
      byteCount,
      [this, token](
          uint32_t,
          uint32_t width,
          uint32_t height,
          Span<const uint8_t> pixels,
          const char* error) {
        if (error != nullptr)
        {
          rejectImageDecode(token, error);
          return;
        }
        resolveImageDecode(
            token,
            width,
            height,
            pixels.data(),
            (uint32_t)pixels.size());
      });
  return true;
#elif !defined(RIVE_DECODERS)
  return false;
#else
  if (m_decodeOwnerId == 0)
  {
    m_decodeOwnerId = WorkPool::nextOwnerId();
  }
  auto task = make_rcp<WasmImageDecodeTask>();
  task->m_encodedData.assign(bytes, bytes + byteCount);
  task->m_vm = this;
  task->m_token = token;
  task->setOwnerId(m_decodeOwnerId);
  m_pendingDecodes[token] = task;
  getGlobalWorkPool()->submit(std::move(task));
  return true;
#endif
}

void WasmScriptingVM::cancelImageDecode(uint32_t token)
{
  auto it = m_pendingDecodes.find(token);
  if (it != m_pendingDecodes.end())
  {
#ifdef __EMSCRIPTEN__
    cancelBrowserImageDecode(it->second);
#else
    it->second->cancel();
#endif
    m_pendingDecodes.erase(it);
  }
}

void WasmScriptingVM::deliverDecodeResult(const DecodeResult& result)
{
  if (result.ok)
  {
    uint32_t byteCount = (uint32_t)result.pixels.size();
    // The listener's own buffer, where the module hands one out, saves
    // a second copy as big as the image.
    uint32_t bufferArgs[2] = {m_L, byteCount};
    uint32_t pixelsPtr = 0;
    bool staged = callModuleChecked(
                      "host_image_buffer",
                      2,
                      bufferArgs,
                      &pixelsPtr) != CallOutcome::ok;
    if (staged)
    {
      uint32_t sizeArgs[1] = {byteCount};
      pixelsPtr = callModule("malloc", 1, sizeArgs);
    }
    if (pixelsPtr == 0)
    {
      DecodeResult failure;
      failure.token = result.token;
      failure.error = "failed to allocate decoded pixels";
      deliverDecodeResult(failure);
      return;
    }
    copyToModule(pixelsPtr, result.pixels.data(), byteCount);
    uint32_t args[6] =
        {m_L, result.token, result.width, result.height, pixelsPtr, byteCount};
    callModule("host_image_decoded", 6, args);
    if (staged)
    {
      guestFree(pixelsPtr);
    }
    return;
  }
  uint32_t messagePtr = guestString(result.error.c_str());
  if (messagePtr == 0)
  {
    return;
  }
  uint32_t args[3] = {m_L, result.token, messagePtr};
  callModule("host_image_decode_failed", 3, args);
  guestFree(messagePtr);
}

void WasmScriptingVM::resolveImageDecode(
    uint32_t token,
    uint32_t width,
    uint32_t height,
    const uint8_t* pixels,
    uint32_t byteCount)
{
  if (inModuleCall())
  {
    // The decoder frees its pixels once this returns.
    std::vector<uint8_t> owned(pixels, pixels + byteCount);
    holdOutcome([this, token, width, height, owned = std::move(owned)] {
      resolveImageDecode(
          token,
          width,
          height,
          owned.data(),
          (uint32_t)owned.size());
    });
    return;
  }
  if (m_pendingDecodes.erase(token) == 0 || !valid())
  {
    return;
  }
  DecodeResult result;
  result.ok = true;
  result.token = token;
  result.width = width;
  result.height = height;
  result.pixels = Span<const uint8_t>(pixels, byteCount);
  deliverDecodeResult(result);
}

void WasmScriptingVM::rejectImageDecode(uint32_t token, const char* message)
{
  if (inModuleCall())
  {
    holdOutcome([this, token, error = std::string(message)] {
      rejectImageDecode(token, error.c_str());
    });
    return;
  }
  if (m_pendingDecodes.erase(token) == 0 || !valid())
  {
    return;
  }
  DecodeResult result;
  result.token = token;
  result.error = message;
  deliverDecodeResult(result);
}

// --- fetch() over the module ABI --------------------------------------------

#ifdef WITH_RIVE_SCRIPTNET
namespace
{
// The Luau lane's rejection value: "<code>: <message>".
std::string fetchRejection(
    scriptnet::NetErrorCode code,
    const std::string& message)
{
  return std::string(scriptnet::netErrorCodeName(code)) + ": " + message;
}

// Carries one request's outcome back to the VM that started it. scriptnet
// delivers on this VM's thread from its poll; teardown cancels every
// outstanding request first, which clears the VM pointer here.
class WasmFetchListener : public scriptnet::FetchListener
{
public:
  WasmFetchListener(WasmScriptingVM* vm, uint32_t token)
      : m_vm(vm)
      , m_token(token)
  {}

  void onResponse(scriptnet::HttpResponse&& response) override
  {
    if (m_vm != nullptr)
    {
      m_vm->resolveFetch(m_token, std::move(response));
    }
  }

  void onError(const scriptnet::NetError& error) override
  {
    if (m_vm != nullptr)
    {
      m_vm->rejectFetch(
          m_token,
          (uint32_t)error.code,
          fetchRejection(error.code, error.message));
    }
  }

  void onCancel() override { m_vm = nullptr; }

private:
  WasmScriptingVM* m_vm;
  uint32_t m_token;
};
} // namespace
#endif

bool WasmScriptingVM::startFetch(Span<const uint8_t> wire, uint32_t token)
{
#ifdef WITH_RIVE_SCRIPTNET
  // Reusing a live token would leave its first request uncancellable.
  if (m_pendingFetches.count(token) != 0)
  {
    return false;
  }
  scriptnet::HttpRequest request;
  // The module's fetch() writes the stream from arguments it validated, so
  // a bad one is a bug; it goes out empty and the policy refuses it,
  // settling like any other refusal.
  if (!NetWire::decode(wire, request))
  {
    request = scriptnet::HttpRequest();
  }
  if (m_netOwnerId == 0)
  {
    m_netOwnerId = WorkPool::nextOwnerId();
  }
  m_pendingFetches[token] = scriptnet::fetch(
      m_netOwnerId,
      std::move(request),
      make_rcp<WasmFetchListener>(this, token));
  return true;
#else
  (void)wire;
  (void)token;
  return false;
#endif
}

void WasmScriptingVM::cancelFetch(uint32_t token)
{
  auto it = m_pendingFetches.find(token);
  if (it == m_pendingFetches.end())
  {
    return;
  }
  uint32_t id = it->second;
  m_pendingFetches.erase(it);
#ifdef WITH_RIVE_SCRIPTNET
  scriptnet::cancel(id);
#else
  (void)id;
#endif
}

void WasmScriptingVM::resolveFetch(
    uint32_t token,
    scriptnet::HttpResponse&& response)
{
  if (inModuleCall())
  {
    holdOutcome([this, token, response = std::move(response)]() mutable {
      resolveFetch(token, std::move(response));
    });
    return;
  }
  // Cancelled while held.
  if (m_pendingFetches.erase(token) == 0 || !valid())
  {
    return;
  }
  auto tooLarge = [&](const char* message) {
#ifdef WITH_RIVE_SCRIPTNET
    deliverFetchFailure(
        token,
        (uint32_t)scriptnet::NetErrorCode::TooLarge,
        fetchRejection(scriptnet::NetErrorCode::TooLarge, message));
#else
    (void)message;
#endif
  };
  // The body stays in the host, so it reaches a lane without ever entering
  // module memory.
  uint32_t body = 0;
  if (m_fetchBlobBody)
  {
    uint64_t size = response.body.size();
    uint64_t owner = sharedCreator(this, size, true, true, body);
    if (owner != 0)
    {
      body = sharedCreated(
          this,
          owner,
          size,
          make_rcp<SharedBlob>(
              Span<const uint8_t>(response.body.data(), response.body.size())));
    }
    if (!isSharedId(body))
    {
      tooLarge("the response body does not fit in shared memory");
      return;
    }
  }
  NetWire::Response wire{response, body};
  uint32_t byteCount = (uint32_t)NetWire::encodedSize(wire);
  uint32_t sizeArgs[1] = {byteCount};
  uint32_t responsePtr = callModule("malloc", 1, sizeArgs);
  if (responsePtr == 0)
  {
    if (body != 0)
    {
      SharedRegistry::release(body, sharedOwner());
    }
    tooLarge("the response does not fit in script memory");
    return;
  }
  NetWire::encode(
      wire,
      (uint8_t*)resolveModuleWritePtr(responsePtr, byteCount));
  uint32_t args[4] = {m_L, token, responsePtr, byteCount};
  callModule("host_fetch_resolved", 4, args);
  guestFree(responsePtr);
}

void WasmScriptingVM::rejectFetch(
    uint32_t token,
    uint32_t code,
    const std::string& message)
{
  if (inModuleCall())
  {
    holdOutcome(
        [this, token, code, message] { rejectFetch(token, code, message); });
    return;
  }
  if (m_pendingFetches.erase(token) != 0 && valid())
  {
    deliverFetchFailure(token, code, message);
  }
}

void WasmScriptingVM::deliverFetchFailure(
    uint32_t token,
    uint32_t code,
    const std::string& message)
{
  uint32_t messagePtr = guestString(message.c_str());
  if (messagePtr == 0)
  {
    return;
  }
  uint32_t args[4] = {m_L, token, code, messagePtr};
  callModule("host_fetch_failed", 4, args);
  guestFree(messagePtr);
}

bool WasmScriptingVM::inModuleCall() const { return m_callDepth != 0; }

void WasmScriptingVM::holdOutcome(std::function<void()> deliver)
{
  if (m_heldOutcomes.empty())
  {
    VMPollList::join(m_holdingList, t_holdingVMs, this);
  }
  m_heldOutcomes.push_back(std::move(deliver));
}

void WasmScriptingVM::attachLanes(Span<const uint8_t> module)
{
  // Not generic, so the three kinds share one copy.
  auto load = [&](LaneKind kind) {
    std::string& error = m_laneLoadErrors[(int)kind];
    rcp<WasmLaneModule> lanes = loadLaneModule(module, kind, error);
    if (!error.empty())
    {
      consoleWrite(
          this,
          std::string("script ") + laneNoun(kind) + "s unavailable: " + error);
    }
    return lanes;
  };
  auto attach = [&](auto& set, LaneKind kind) {
    rcp<WasmLaneModule> lanes = load(kind);
    if (lanes != nullptr)
    {
      set.reset(new std::remove_reference_t<decltype(*set)>(std::move(lanes)));
      set->setSharedOwner(sharedOwner());
    }
  };
  attach(m_jobs, LaneKind::job);
  attach(m_workers, LaneKind::worker);
  attach(m_processors, LaneKind::processor);
}

void WasmScriptingVM::laneTierLines(std::vector<std::string>& lines) const
{
#ifndef __EMSCRIPTEN__
  auto add = [&](const auto& lanes, LaneKind kind) {
    if (lanes != nullptr)
    {
      lines.push_back(
          std::string(laneSpec(kind).module) + " tier: " + lanes->tier());
    }
  };
  add(m_jobs, LaneKind::job);
  add(m_workers, LaneKind::worker);
  add(m_processors, LaneKind::processor);
#endif
}

void WasmScriptingVM::poison()
{
  m_poisoned = true;
  auto stopAll = [](auto* set) {
    if (set == nullptr)
    {
      return;
    }
    for (const auto& lane : set->lanes())
    {
      lane->stop();
    }
  };
  stopAll(m_jobs.get());
  stopAll(m_workers.get());
  stopAll(m_processors.get());
}

void WasmScriptingVM::deliverAllLaneOutput()
{
  t_laneVMs->deliver([](WasmScriptingVM* vm) {
    // At a poll no module call is live, so workers without threads may
    // run here.
    if (vm->m_workers != nullptr)
    {
      vm->m_workers->pump();
    }
    vm->deliverLaneOutput();
  });
}

void WasmScriptingVM::watchLanes()
{
  // Its lanes start only from one thread, whose polls reach their output.
  assert(m_laneList == nullptr || m_laneList == t_laneVMs);
  VMPollList::join(m_laneList, t_laneVMs, this);
}

uint32_t WasmScriptingVM::watchLane(uint32_t lane)
{
  m_mainWaits.push_back({SignalWait(m_nextMainWait), lane});
  return m_nextMainWait++;
}

uint32_t WasmScriptingVM::waitOnMain(
    uint32_t signal,
    double after,
    double timeoutSeconds)
{
  watchLanes();
  uint64_t owner = sharedOwner();
  rcp<SharedSignal> found = SignalWait::find(signal, owner);
  m_mainWaits.push_back(
      {SignalWait(
           m_nextMainWait,
           owner,
           signal,
           found.get(),
           after,
           timeoutSeconds),
       0});
  return m_nextMainWait++;
}

void WasmScriptingVM::cancelMainWait(uint32_t token)
{
  m_mainWaits.erase(
      std::remove_if(
          m_mainWaits.begin(),
          m_mainWaits.end(),
          [token](const MainWait& wait) { return wait.wait.id == token; }),
      m_mainWaits.end());
}

void WasmScriptingVM::settleMainWaits()
{
  auto now = std::chrono::steady_clock::now();
  size_t resultBytes = 0;
  for (auto it = m_mainWaits.begin(); it != m_mainWaits.end();)
  {
    double value = 0;
    bool fire = false;
    if (it->lane != 0)
    {
      WasmLane* lane = resolveLane(this, it->lane);
      LaneState state = lane != nullptr ? lane->state() : LaneState::stopped;
      fire = state >= LaneState::done;
      value = (double)state;
      // Each result is decoded into the main heap as it wakes, so a
      // burst of large ones spreads over polls.
      if (fire && state == LaneState::done)
      {
        size_t bytes = laneResult(lane).size();
        if (resultBytes != 0 && resultBytes + bytes > kLaneResultBytesPerPoll)
        {
          ++it;
          continue;
        }
        resultBytes += bytes;
      }
    }
    else
    {
      fire = it->wait.fired(now, value);
    }
    if (!fire)
    {
      ++it;
      continue;
    }
    uint32_t token = it->wait.id;
    it = m_mainWaits.erase(it);
    // Held even outside a call, so an await always resumes at a poll.
    holdOutcome([this, token, value] {
      double args[3] = {(double)m_L, (double)token, value};
      double unused = 0;
      callF64("host_lane_wake", args, 3, &unused);
    });
  }
}

void WasmScriptingVM::deliverLaneOutput(WasmLane* lane)
{
  lane->takeLogs(
      [](void* vm, std::string_view line) {
        rtLogImpl((WasmScriptingVM*)vm, 0, line.data(), (uint32_t)line.size());
      },
      this);
  // A job's failure is its joiner's result, so only workers and processors
  // announce theirs.
  if (lane->kind() == LaneKind::job || lane->reportedFailure ||
      lane->state() != LaneState::failed)
  {
    return;
  }
  lane->reportedFailure = true;
  std::string line = lane->name() + " failed: " + lane->failure();
  if (m_error)
  {
    m_error(line);
  }
  else
  {
    consoleWrite(this, line);
  }
  // A failed processor renders only silence, so it leaves the mix.
  lane->stop();
}

void WasmScriptingVM::deliverLaneOutput()
{
  settleMainWaits();
  auto deliver = [this](auto* set) {
    if (set == nullptr)
    {
      return;
    }
    for (const auto& lane : set->lanes())
    {
      deliverLaneOutput(lane.get());
    }
    // Their output is out, and an idle slot keeps the watchdog polling.
    set->prune();
  };
  deliver(m_jobs.get());
  deliver(m_workers.get());
  deliver(m_processors.get());
}

bool WasmScriptingVM::hasPendingAsyncWork() const
{
  // A started job keeps hosts ticking until the script can see it finish,
  // and a worker only while it needs a step, never for being alive.
  return !m_pendingDecodes.empty() || !m_pendingFetches.empty() ||
         !m_heldOutcomes.empty() || !m_mainWaits.empty() ||
         (m_jobs != nullptr && m_jobs->hasUnfinished()) ||
         (m_workers != nullptr && m_workers->due()) ||
         (m_processors != nullptr && m_processors->starting());
}

uint64_t WasmScriptingVM::sharedOwner()
{
  if (m_sharedOwner == 0)
  {
    m_sharedOwner = SharedRegistry::nextOwner(m_limits.sharedBytesPerFile);
  }
  return m_sharedOwner;
}

void WasmScriptingVM::keepReadHolds(Span<const uint32_t> ids)
{
  m_readHolds.insert(m_readHolds.end(), ids.begin(), ids.end());
}

void WasmScriptingVM::releaseReadHolds()
{
  for (uint32_t id : m_readHolds)
  {
    SharedRegistry::release(id, m_sharedOwner);
  }
  m_readHolds.clear();
}

void WasmScriptingVM::deliverHeldOutcomes()
{
  if (inModuleCall() || m_heldOutcomes.empty())
  {
    return;
  }
  VMPollList::leave(m_holdingList, this);
  // Deliveries can hold more, which wait for the next pass.
  std::vector<std::function<void()>> held;
  held.swap(m_heldOutcomes);
  for (auto& deliver : held)
  {
    deliver();
  }
}

void WasmScriptingVM::deliverAllHeldOutcomes()
{
  t_holdingVMs->deliver([](WasmScriptingVM* vm) { vm->deliverHeldOutcomes(); });
}

void WasmScriptingVM::setBootHook(BootHook hook)
{
  s_bootHook = std::move(hook);
}

void WasmScriptingVM::requireBounds(bool on) { s_requireBounds.store(on); }

void WasmScriptingVM::setTimeoutMs(int ms)
{
  m_timeoutMs = ms;
  if (valid())
  {
    uint32_t args[2] = {m_L, (uint32_t)ms};
    callModule("host_set_timeout", 2, args);
  }
}

std::string WasmScriptingVM::tierName(
    ExecutionTier tier,
    const std::string& interpReason)
{
  switch (tier)
  {
    case ExecutionTier::interp:
      return interpReason.empty() ? "interp" : "interp, " + interpReason;
    case ExecutionTier::aotO0:
      return "aot -O0";
    case ExecutionTier::aotO3:
      return "aot -O3";
  }
  return "unknown";
}

#ifndef __EMSCRIPTEN__
std::shared_ptr<void> WasmScriptingVM::loadSharedModule(
    Span<const uint8_t> bytes,
    ExecutionTier& tier,
    std::string& interpReason,
    std::string& outError)
{
  if (!ensureRuntime())
  {
    outError = "wamr runtime init failed";
    return nullptr;
  }
  // One content hash serves the AOT artifact lookups and the shared module
  // cache key; the key folds in which artifact actually loads so interp
  // and AOT modules never share an entry.
  const uint64_t plainKey = wasmModuleKey(bytes);
  uint64_t moduleKey = plainKey;
  tier = ExecutionTier::interp;
  // wasm_runtime_load keeps referencing the buffer, and may rewrite it.
  std::vector<uint8_t> moduleBytes;
  wasm_module_t loaded = nullptr;
  char error[256] = {0};
  // Dev AOT lane: with RIVE_WASM_AOT_CACHE set, swap in a wamrc-compiled
  // artifact when one exists for these bytes, else dump the wasm so it can
  // be compiled offline. wasm_runtime_load sniffs the AOT magic.
  char aotPath[1024] = {0};
  bool haveAot = false;
  bool haveHwAot = false;
  bool haveO0Aot = false;
  auto setCompiledSpecies = [&](TierSpecies species) {
    haveHwAot = species == TierSpecies::hw;
    haveO0Aot = species == TierSpecies::o0;
    haveAot = !haveHwAot && !haveO0Aot;
  };
  if (const char* cacheDir = getenv("RIVE_WASM_AOT_CACHE"))
  {
#ifdef RIVE_WASM_HW_BOUNDS
    // Hw artifacts (wamrc --bounds-checks=0 --stack-bounds-checks=1)
    // rely on the guard-page trap handler, so only builds carrying it
    // ever probe the .hw.aot name; sw-only builds cannot load them.
    snprintf(
        aotPath,
        sizeof(aotPath),
        "%s/%s",
        cacheDir,
        aotArtifactName(plainKey, ".hw").c_str());
    struct stat hwAotStat;
    haveHwAot = stat(aotPath, &hwAotStat) == 0;
#endif
    if (!haveHwAot)
    {
      snprintf(
          aotPath,
          sizeof(aotPath),
          "%s/%s",
          cacheDir,
          aotArtifactName(plainKey).c_str());
      struct stat aotStat;
      haveAot = stat(aotPath, &aotStat) == 0;
    }
    if (!haveAot && !haveHwAot)
    {
      char dumpPath[1024];
      snprintf(
          dumpPath,
          sizeof(dumpPath),
          "%s/%s",
          cacheDir,
          wasmModuleName(plainKey).c_str());
      struct stat dumpStat;
      if (stat(dumpPath, &dumpStat) != 0)
      {
        if (FILE* dump = fopen(dumpPath, "wb"))
        {
          fwrite(bytes.data(), 1, bytes.size(), dump);
          fclose(dump);
          fprintf(stderr, "wasm aot: dumped %s for wamrc\n", dumpPath);
        }
      }
    }
  }
  // Tools builds boot compiled code: a ladder cache hit loads its
  // artifact and a miss compiles one on this thread, blocking the load.
  // RIVE_WASM_AOT_SYNC=o0 asks for the faster debug compile and =interp
  // for no compile at all.
  const char* tierEnv = getenv("RIVE_WASM_AOT_SYNC");
  bool wantInterp = tierEnv != nullptr && strcmp(tierEnv, "interp") == 0;
  TierSpecies species = bootTierSpecies();
  bool ladderArtifact = false;
  if (!haveAot && !haveHwAot)
  {
    if (wantInterp)
    {
      interpReason = "RIVE_WASM_AOT_SYNC=interp";
    }
    else if (
        auto cached = leaseCachedModule(
            compiledModuleKey(plainKey, species),
            tier))
    {
      interpReason.clear();
      return cached;
    }
    else
    {
      TierCompile compile = ModuleTierLadder::instance().bootCompile(
          plainKey,
          bytes,
          species);
      if (!compile.path.empty() && compile.path.size() < sizeof(aotPath))
      {
        memcpy(aotPath, compile.path.c_str(), compile.path.size() + 1);
        setCompiledSpecies(species);
        ladderArtifact = true;
      }
      else
      {
        interpReason = compile.reason;
      }
    }
  }
#if WASM_ENABLE_PRELINKED_AOT != 0
  // Offline-compiled code linked into this binary; the only AOT shape on
  // platforms without runtime executable memory. The file lanes above never
  // fire there, but an explicit artifact still wins if one appears.
  const PrelinkedAotModule* prelinked = haveAot || haveHwAot || haveO0Aot
                                            ? nullptr
                                            : findPrelinkedAotModule(
                                                  plainKey,
                                                  bytes.size());
  if (prelinked != nullptr)
  {
    moduleKey ^= 0x94d049bb133111ebull;
  }
#endif
  if (haveAot || haveHwAot || haveO0Aot)
  {
    moduleKey = compiledModuleKey(
        moduleKey,
        haveHwAot   ? TierSpecies::hw
        : haveO0Aot ? TierSpecies::o0
                    : TierSpecies::o3);
  }
  bool pinned = false;
  std::shared_ptr<void> lease = leaseCachedModule(moduleKey, tier);
  if (lease == nullptr)
  {
    if (haveAot || haveHwAot || haveO0Aot)
    {
      if (FILE* aot = fopen(aotPath, "rb"))
      {
        fseek(aot, 0, SEEK_END);
        long size = ftell(aot);
        fseek(aot, 0, SEEK_SET);
        moduleBytes.resize(size);
        size_t read = fread(moduleBytes.data(), 1, size, aot);
        fclose(aot);
        fprintf(stderr, "wasm aot: loaded %s (%zu bytes)\n", aotPath, read);
        tier = haveO0Aot ? ExecutionTier::aotO0 : ExecutionTier::aotO3;
      }
    }
    else
    {
      moduleBytes.assign(bytes.begin(), bytes.end());
    }
#if WASM_ENABLE_PRELINKED_AOT != 0
    if (prelinked != nullptr)
    {
      loaded = (wasm_module_t)
          loadPrelinkedAotModule(*prelinked, error, sizeof(error));
      if (loaded != nullptr)
      {
        tier = ExecutionTier::aotO3;
        pinned = true;
        fprintf(
            stderr,
            "wasm aot: prelinked module %016llx (%zu byte text)\n",
            (unsigned long long)plainKey,
            prelinked->textSize);
      }
      else
      {
        // A stale or incompatible bake must not fail the file; drop
        // to the wasm bytes under the plain key.
        fprintf(
            stderr,
            "wasm aot: prelinked module rejected (%s); "
            "falling back\n",
            error);
        moduleKey = plainKey;
        lease = leaseCachedModule(moduleKey, tier);
      }
    }
    if (loaded == nullptr && lease == nullptr)
#endif
    {
      auto loadBytes = [&]() {
        loaded = wasm_runtime_load(
            moduleBytes.data(),
            (uint32_t)moduleBytes.size(),
            error,
            sizeof(error));
      };
      loadBytes();
      if (loaded == nullptr && (haveAot || haveHwAot || haveO0Aot))
      {
        // A stale artifact, say from another WAMR, must not fail the
        // file; drop to the wasm bytes under the plain key.
        fprintf(
            stderr,
            "wasm aot: %s rejected (%s); falling back\n",
            aotPath,
            error);
        haveAot = haveHwAot = haveO0Aot = false;
        tier = ExecutionTier::interp;
        interpReason = std::string("aot artifact rejected, ") + error;
        // Every later load would read and reject it again.
        if (ladderArtifact)
        {
          ModuleTierLadder::instance().rejectArtifact(
              plainKey,
              species,
              interpReason);
        }
        else
        {
          remove(aotPath);
        }
        moduleKey = plainKey;
        lease = leaseCachedModule(moduleKey, tier);
        if (lease == nullptr)
        {
          moduleBytes.assign(bytes.begin(), bytes.end());
          loadBytes();
        }
      }
    }
    if (loaded != nullptr)
    {
      if (haveHwAot)
      {
        wasm_runtime_set_module_hw_bounds(loaded, true);
      }
      auto entry = std::make_shared<SharedWasmModule>();
      entry->bytes = std::move(moduleBytes);
      entry->module = loaded;
      entry->tier = tier;
      entry->pinned = pinned;
      // Like entry, declared before the lock so modules unload once it
      // is released, where they stall no other boot.
      std::vector<std::shared_ptr<SharedWasmModule>> evicted;
      std::lock_guard<std::mutex> lock(sharedModuleCacheMutex());
      auto inserted = sharedModuleCache().try_emplace(moduleKey, entry);
      if (inserted.second)
      {
        lease = leaseLocked(entry);
        evictIdleModulesLocked(evicted);
      }
      else
      {
        // Another thread loaded the same bytes first, so entry
        // unloads this copy.
        tier = inserted.first->second->tier;
        lease = leaseLocked(inserted.first->second);
      }
    }
  }
  if (lease == nullptr)
  {
    outError = error;
    return nullptr;
  }
  // Prelinked code runs compiled whatever the ladder said.
  if (tier != ExecutionTier::interp)
  {
    interpReason.clear();
  }
  return lease;
}

bool WasmScriptingVM::init(Span<const uint8_t> module)
{
  // Unsigned modules are safe only as bytecode WAMR compiles itself; AOT
  // comes solely from our own artifacts, found by this bytecode's hash.
  if (module.size() < 4 || memcmp(module.data(), "\0asm", 4) != 0)
  {
    m_lastError = "script module is not wasm bytecode";
    return false;
  }

  m_state = std::make_unique<WamrState>();
  m_state
      ->lease = loadSharedModule(module, m_tier, m_interpReason, m_lastError);
  m_state->module = (wasm_module_t)m_state->lease.get();
  if (m_state->module == nullptr)
  {
    m_lastError = "module load failed: " + m_lastError;
    return false;
  }
  // Unresolved function imports link fine and trap only when first called,
  // with no diagnostic; report them here where the failure is actionable.
  int32_t importCount = wasm_runtime_get_import_count(m_state->module);
  for (int32_t i = 0; i < importCount; i++)
  {
    wasm_import_t import;
    wasm_runtime_get_import_type(m_state->module, i, &import);
    if (import.kind == WASM_IMPORT_EXPORT_KIND_FUNC && !import.linked)
    {
      m_unresolvedImports.push_back(
          std::string(import.module_name) + "." + import.name);
    }
  }
  int32_t exportCount = wasm_runtime_get_export_count(m_state->module);
  for (int32_t i = 0; i < exportCount; i++)
  {
    wasm_export_t moduleExport;
    wasm_runtime_get_export_type(m_state->module, i, &moduleExport);
    noteModuleExport(moduleExport.name);
  }
  if (const char* refusal = WasmScriptingVMNatives::startRefusal(this))
  {
    m_lastError = std::string("module instantiate failed: ") + refusal;
    return false;
  }
  // The top level may start lanes.
  attachLanes(module);
  // A top level that decodes a file starts another module inside this one.
  const std::function<void(const char*, size_t)>* outerPrint = s_bootPrint;
  WasmScriptingVM* outerBooting = s_booting;
  s_bootPrint = &m_print;
  s_booting = this;
  char error[256] = {0};
  // Module start is a call into the module like any other: the top
  // level's probes need the bracket, its budget bounds it, and its trap
  // is the instantiate error.
  if (m_debugHooks != nullptr)
  {
    m_debugHooks->onCallBegin(*this);
  }
  // Set once: WAMR keeps it in a plain global, as ARMv7 Android loads no
  // ELF TLS, and the hook finds its VM through s_booting.
  static std::once_flag startHookOnce;
  std::call_once(startHookOnce, [] {
    wasm_runtime_rive_set_start_hook(
        [](wasm_exec_env_t env, bool begin, void*) {
          WasmScriptingVM* vm = s_booting;
          // A lane the top level joined starts on this thread too.
          if (vm == nullptr || WasmLane::current() != nullptr)
          {
            return;
          }
          if (begin)
          {
            // A start nested in an outer call can run on that call's
            // env, whose own armed watchdog then bounds it.
            WasmScriptingVM* outer = vmFromEnv(env);
            bool outerBounds = outer != nullptr && outer->m_budgetArmed &&
                               outer->m_budgetSlot->env == env;
            // Trusted modules run their top level unbounded, like their
            // calls.
            if (vm->m_hostBudget && !outerBounds && !vm->m_budgetArmed &&
                vm->boundsCalls())
            {
              vm->armWatchdog(env);
            }
            vm->armDeadline();
            // A wait moves a metered module's fuel on with a call.
            vm->m_state->execEnv = env;
          }
          else
          {
            vm->m_state->execEnv = nullptr;
            if (vm->m_deadlineArmed)
            {
              vm->disarmDeadline();
            }
            if (vm->m_budgetArmed && vm->m_budgetSlot->env == env)
            {
              vm->disarmWatchdog();
            }
          }
          uint32_t fuelMs = (uint32_t)WasmScriptingVMNatives::startFuelMs(vm);
          wasm_module_inst_t inst = wasm_runtime_get_module_inst(env);
          wasm_function_inst_t arm = wasm_runtime_lookup_function(
              inst,
              "__riveFuelArm");
          wasm_function_inst_t end = wasm_runtime_lookup_function(
              inst,
              "__riveFuelEnd");
          // A start that trapped leaves no fuel to put back.
          if (fuelMs != 0 && arm != nullptr && end != nullptr &&
              wasm_runtime_get_exception(inst) == nullptr)
          {
            wasm_runtime_call_wasm(
                env,
                begin ? arm : end,
                begin ? 1 : 0,
                &fuelMs);
          }
        },
        nullptr);
  });
  // A start nested in a call must not see the region it holds, and a top
  // level that trapped inside exclusive must not leave its region held.
  uint32_t outerRegionId;
  SharedRegion* outerRegion = stashHeldRegion(&outerRegionId);
  m_state->instance = wasm_runtime_instantiate(
      m_state->module,
      512 * 1024,
      0,
      error,
      sizeof(error));
  restoreHeldRegion(outerRegionId, outerRegion);
  if (m_debugHooks != nullptr)
  {
    m_debugHooks->onCallEnd(
        *this,
        m_state->instance == nullptr ? error : nullptr);
  }
  s_booting = outerBooting;
  s_bootPrint = outerPrint;
  if (m_state->instance == nullptr || m_poisoned)
  {
    // The runtime's exception buffer truncates what start threw, and AOT
    // code traps on a timeout as unreachable.
    std::string detail = m_poisoned ? "execution exceeded timeout"
                         : m_moduleErrorDetail.empty() ? std::string(error)
                                                       : m_moduleErrorDetail;
    m_lastError = "module instantiate failed: " + detail;
    return false;
  }
  m_state->findCallState();
  m_state->execEnv = wasm_runtime_create_exec_env(
      m_state->instance,
      512 * 1024);
  if (m_state->execEnv == nullptr)
  {
    m_lastError = "exec env creation failed";
    return false;
  }
  wasm_runtime_set_user_data(m_state->execEnv, this);
  return bootModule();
}
#endif

void WasmScriptingVM::noteModuleExport(const char* name)
{
  m_utf16Strings |= strcmp(name, "__riveUtf16Strings") == 0;
  m_utf16HostStrings |= strcmp(name, "__riveUtf16HostStrings") == 0;
  m_hostBudget |= strcmp(name, "__riveHostBudget") == 0;
  m_fuelBudget |= strcmp(name, "__riveFuelCheck") == 0;
  m_fuelShift |= strcmp(name, "__riveFuelShift") == 0;
  m_fetchBlobBody |= strcmp(name, "__riveFetchBlobBody") == 0;
}

bool WasmScriptingVM::bootModule()
{
  // The leak watch for rasc-linked modules (riveRegister marks one): the
  // stub baseline never frees, and sustained growth on a collecting
  // module is references accumulating.
  const char* leakEnv = getenv("RIVE_WASM_LEAK_WARN");
  m_leakWatch = hasExport("riveRegister") &&
                (leakEnv == nullptr || strcmp(leakEnv, "0") != 0);
  m_collectedRuntime = hasExport("__riveCollected");
  m_handleWatch = m_leakWatch;
  // Frame collector modules scavenge at every boundary instead of
  // rewinding or finalizing.
  m_frameMinor = hasExport("__riveFrameMinor");
  m_frameRelease = m_frameMinor && hasExport("__riveFrameRelease");
  m_frameMajorsProbe = hasExport("__riveFrameMajors");
  // Stub modules report their bump position, finer than page counts.
  m_heapUsedProbe = hasExport("__riveHeapUsed");

  m_rawAllocExport = hasExport("__riveRawAlloc");
  m_inputSlotExport = hasExport("host_obj_input_slot");
  m_pathEffectV2 = hasExport("host_obj_path_effect_v2");
  m_pointerEventV2 = hasExport("host_obj_pointer_event_v2");
  m_layoutResizeV2 = hasExport("host_obj_layout_resize_v2");
  m_transitionManagesToExport = hasExport("host_obj_transition_manages_to");

  callModule("__wasm_call_ctors", 0, nullptr);
  m_L = callModule("host_newstate", 0, nullptr);
  if (m_L == 0)
  {
    m_lastError = "host_newstate failed";
    return false;
  }
  // A deterministic run pins math.random, as ScriptingVM::init does for
  // native Luau. Older blobs without the export stay clock-seeded.
  if (File::deterministicMode && hasExport("host_randomseed"))
  {
    uint32_t seedArgs[2] = {m_L, (uint32_t)File::deterministicSeed};
    callModule("host_randomseed", 2, seedArgs);
  }
  uint32_t largs[1] = {m_L};
  // Same execution budget as the Luau backend's timed pcall.
  uint32_t timeoutArgs[2] = {m_L, (uint32_t)m_timeoutMs};
  callModule("host_set_timeout", 2, timeoutArgs);
  callModule("host_install_require", 1, largs);
  callModule("host_install_rive_math", 1, largs);
  callModule("host_install_rive_input", 1, largs);
  callModule("host_install_rive_renderer", 1, largs);
  callModule("host_install_rive_data", 1, largs);
  callModule("host_install_rive_gpu", 1, largs);
  // Last like the Luau backend's lualibs order, and before host_seal_env
  // so the Promise/async/await globals do not clear the safe-env flag.
  callModule("host_install_rive_promise", 1, largs);
  m_embeddedModules = (int)callModule("host_register_embedded", 1, largs);
  callModule("host_seal_env", 1, largs);
  return true;
}

const char* WasmScriptingVM::frameBoundary()
{
  const char* warning = collectFrame();
  if (m_decodedVMs.empty())
  {
    return warning;
  }
  m_frameReport = warning != nullptr ? warning : "";
  // A file a script decoded has no host to give it boundaries.
  for (WasmScriptingVM* decoded : std::vector<WasmScriptingVM*>(m_decodedVMs))
  {
    // Releasing one file can end another.
    if (std::find(m_decodedVMs.begin(), m_decodedVMs.end(), decoded) ==
        m_decodedVMs.end())
    {
      continue;
    }
    if (const char* notice = decoded->frameBoundary())
    {
      joinFrameReport(m_frameReport, notice);
    }
  }
  return m_frameReport.empty() ? nullptr : m_frameReport.c_str();
}

const char* WasmScriptingVM::frameBoundaryWarnings()
{
  const char* report = frameBoundary();
  std::string warnings;
  for (std::string_view rest = report != nullptr ? report : ""; !rest.empty();)
  {
    std::string_view line = rest.substr(0, rest.find('\n'));
    rest.remove_prefix(std::min(rest.size(), line.size() + 1));
    if (line != kFrameGcNotice)
    {
      joinFrameReport(warnings, line);
    }
  }
  m_frameReport = std::move(warnings);
  return m_frameReport.empty() ? nullptr : m_frameReport.c_str();
}

void WasmScriptingVM::joinFrameReport(
    std::string& joined,
    std::string_view report)
{
  if (!joined.empty())
  {
    joined += '\n';
  }
  joined += report;
}

const char* WasmScriptingVM::collectFrame()
{
  if (!valid())
  {
    return nullptr;
  }
  releaseReadHolds();
  if (m_frameMinor)
  {
    uint32_t promoted = 0;
    if (callModuleChecked("__riveFrameMinor", 0, nullptr, &promoted) ==
        CallOutcome::trapped)
    {
      // Objects may be half moved, so nothing may run on the heap.
      poison();
      return "script stopped: its collector trapped mid collection";
    }
    if (getenv("RIVE_FRAME_GC_DEBUG") != nullptr)
    {
      fprintf(stderr, "frameMinor promoted=%u\n", promoted);
      callModule("__riveFrameVerify", 0, nullptr);
    }
    if (m_frameRelease)
    {
      // The handles the collection found dead. Releasing one can
      // destroy an artboard instance whose scripted objects call
      // back into the module (host_obj_dispose, host_unref), so this
      // runs after the collection, never inside it. (A host that
      // skips it gets them released at the next boundary.)
      callModule("__riveFrameRelease", 0, nullptr);
    }
    if (!m_frameMinorAnnounced)
    {
      m_frameMinorAnnounced = true;
      return kFrameGcNotice;
    }
    if (const char* warning = handleLeakWarning())
    {
      return warning;
    }
    return heapGrowthWarning();
  }
  if (const char* warning = handleLeakWarning())
  {
    return warning;
  }
  return heapGrowthWarning();
}

#ifdef __EMSCRIPTEN__
// A page cannot set the environment.
static constexpr const char* kLeakWarnOffHint = "";
#else
static constexpr const char*
    kLeakWarnOffHint = " RIVE_WASM_LEAK_WARN=0 silences this.";
#endif

const char* WasmScriptingVM::heapGrowthWarning()
{
  if (!m_leakWatch || !m_advancedOnce)
  {
    return nullptr;
  }
  uint32_t pages = 0;
  uint32_t maxPages = 0;
  if (!memoryLimits(&pages, &maxPages))
  {
    return nullptr;
  }
  // The stub bump position beats page counts when available.
  if (m_heapUsedProbe)
  {
    pages = std::max(1u, callModule("__riveHeapUsed", 0, nullptr) >> 16);
  }
  if (m_leakBaselinePages == 0)
  {
    m_leakBaselinePages = pages;
    m_leakFirstBaselinePages = pages;
    return nullptr;
  }
  m_leakFrames++;
  m_leakTotalFrames++;
  // 8MB past baseline over at least two seconds of frames: far beyond any
  // one-time warmup we have measured, reached in seconds by a per-frame
  // leak (box2d hand-optimized leaked ~2.1MB/s). The watch re-arms after
  // each warning, so a leaking session keeps hearing about it every 8MB
  // instead of dying hours after a single line scrolled away.
  constexpr uint32_t kLeakWarnPages = 128;
  constexpr uint32_t kLeakWarnMinFrames = 120;
  if (m_leakFrames < kLeakWarnMinFrames ||
      pages < m_leakBaselinePages + kLeakWarnPages)
  {
    return nullptr;
  }
  if (m_collectedRuntime && !m_leakArmedCollected)
  {
    // Linear memory never shrinks, so a collected runtime's one-time
    // spike would read as growth forever. Demand a second growing
    // window before the first warning.
    m_leakArmedCollected = true;
    m_leakBaselinePages = pages;
    m_leakFrames = 0;
    return nullptr;
  }
  uint32_t grownMB = (pages - m_leakBaselinePages) / 16;
  uint32_t frames = m_leakFrames;
  m_leakBaselinePages = pages;
  m_leakFrames = 0;
  m_leakWarningCount++;

  // Time to the wasmMaxPages trap from the average growth rate at 60fps.
  // Growth arrives in page-doubling steps, so this is an estimate.
  char projection[96] = {0};
  double pagesPerFrame = (double)(pages - m_leakFirstBaselinePages) /
                         (double)m_leakTotalFrames;
  if (maxPages > pages && pagesPerFrame > 0.0)
  {
    double minutes = (double)(maxPages - pages) / pagesPerFrame / (60.0 * 60.0);
    snprintf(
        projection,
        sizeof(projection),
        "; at this rate every frame traps in roughly %.0f minutes",
        minutes < 1.0 ? 1.0 : minutes);
  }
  char buffer[384];
  if (m_collectedRuntime)
  {
    snprintf(
        buffer,
        sizeof(buffer),
        "script heap grew %uMB over %u frames%s; the collector is "
        "running, so something is accumulating references (a "
        "growing array, map, or cache).%s",
        grownMB,
        frames,
        projection,
        kLeakWarnOffHint);
  }
  else
  {
    snprintf(
        buffer,
        sizeof(buffer),
        "script heap grew %uMB over %u frames%s; the stub runtime "
        "never frees, so per-frame allocations leak. Set "
        "wasmRuntime: frame.%s",
        grownMB,
        frames,
        projection,
        kLeakWarnOffHint);
  }
  m_leakWarning = buffer;
  return m_leakWarning.c_str();
}

static const char* handleTagName(WasmScriptingVM::HandleTable::Tag tag)
{
  using Tag = WasmScriptingVM::HandleTable::Tag;
  switch (tag)
  {
    case Tag::path:
      return "path";
    case Tag::measure:
      return "measure";
    case Tag::paint:
      return "paint";
    case Tag::renderer:
      return "renderer";
    case Tag::shader:
      return "shader";
    case Tag::object:
      return "object";
    case Tag::viewModelInstance:
      return "viewModelInstance";
    case Tag::instanceValue:
      return "instanceValue";
    case Tag::image:
      return "image";
    case Tag::font:
      return "font";
    case Tag::text:
      return "text";
    case Tag::buffer:
      return "buffer";
    case Tag::meshInstances:
      return "meshInstances";
    case Tag::canvas:
      return "canvas";
    case Tag::gpuCanvas:
      return "gpuCanvas";
    case Tag::gpuPass:
      return "gpuPass";
    case Tag::gpuBuffer:
      return "gpuBuffer";
    case Tag::gpuTexture:
      return "gpuTexture";
    case Tag::gpuSampler:
      return "gpuSampler";
    case Tag::gpuTextureView:
      return "gpuTextureView";
    case Tag::gpuShaderModule:
      return "gpuShaderModule";
    case Tag::gpuBindGroupLayout:
      return "gpuBindGroupLayout";
    case Tag::gpuBindGroup:
      return "gpuBindGroup";
    case Tag::gpuPipeline:
      return "gpuPipeline";
    case Tag::dataContext:
      return "dataContext";
    case Tag::artboard:
      return "artboard";
    case Tag::animation:
      return "animation";
    case Tag::node:
      return "node";
    case Tag::transitionChild:
      return "transitionChild";
    case Tag::audioSource:
      return "audioSource";
    case Tag::audioSound:
      return "audioSound";
    case Tag::drawable:
      return "drawable";
    case Tag::riveFile:
      return "riveFile";
    case Tag::bindableArtboard:
      return "bindableArtboard";
    case Tag::lane:
      return "lane";
    case Tag::empty:
    case Tag::count:
      break;
  }
  return "unknown";
}

#ifndef __EMSCRIPTEN__
bool WasmScriptingVM::memoryLimits(uint32_t* pages, uint32_t* maxPages) const
{
  if (m_state == nullptr || m_state->instance == nullptr)
  {
    return false;
  }
  wasm_memory_inst_t memory = wasm_runtime_get_default_memory(
      m_state->instance);
  if (memory == nullptr)
  {
    return false;
  }
  *pages = (uint32_t)wasm_memory_get_cur_page_count(memory);
  *maxPages = (uint32_t)wasm_memory_get_max_page_count(memory);
  return true;
}
#endif

uint32_t WasmScriptingVM::memoryPages() const
{
  uint32_t pages = 0;
  uint32_t maxPages = 0;
  return memoryLimits(&pages, &maxPages) ? pages : 0;
}

uint32_t WasmScriptingVM::frameMajors()
{
  return m_frameMajorsProbe ? callModule("__riveFrameMajors", 0, nullptr) : 0;
}

const char* WasmScriptingVM::handleLeakWarning()
{
  if (!m_handleWatch || !m_advancedOnce)
  {
    return nullptr;
  }
  uint32_t live = (uint32_t)m_handles.live();
  if (m_handleBaselineLive == 0)
  {
    // Bias by one so a zero-handle module still records its baseline.
    m_handleBaselineLive = live + 1;
    return nullptr;
  }
  m_handleFrames++;
  // Hundreds of handles past warmup over two seconds of frames is a
  // per-frame mint with no release, not a working set.
  constexpr uint32_t kHandleWarnCount = 512;
  constexpr uint32_t kHandleWarnMinFrames = 120;
  if (live + 1 < m_handleBaselineLive + kHandleWarnCount)
  {
    m_handleCollects = 0;
    return nullptr;
  }
  if (m_handleFrames < kHandleWarnMinFrames)
  {
    return nullptr;
  }
  // Wrappers that outlived a boundary die in the old region, which only
  // a major sweeps, and its size trigger cannot see the host side of a
  // handle. Ask for one and only warn if the growth survives it. A bake
  // without the majors counter cannot say when that is, so it stays quiet.
  if (m_collectedRuntime && !m_frameMajorsProbe)
  {
    return nullptr;
  }
  // A major already in flight keeps wrappers that died after it began,
  // so growth is judged by one requested once the first finishes.
  constexpr uint8_t kHandleJudgeCollects = 2;
  if (m_collectedRuntime)
  {
    uint32_t majors = frameMajors();
    if (m_handleCollects != 0 && majors == m_handleCollectMajors)
    {
      return nullptr;
    }
    if (m_handleCollects < kHandleJudgeCollects)
    {
      m_handleCollects++;
      m_handleCollectMajors = majors;
      callModule("__riveCollect", 0, nullptr);
      return nullptr;
    }
  }
  m_handleCollects = 0;
  // Re-arm so a leaking session keeps warning every 512 handles.
  uint32_t grown = live + 1 - m_handleBaselineLive;
  uint32_t frames = m_handleFrames;
  m_handleBaselineLive = live + 1;
  m_handleFrames = 0;
  uint32_t counts[(size_t)HandleTable::Tag::count] = {0};
  for (const HandleTable::Slot& slot : m_handles.slots)
  {
    if (slot.tag != HandleTable::Tag::empty)
    {
      counts[(size_t)slot.tag]++;
    }
  }
  size_t top = 0;
  for (size_t i = 1; i < sizeof(counts) / sizeof(counts[0]); i++)
  {
    if (counts[i] > counts[top])
    {
      top = i;
    }
  }
  char buffer[256];
  snprintf(
      buffer,
      sizeof(buffer),
      "script leaked %u host handles over %u frames (most: %u %s); "
      "resources created per frame need release() or finish().%s",
      grown,
      frames,
      counts[top],
      handleTagName((HandleTable::Tag)top),
      kLeakWarnOffHint);
  m_leakWarning = buffer;
  return m_leakWarning.c_str();
}

bool WasmScriptingVM::registerBytecode(
    const std::string& name,
    Span<const uint8_t> bytecode)
{
  uint32_t nameSizeArgs[1] = {(uint32_t)name.size() + 1};
  uint32_t namePtr = callModule("malloc", 1, nameSizeArgs);
  uint32_t bcSizeArgs[1] = {(uint32_t)bytecode.size()};
  uint32_t bcPtr = callModule("malloc", 1, bcSizeArgs);
  if (namePtr == 0 || bcPtr == 0)
  {
    m_lastError = "bytecode allocation failed";
    return false;
  }
  copyToModule(namePtr, name.c_str(), (uint32_t)name.size() + 1);
  copyToModule(bcPtr, bytecode.data(), (uint32_t)bytecode.size());

  uint32_t args[4] = {m_L, namePtr, bcPtr, (uint32_t)bytecode.size()};
  callModule("host_register_module", 4, args);

  uint32_t freeName[1] = {namePtr};
  callModule("free", 1, freeName);
  // The registry keeps its own copy via lua_pushlstring; the staging
  // buffer frees.
  uint32_t freeBc[1] = {bcPtr};
  callModule("free", 1, freeBc);
  return true;
}

#ifdef WITH_RIVE_TOOLS
void WasmScriptingVM::registerShaderRstb(
    std::string name,
    std::vector<uint8_t> bytes)
{
  m_shaderRstbs[std::move(name)] = std::move(bytes);
}

const std::vector<uint8_t>* WasmScriptingVM::findShaderRstb(
    const std::string& name) const
{
  auto it = m_shaderRstbs.find(name);
  if (it != m_shaderRstbs.end())
  {
    return &it->second;
  }
  // Registered names are library-mangled paths; scripts ask by short name.
  for (const auto& entry : m_shaderRstbs)
  {
    size_t slash = entry.first.rfind('/');
    if (slash != std::string::npos && entry.first.substr(slash + 1) == name)
    {
      return &entry.second;
    }
  }
  return nullptr;
}
#endif

bool WasmScriptingVM::requireModule(const std::string& name, int* outResultRef)
{
  uint32_t namePtr = guestString(name.c_str());
  if (namePtr == 0)
  {
    m_lastError = "module name allocation failed";
    return false;
  }

  uint32_t requireArgs[2] = {m_L, namePtr};
  uint32_t status = 0;
  CallOutcome outcome = callModuleChecked(
      "host_require",
      2,
      requireArgs,
      &status);

  guestFree(namePtr);

  if (outcome != CallOutcome::ok)
  {
    // Folding a trap into "status 0" once read as a successful require
    // with no generator; fail the require instead.
    m_lastError = outcome == CallOutcome::trapped
                      ? "module require trapped"
                      : "module has no host_require export";
    uint32_t topArgs[2] = {m_L, 0};
    callModule("host_settop", 2, topArgs);
    return false;
  }
  if (status != 0)
  {
    uint32_t strArgs[2] = {m_L, (uint32_t)-1};
    uint32_t messagePtr = callModule("host_tostring", 2, strArgs);
    const char* message = messagePtr != 0 ? resolveModuleString(messagePtr)
                                          : nullptr;
    m_lastError = message != nullptr ? message : "module execution failed";
  }
  else if (outResultRef != nullptr)
  {
    uint32_t refArgs[1] = {m_L};
    *outResultRef = (int)callModule("host_ref", 1, refArgs);
  }
  uint32_t topArgs[2] = {m_L, 0};
  callModule("host_settop", 2, topArgs);
  return status == 0;
}

// --- ScriptBackend over the module's host_obj_* exports ---------------------

#ifndef __EMSCRIPTEN__
bool WasmScriptingVM::valid() const
{
  return m_state != nullptr && m_state->execEnv != nullptr && m_L != 0 &&
         !m_poisoned;
}
#endif

void WasmScriptingVM::releaseRef(int ref)
{
  if (!valid() || ref == 0)
  {
    return;
  }
  auto it = m_contextObjects.find(ref);
  if (it != m_contextObjects.end())
  {
    m_handles.release(it->second, HandleTable::Tag::object);
    m_contextObjects.erase(it);
  }
  m_inputSlots.erase(ref);
  uint32_t args[2] = {m_L, (uint32_t)ref};
  // Listeners owned by this script instance stop immediately, mirroring the
  // Luau backend's tracked property dispose.
  callModule("host_obj_dispose", 2, args);
  callModule("host_unref", 2, args);
}

int WasmScriptingVM::instantiate(
    int generatorRef,
    ScriptedObject* object,
    int* outContextRef,
    ScriptedContext** outContextPtr)
{
  uint32_t objectHandle = m_handles.mint(HandleTable::Tag::object, object);
  uint32_t args[3] = {m_L, (uint32_t)generatorRef, objectHandle};
  int selfRef = (int)callModule("host_obj_instantiate", 3, args);
  if (selfRef == 0)
  {
    m_handles.release(objectHandle, HandleTable::Tag::object);
    return 0;
  }
  uint32_t contextArgs[1] = {m_L};
  *outContextRef = (int)callModule("host_obj_context", 1, contextArgs);
  if (*outContextRef != 0)
  {
    m_contextObjects[*outContextRef] = objectHandle;
  }
  else
  {
    m_handles.release(objectHandle, HandleTable::Tag::object);
  }
  // The wasm context lives entirely in the module until the binding layer
  // moves in; there is no host-side ScriptedContext.
  *outContextPtr = nullptr;
  return selfRef;
}

ScriptBackend::InitResult WasmScriptingVM::callUserInit(
    ScriptedObject* object,
    int selfRef,
    int contextRef)
{
  double args[3] = {(double)m_L, (double)selfRef, (double)contextRef};
  double status = 0;
  // An init can run a nested one; each reads only its own misses.
  bool outerMissing = m_missingRequestedData;
  m_missingRequestedData = false;
  CallOutcome outcome = callF64("host_obj_user_init", args, 3, &status);
  bool missing = m_missingRequestedData;
  m_missingRequestedData = outerMissing;
  switch (outcome)
  {
    case CallOutcome::missing:
      return InitResult::notImplemented;
    case CallOutcome::trapped:
      m_lastError = lastTrap().message;
      return InitResult::failed;
    case CallOutcome::ok:
      break;
  }
  if (status == 0)
  {
    return InitResult::notImplemented;
  }
  return status == 1 && !missing ? InitResult::succeeded : InitResult::failed;
}

bool WasmScriptingVM::callAdvance(
    ScriptedObject* object,
    int selfRef,
    float elapsedSeconds)
{
  double args[3] = {(double)m_L, (double)selfRef, elapsedSeconds};
  double keepGoing = 0;
  if (callF64("host_obj_advance", args, 3, &keepGoing) != CallOutcome::ok)
  {
    return false;
  }
  m_advancedOnce = true;
  return keepGoing != 0;
}

void WasmScriptingVM::callUpdate(ScriptedObject* object, int selfRef)
{
  // Marks issued during update must not re-arm it, like the Luau backend.
  object->setInUpdatePhase(true);
  uint32_t args[2] = {m_L, (uint32_t)selfRef};
  callModule("host_obj_update", 2, args);
  object->setInUpdatePhase(false);
}

void WasmScriptingVM::callTrigger(
    ScriptedObject* object,
    int selfRef,
    const char* name)
{
  uint32_t arg = 0;
  uint32_t owned = 0;
  if (!inputArg(selfRef, name, arg, owned))
  {
    return;
  }
  uint32_t args[3] = {m_L, (uint32_t)selfRef, arg};
  callModule("host_obj_trigger", 3, args);
  guestFree(owned);
}

bool WasmScriptingVM::callNumberMethod(
    ScriptedObject* object,
    int selfRef,
    const char* name,
    const float* args,
    size_t argCount,
    float* outResult)
{
  GuestName nameCopy(this, name);
  uint32_t namePtr = nameCopy.ptr;
  if (namePtr == 0)
  {
    return false;
  }
  uint32_t scratchSize = (uint32_t)(argCount * sizeof(float)) + sizeof(float);
  uint32_t sizeArgs[1] = {scratchSize};
  uint32_t scratch = callModule("malloc", 1, sizeArgs);
  if (scratch == 0)
  {
    return false;
  }
  copyToModule(scratch, args, (uint32_t)(argCount * sizeof(float)));
  uint32_t outPtr = scratch + (uint32_t)(argCount * sizeof(float));
  uint32_t callArgs[6] =
      {m_L, (uint32_t)selfRef, namePtr, scratch, (uint32_t)argCount, outPtr};
  uint32_t ok = callModule("host_obj_number_method", 6, callArgs);
  if (ok != 0)
  {
    memcpy(outResult, resolveModulePtr(outPtr, sizeof(float)), sizeof(float));
  }
  guestFree(scratch);
  return ok != 0;
}

bool WasmScriptingVM::callBooleanMethod(
    ScriptedObject* object,
    int selfRef,
    const char* name)
{
  GuestName nameCopy(this, name);
  uint32_t namePtr = nameCopy.ptr;
  if (namePtr == 0)
  {
    return false;
  }
  uint32_t args[3] = {m_L, (uint32_t)selfRef, namePtr};
  uint32_t result = callModule("host_obj_boolean_method", 3, args);
  return result != 0;
}

bool WasmScriptingVM::callPathEffectUpdate(
    ScriptedObject* object,
    int selfRef,
    const RawPath& sourcePath,
    const ShapePaint* shapePaint,
    RawPath* outPath)
{
  if (!valid() || shapePaint == nullptr || outPath == nullptr)
  {
    return false;
  }
  // The exact subset the Luau lane's ScriptedPaintData(ShapePaint*)
  // snapshot captures; gradients stay host side there too.
  PathEffectPaintWire paint;
  paint.style = (uint32_t)RenderPaintStyle::fill;
  paint.blendMode = (uint32_t)BlendMode::srcOver;
  paint.thickness = 1;
  paint.color = 0xFF000000;
  if (shapePaint->is<Stroke>())
  {
    auto stroke = shapePaint->as<Stroke>();
    paint.style = (uint32_t)RenderPaintStyle::stroke;
    paint.thickness = stroke->thickness();
    paint.cap = stroke->cap();
    paint.join = stroke->join();
  }
  for (auto& child : shapePaint->children())
  {
    if (child->is<SolidColor>())
    {
      paint.color = child->as<SolidColor>()->colorValue();
      break;
    }
  }
  if (shapePaint->feather() != nullptr)
  {
    paint.feather = shapePaint->feather()->strength();
  }
  paint.blendMode = (uint32_t)shapePaint->blendModeValue();
  return callPathEffectUpdate(
      object,
      selfRef,
      sourcePath,
      paint,
      shapePaint->parentTransformComponent(),
      outPath);
}

bool WasmScriptingVM::callPathEffectUpdate(
    ScriptedObject* object,
    int selfRef,
    const RawPath& sourcePath,
    const PathEffectPaintWire& paint,
    TransformComponent* shape,
    RawPath* outPath)
{
  if (!valid() || outPath == nullptr)
  {
    return false;
  }
  uint32_t verbCount = (uint32_t)sourcePath.verbs().size();
  uint32_t floatCount = (uint32_t)sourcePath.points().size() * 2;
  uint32_t pointBytes = floatCount * (uint32_t)sizeof(float);
  // One staging block, float-aligned parts first: [paint][points][verbs].
  uint32_t byteCount = (uint32_t)sizeof(paint) + pointBytes + verbCount;
  uint32_t sizeArgs[1] = {byteCount};
  uint32_t dataPtr = callModule("malloc", 1, sizeArgs);
  if (dataPtr == 0)
  {
    return false;
  }
  uint8_t* out = (uint8_t*)resolveModulePtr(dataPtr, byteCount);
  memcpy(out, &paint, sizeof(paint));
  memcpy(out + sizeof(paint), sourcePath.points().data(), pointBytes);
  memcpy(
      out + sizeof(paint) + pointBytes,
      sourcePath.verbs().data(),
      verbCount);
  m_pathEffectOut = outPath;
  // _v2 also gets a node over the shape, the Luau lane's NodeData; modules
  // built before it get the paint alone.
  bool withNode = m_pathEffectV2;
  size_t scopeStart = m_scopedNodes.size();
  uint32_t nodeHandle = 0;
  if (withNode && shape != nullptr)
  {
    nodeHandle = mintNode(this, nullptr, shape);
  }
  uint32_t args[8] = {
      m_L,
      (uint32_t)selfRef,
      dataPtr + (uint32_t)sizeof(paint) + pointBytes,
      verbCount,
      dataPtr + (uint32_t)sizeof(paint),
      floatCount,
      dataPtr,
      nodeHandle};
  uint32_t ok = withNode ? callModule("host_obj_path_effect_v2", 8, args)
                         : callModule("host_obj_path_effect", 7, args);
  releaseScopedNodes(scopeStart);
  m_pathEffectOut = nullptr;
  guestFree(dataPtr);
  return ok != 0;
}

void WasmScriptingVM::releaseScopedNodes(size_t from)
{
  for (size_t i = from; i < m_scopedNodes.size(); i++)
  {
    uint32_t handle = m_scopedNodes[i];
    if (auto node = static_cast<HostNode*>(
            m_handles.resolve(handle, HandleTable::Tag::node)))
    {
      m_handles.release(handle, HandleTable::Tag::node);
      delete node;
    }
  }
  m_scopedNodes.resize(from);
}

bool WasmScriptingVM::callDataConvert(
    ScriptedObject* object,
    int selfRef,
    const char* method,
    DataValue* input,
    ScriptDataResult* outResult)
{
  if (!valid() || input == nullptr)
  {
    return false;
  }
  DataConvertWire wire;
  std::string text;
  if (input->is<DataValueNumber>())
  {
    wire.kind = DataConvertWire::kindNumber;
    wire.number = input->as<DataValueNumber>()->value();
  }
  else if (input->is<DataValueString>())
  {
    wire.kind = DataConvertWire::kindString;
    const std::string& value = input->as<DataValueString>()->value();
    text = moduleStringBytes(this, value.data(), value.size());
    wire.stringLength = (uint32_t)text.size();
  }
  else if (input->is<DataValueBoolean>())
  {
    wire.kind = DataConvertWire::kindBoolean;
    wire.boolean = input->as<DataValueBoolean>()->value() ? 1 : 0;
  }
  else if (input->is<DataValueColor>())
  {
    wire.kind = DataConvertWire::kindColor;
    wire.color = input->as<DataValueColor>()->value();
  }
  else
  {
    // Unsupported input kinds report handled with no result, so the
    // converter yields an empty DataValue like the Luau backend.
    return true;
  }
  GuestName methodCopy(this, method);
  uint32_t methodPtr = methodCopy.ptr;
  if (methodPtr == 0)
  {
    return false;
  }
  uint32_t byteCount = (uint32_t)sizeof(wire) + wire.stringLength;
  uint32_t sizeArgs[1] = {byteCount};
  uint32_t wirePtr = callModule("malloc", 1, sizeArgs);
  if (wirePtr == 0)
  {
    return false;
  }
  uint8_t* out = (uint8_t*)resolveModulePtr(wirePtr, byteCount);
  memcpy(out, &wire, sizeof(wire));
  memcpy(out + sizeof(wire), text.data(), wire.stringLength);
  m_convertResultOut = outResult;
  uint32_t args[4] = {m_L, (uint32_t)selfRef, methodPtr, wirePtr};
  uint32_t ran = callModule("host_obj_data_convert", 4, args);
  m_convertResultOut = nullptr;
  guestFree(wirePtr);
  return ran != 0;
}

bool WasmScriptingVM::callHitExport(
    const char* name,
    const double* args,
    uint32_t argc,
    HitResult* outResult)
{
  double hit = 0;
  if (callF64(name, args, argc, &hit) != CallOutcome::ok || hit == 0)
  {
    return false;
  }
  *outResult = (HitResult)((int)hit - 1);
  return true;
}

bool WasmScriptingVM::callPointerEvent(
    ScriptedObject* object,
    int selfRef,
    const char* method,
    int pointerId,
    Vec2D localPosition,
    ListenerType hitType,
    float timeStamp,
    HitResult* outResult)
{
  if (!valid())
  {
    return false;
  }
  GuestName methodCopy(this, method);
  uint32_t methodPtr = methodCopy.ptr;
  if (methodPtr == 0)
  {
    return false;
  }
  double args[8] = {
      (double)m_L,
      (double)selfRef,
      (double)methodPtr,
      (double)pointerId,
      localPosition.x,
      localPosition.y,
      (double)hitType,
      timeStamp};
  // Modules built before _v2 take no listener type or time stamp.
  return m_pointerEventV2
             ? callHitExport("host_obj_pointer_event_v2", args, 8, outResult)
             : callHitExport("host_obj_pointer_event", args, 6, outResult);
}

bool WasmScriptingVM::callScrollEvent(
    ScriptedObject* object,
    int selfRef,
    int pointerId,
    Vec2D localPosition,
    const ScrollEvent& event,
    float timeStamp,
    HitResult* outResult)
{
  if (!valid())
  {
    return false;
  }
  double args[10] = {
      (double)m_L,
      (double)selfRef,
      (double)pointerId,
      localPosition.x,
      localPosition.y,
      event.delta.x,
      event.delta.y,
      (double)event.phase,
      event.precise ? 1.0 : 0.0,
      timeStamp};
  // Modules built before scroll input lack the export and never scroll.
  return callHitExport("host_obj_scroll_event", args, 10, outResult);
}

bool WasmScriptingVM::callKeyboardEvent(
    ScriptedObject* object,
    int selfRef,
    Key key,
    KeyModifiers modifiers,
    bool isPressed,
    bool isRepeat)
{
  if (!valid())
  {
    return false;
  }
  uint32_t args[6] = {
      m_L,
      (uint32_t)selfRef,
      (uint32_t)key,
      (uint32_t)modifiers,
      isPressed ? 1u : 0u,
      isRepeat ? 1u : 0u};
  return callModule("host_obj_keyboard_event", 6, args) != 0;
}

bool WasmScriptingVM::callTextEvent(
    ScriptedObject* object,
    int selfRef,
    const std::string& text)
{
  if (!valid())
  {
    return false;
  }
  // Length crosses explicitly, so embedded nulls survive.
  std::string bytes = moduleStringBytes(this, text.data(), text.size());
  uint32_t sizeArgs[1] = {(uint32_t)bytes.size() + 1};
  uint32_t textPtr = callModule("malloc", 1, sizeArgs);
  if (textPtr == 0)
  {
    return false;
  }
  copyToModule(textPtr, bytes.data(), (uint32_t)bytes.size());
  uint32_t args[4] = {m_L, (uint32_t)selfRef, textPtr, (uint32_t)bytes.size()};
  uint32_t result = callModule("host_obj_text_event", 4, args);
  guestFree(textPtr);
  return result != 0;
}

bool WasmScriptingVM::callGamepadEvent(
    ScriptedObject* object,
    int selfRef,
    const char* method,
    const ListenerInvocation& invocation)
{
  if (!valid())
  {
    return false;
  }
  GamepadWire wire;
  const GamepadSnapshot* snapshot = nullptr;
  if (!packGamepadWire(invocation, wire, snapshot))
  {
    return false;
  }
  GuestName methodCopy(this, method);
  uint32_t methodPtr = methodCopy.ptr;
  if (methodPtr == 0)
  {
    return false;
  }
  uint32_t byteCount = gamepadPayloadSize(wire);
  uint32_t sizeArgs[1] = {byteCount};
  uint32_t dataPtr = callModule("malloc", 1, sizeArgs);
  if (dataPtr == 0)
  {
    return false;
  }
  writeGamepadPayload(
      (uint8_t*)resolveModulePtr(dataPtr, byteCount),
      wire,
      snapshot);
  uint32_t args[5] = {m_L, (uint32_t)selfRef, methodPtr, dataPtr, byteCount};
  uint32_t result = callModule("host_obj_gamepad_event", 5, args);
  guestFree(dataPtr);
  return result != 0;
}

void WasmScriptingVM::callListenerPerform(
    ScriptedObject* object,
    int selfRef,
    const ListenerInvocation& invocation)
{
  if (!valid())
  {
    return;
  }
  ListenerWire wire;
  wire.kind = (uint32_t)invocation.kind();
  std::string textBytes;
  const std::string* text = nullptr;
  GamepadWire gamepad;
  const GamepadSnapshot* snapshot = nullptr;
  bool hasGamepad = false;
  if (const PointerInvocation* p = invocation.asPointer())
  {
    wire.posX = p->position.x;
    wire.posY = p->position.y;
    wire.prevX = p->previousPosition.x;
    wire.prevY = p->previousPosition.y;
    wire.pointerId = p->pointerId;
    wire.hitEvent = (uint32_t)p->hitEvent;
    wire.timeStamp = p->timeStamp;
  }
  else if (const KeyboardInvocation* k = invocation.asKeyboard())
  {
    wire.key = (uint32_t)k->key;
    wire.modifiers = (uint32_t)k->modifiers;
    wire.isPressed = k->isPressed ? 1 : 0;
    wire.isRepeat = k->isRepeat ? 1 : 0;
  }
  else if (const TextInputInvocation* t = invocation.asTextInput())
  {
    textBytes = moduleStringBytes(this, t->text.data(), t->text.size());
    text = &textBytes;
    wire.textLength = (uint32_t)textBytes.size();
  }
  else if (const FocusInvocation* f = invocation.asFocus())
  {
    wire.isFocus = f->isFocus ? 1 : 0;
  }
  else if (const ReportedEventInvocation* e = invocation.asReportedEvent())
  {
    wire.delaySeconds = e->delaySeconds;
  }
  else if (const SemanticInvocation* s = invocation.asSemantic())
  {
    wire.semanticAction = (uint32_t)s->actionType;
  }
  else
  {
    // viewModelChange and none carry only their kind; the gamepad kinds
    // append their own payload.
    hasGamepad = packGamepadWire(invocation, gamepad, snapshot);
  }
  uint32_t tailBytes = text != nullptr
                           ? wire.textLength
                           : (hasGamepad ? gamepadPayloadSize(gamepad) : 0);
  uint32_t byteCount = (uint32_t)sizeof(wire) + tailBytes;
  uint32_t sizeArgs[1] = {byteCount};
  uint32_t dataPtr = callModule("malloc", 1, sizeArgs);
  if (dataPtr == 0)
  {
    return;
  }
  uint8_t* out = (uint8_t*)resolveModulePtr(dataPtr, byteCount);
  memcpy(out, &wire, sizeof(wire));
  if (text != nullptr)
  {
    memcpy(out + sizeof(wire), text->data(), wire.textLength);
  }
  else if (hasGamepad)
  {
    writeGamepadPayload(out + sizeof(wire), gamepad, snapshot);
  }
  uint32_t args[4] = {m_L, (uint32_t)selfRef, dataPtr, byteCount};
  callModule("host_obj_listener_perform", 4, args);
  guestFree(dataPtr);
}

// A module predating Transition cannot draw the incoming child, so the
// runtime keeps drawing it.
bool WasmScriptingVM::transitionManagesTo(int selfRef)
{
  if (!valid() || !m_transitionManagesToExport)
  {
    return false;
  }
  uint32_t args[2] = {m_L, (uint32_t)selfRef};
  return callModule("host_obj_transition_manages_to", 2, args) != 0;
}

uint32_t WasmScriptingVM::mintTransitionChild(const TransitionChildRef& ref)
{
  return ref.artboard == nullptr ? 0
                                 : m_handles.mint(
                                       HandleTable::Tag::transitionChild,
                                       const_cast<TransitionChildRef*>(&ref));
}

void WasmScriptingVM::releaseTransitionChild(uint32_t handle)
{
  if (handle != 0)
  {
    m_handles.release(handle, HandleTable::Tag::transitionChild);
  }
}

void WasmScriptingVM::callTransitionChanged(
    ScriptedObject* object,
    int selfRef,
    const TransitionChildRef& from,
    const TransitionChildRef& to,
    int direction)
{
  if (!valid())
  {
    return;
  }
  uint32_t fromHandle = mintTransitionChild(from);
  uint32_t toHandle = mintTransitionChild(to);
  uint32_t args[5] =
      {m_L, (uint32_t)selfRef, fromHandle, toHandle, (uint32_t)direction};
  callModule("host_obj_transition_changed", 5, args);
  releaseTransitionChild(fromHandle);
  releaseTransitionChild(toHandle);
}

void WasmScriptingVM::callTransitionDraw(
    ScriptedObject* object,
    int selfRef,
    Renderer* renderer,
    const TransitionChildRef& from,
    const TransitionChildRef& to)
{
  if (!valid())
  {
    return;
  }
  HostRenderer host{renderer};
  uint32_t rendererHandle = m_handles.mint(HandleTable::Tag::renderer, &host);
  uint32_t fromHandle = mintTransitionChild(from);
  uint32_t toHandle = mintTransitionChild(to);
  uint32_t args[5] =
      {m_L, (uint32_t)selfRef, rendererHandle, fromHandle, toHandle};
  callModule("host_obj_transition_draw", 5, args);
  host.restoreSaves();
  releaseTransitionChild(fromHandle);
  releaseTransitionChild(toHandle);
  m_handles.release(rendererHandle, HandleTable::Tag::renderer);
}

void WasmScriptingVM::callLayoutResize(
    ScriptedObject* object,
    int selfRef,
    Vec2D size)
{
  if (!valid())
  {
    return;
  }
  double args[5] =
      {(double)m_L, (double)selfRef, size.x, size.y, displayScale()};
  double unused = 0;
  // Modules built before _v2 take no surface scale.
  if (m_layoutResizeV2)
  {
    callF64("host_obj_layout_resize_v2", args, 5, &unused);
  }
  else
  {
    callF64("host_obj_layout_resize", args, 4, &unused);
  }
}

bool WasmScriptingVM::callLayoutMeasure(
    ScriptedObject* object,
    int selfRef,
    Vec2D* outSize)
{
  if (!valid())
  {
    return false;
  }
  uint32_t sizeArgs[1] = {2 * sizeof(float)};
  uint32_t scratch = callModule("malloc", 1, sizeArgs);
  if (scratch == 0)
  {
    return false;
  }
  // Prefill so the module's error paths leave outSize untouched, like the
  // Luau backend.
  float* out = (float*)resolveModulePtr(scratch, 2 * sizeof(float));
  out[0] = outSize->x;
  out[1] = outSize->y;
  uint32_t args[3] = {m_L, (uint32_t)selfRef, scratch};
  uint32_t measured = callModule("host_obj_layout_measure", 3, args);
  if (measured != 0)
  {
    out = (float*)resolveModulePtr(scratch, 2 * sizeof(float));
    outSize->x = out[0];
    outSize->y = out[1];
  }
  guestFree(scratch);
  return measured != 0;
}

int32_t WasmScriptingVM::inputSlot(int selfRef, const char* name)
{
  auto& slots = m_inputSlots[selfRef];
  auto it = slots.find(name);
  if (it != slots.end())
  {
    return it->second;
  }
  uint32_t namePtr = guestString(name);
  if (namePtr == 0)
  {
    return -1;
  }
  uint32_t args[3] = {m_L, (uint32_t)selfRef, namePtr};
  uint32_t slot = 0;
  CallOutcome outcome = callModuleChecked(
      "host_obj_input_slot",
      3,
      args,
      &slot);
  guestFree(namePtr);
  // A trap answers nothing, so the next lookup asks again.
  if (outcome != CallOutcome::ok)
  {
    return -1;
  }
  slots.emplace(name, (int32_t)slot);
  return (int32_t)slot;
}

bool WasmScriptingVM::legacyInputs() { return !m_inputSlotExport; }

bool WasmScriptingVM::inputArg(
    int selfRef,
    const char* name,
    uint32_t& arg,
    uint32_t& owned)
{
  owned = 0;
  if (legacyInputs())
  {
    owned = arg = guestString(name);
    return arg != 0;
  }
  int32_t slot = inputSlot(selfRef, name);
  if (slot < 0)
  {
    return false;
  }
  arg = (uint32_t)slot;
  return true;
}

void WasmScriptingVM::setInputBoolean(int selfRef, const char* name, bool value)
{
  uint32_t arg = 0;
  uint32_t owned = 0;
  if (!inputArg(selfRef, name, arg, owned))
  {
    return;
  }
  uint32_t args[4] = {m_L, (uint32_t)selfRef, arg, value ? 1u : 0u};
  callModule("host_obj_set_boolean", 4, args);
  guestFree(owned);
}

void WasmScriptingVM::setInputNumber(int selfRef, const char* name, float value)
{
  uint32_t arg = 0;
  uint32_t owned = 0;
  if (!inputArg(selfRef, name, arg, owned))
  {
    return;
  }
  double args[4] = {(double)m_L, (double)selfRef, (double)arg, value};
  double unused = 0;
  callF64("host_obj_set_number", args, 4, &unused);
  guestFree(owned);
}

void WasmScriptingVM::setInputUnsigned(
    int selfRef,
    const char* name,
    uint32_t value)
{
  uint32_t arg = 0;
  uint32_t owned = 0;
  if (!inputArg(selfRef, name, arg, owned))
  {
    return;
  }
  uint32_t args[4] = {m_L, (uint32_t)selfRef, arg, value};
  callModule(
      owned != 0 ? "host_obj_set_unsigned" : "host_obj_set_color",
      4,
      args);
  guestFree(owned);
}

void WasmScriptingVM::setInputString(
    int selfRef,
    const char* name,
    const char* value)
{
  uint32_t arg = 0;
  uint32_t owned = 0;
  if (!inputArg(selfRef, name, arg, owned))
  {
    return;
  }
  uint32_t valuePtr = guestString(value);
  if (valuePtr != 0)
  {
    uint32_t args[4] = {m_L, (uint32_t)selfRef, arg, valuePtr};
    callModule("host_obj_set_string", 4, args);
    guestFree(valuePtr);
  }
  guestFree(owned);
}

void WasmScriptingVM::setInputViewModel(
    int selfRef,
    const char* name,
    ViewModelInstanceValue* value)
{
  if (!valid() || value == nullptr)
  {
    return;
  }
  switch (value->coreType())
  {
    case ViewModelInstanceViewModelBase::typeKey:
    {
      auto viewModel = value->as<ViewModelInstanceViewModel>();
      auto vmi = viewModel->referenceViewModelInstance();
      if (vmi == nullptr)
      {
        fprintf(
            stderr,
            "setInputViewModel - passed in a "
            "ViewModelInstanceViewModel with no associated "
            "ViewModelInstance.\n");
        return;
      }
      uint32_t arg = 0;
      uint32_t owned = 0;
      if (!inputArg(selfRef, name, arg, owned))
      {
        return;
      }
      // The module's view model userdata owns the handle and releases
      // it from its finalizer.
      uint32_t handle = m_handles.mint(
          HandleTable::Tag::viewModelInstance,
          new HostViewModelInstance{std::move(vmi)});
      uint32_t args[4] = {m_L, (uint32_t)selfRef, arg, handle};
      uint32_t unused = 0;
      if (callModuleChecked("host_obj_set_view_model", 4, args, &unused) !=
          CallOutcome::ok)
      {
        releaseHostHandle(this, handle, HandleTable::Tag::viewModelInstance);
      }
      guestFree(owned);
      break;
    }
    default:
      // Nothing assigned; leave self untouched, like the Luau backend.
      break;
  }
}

void WasmScriptingVM::setInputArtboard(
    int selfRef,
    const char* name,
    ScriptedObject* object,
    Artboard* artboard)
{
  if (!valid() || object == nullptr || object->scriptAsset() == nullptr ||
      object->scriptAsset()->file() == nullptr || artboard == nullptr)
  {
    return;
  }
  uint32_t arg = 0;
  uint32_t owned = 0;
  if (!inputArg(selfRef, name, arg, owned))
  {
    return;
  }
  auto artboardInstance = artboard->instance();
  artboardInstance->frameOrigin(false);
  // The module's artboard userdata owns the handle and releases it from
  // its finalizer.
  uint32_t handle = m_handles.mint(
      HandleTable::Tag::artboard,
      new HostArtboard(
          object->scriptAsset()->file(),
          std::move(artboardInstance),
          nullptr,
          object->dataContext()));
  uint32_t args[4] = {m_L, (uint32_t)selfRef, arg, handle};
  uint32_t unused = 0;
  if (callModuleChecked("host_obj_set_artboard", 4, args, &unused) !=
      CallOutcome::ok)
  {
    releaseHostHandle(this, handle, HandleTable::Tag::artboard);
  }
  guestFree(owned);
}

uint32_t WasmScriptingVM::guestString(const char* text, const char* allocator)
{
  if (text == nullptr)
  {
    return 0;
  }
  std::string bytes = moduleStringBytes(this, text, strlen(text));
  // The terminator is one unit wide.
  bytes.append(m_utf16HostStrings ? 2 : 1, '\0');
  uint32_t sizeArgs[1] = {(uint32_t)bytes.size()};
  uint32_t ptr = callModule(allocator, 1, sizeArgs);
  if (ptr != 0)
  {
    copyToModule(ptr, bytes.data(), (uint32_t)bytes.size());
  }
  return ptr;
}

WasmScriptingVM::GuestName::GuestName(WasmScriptingVM* vm, const char* name)
    : m_vm(vm)
{
  if (!vm->m_rawAllocExport)
  {
    ptr = vm->guestString(name);
    m_owned = true;
    return;
  }
  auto& names = vm->m_guestNames;
  auto it = names.find(name);
  if (it != names.end())
  {
    ptr = it->second;
    return;
  }
  ptr = vm->guestString(name, "__riveRawAlloc");
  if (ptr != 0)
  {
    names.emplace(name, ptr);
  }
}

WasmScriptingVM::GuestName::~GuestName()
{
  if (m_owned)
  {
    m_vm->guestFree(ptr);
  }
}

void WasmScriptingVM::guestFree(uint32_t ptr)
{
  if (ptr == 0)
  {
    return;
  }
  uint32_t args[1] = {ptr};
  callModule("free", 1, args);
}

#ifdef __EMSCRIPTEN__
// The runner sets the booting VM around a module's start.
extern "C" EMSCRIPTEN_KEEPALIVE void rive_web_vm_booting(uint32_t handle)
{
  WasmScriptingVMNatives::booting((WasmScriptingVM*)(uintptr_t)handle);
}

extern "C" EMSCRIPTEN_KEEPALIVE int rive_web_vm_start_fuel(uint32_t handle)
{
  return WasmScriptingVMNatives::startFuelMs(
      (WasmScriptingVM*)(uintptr_t)handle);
}

extern "C" EMSCRIPTEN_KEEPALIVE const char* rive_web_vm_start_refusal(
    uint32_t handle)
{
  return WasmScriptingVMNatives::startRefusal(
      (WasmScriptingVM*)(uintptr_t)handle);
}

// The std's console imports, by the method codes the page's env block uses.
extern "C" void rive_web_console(
    uint32_t handle,
    uint32_t method,
    const char* text,
    uint32_t length)
{
  WasmScriptingVM* vm = bootVmFromHandle(handle);
  WasmStringArg utf8(vm, text, length);
  std::string value(utf8.data(), utf8.size());
  switch (method)
  {
    case 0:
      consoleWrite(vmFromHandle(handle), value);
      break;
    case 1:
      consoleTimeStart(vm, value);
      break;
    default:
      consoleTimeReport(vm, value, method == 3);
      break;
  }
}

extern "C" void rive_web_trace(
    uint32_t handle,
    const char* text,
    uint32_t length,
    int32_t count,
    double a0,
    double a1,
    double a2,
    double a3,
    double a4)
{
  WasmStringArg utf8(bootVmFromHandle(handle), text, length);
  double args[5] = {a0, a1, a2, a3, a4};
  traceWrite(
      vmFromHandle(handle),
      std::string(utf8.data(), utf8.size()),
      count,
      args);
}
#endif

// Browser-lane exports over the same impl cores; nothing off emscripten.
#include "wasm_natives_web_gen.hpp"

#endif
