#ifdef WITH_RIVE_SCRIPTING_WASM

#include "rive/wasm/wasm_jobs.hpp"
#include "rive/async/timeouts.hpp"
#include "rive/wasm/wasm_lane.hpp"
#include "rive/wasm/wasm_lane_policy.hpp"
#include "rive/wasm/wasm_scripting_vm.hpp"
#include "rive/wasm/wasm_shared.hpp"

#ifndef __EMSCRIPTEN__
#include "wasm_export.h"
#else
#include <emscripten/em_js.h>
#include <emscripten/emscripten.h>
#include <emscripten/threading.h>
#endif

#include <algorithm>
#include <cstring>
#include <thread>

using namespace rive;

// --- the pool ----------------------------------------------------------------

namespace
{
// Leaked with the pool, so no static destructor joins a running job at exit
// or frees a mutex a VM in another static still uses.
LaneMutex& poolMutex()
{
  static LaneMutex* mutex = new LaneMutex();
  return *mutex;
}

JobPool*& poolSlot()
{
  static JobPool* pool = nullptr;
  return pool;
}

uint32_t defaultThreadCount()
{
#if !RIVE_POOL_THREADS()
  return 0;
#else
  if (uint32_t threads = ScriptingLimits::get().jobThreads)
  {
    return threads;
  }
  uint32_t hw = std::thread::hardware_concurrency();
  return std::min<uint32_t>(hw > 1 ? hw - 1 : 1, 6);
#endif
}
} // namespace

void WasmJobs::runQueuedInline()
{
  {
    std::lock_guard<LaneMutex> lock(poolMutex());
    if (poolSlot() == nullptr || poolSlot()->threadCount() != 0)
    {
      return;
    }
  }
  pool().runPendingFor(lane_timing::kCooperativeSliceMs / 1000.0);
}

void WasmJobs::configurePool(
    uint32_t threads,
    JobSchedule schedule,
    uint64_t seed)
{
  std::lock_guard<LaneMutex> lock(poolMutex());
  delete poolSlot();
  poolSlot() = new JobPool(threads, schedule, seed);
}

JobPool& WasmJobs::pool()
{
  std::lock_guard<LaneMutex> lock(poolMutex());
  JobPool*& pool = poolSlot();
  if (pool == nullptr)
  {
    pool = new JobPool(defaultThreadCount());
  }
  return *pool;
}

// --- the jobs module ---------------------------------------------------------

#ifndef __EMSCRIPTEN__
namespace
{
uint32_t jobCancelled(wasm_exec_env_t)
{
  WasmJob* job = WasmJob::current();
  return job != nullptr && job->cancelRequested() ? 1 : 0;
}

NativeSymbol kJobNatives[] = {
    {"cancelled", (void*)jobCancelled, "()i", nullptr},
};

constexpr uint32_t kJobStack = 256 * 1024;
} // namespace

void rive::registerJobNatives()
{
  static std::once_flag once;
  std::call_once(once, []() {
    wasm_runtime_register_natives(
        "rive_job_v1",
        kJobNatives,
        sizeof(kJobNatives) / sizeof(kJobNatives[0]));
  });
}

bool WasmJob::execute()
{
  bool ok = false;
  bool reused = reuse();
  if (reused || instantiate(kJobStack))
  {
    if (cancelRequested())
    {
      interruptCall();
    }
    uint32_t argv[2] = {};
    ok = runStart(argv, reused) &&
         call(m_module->entryNames[m_entry].c_str(), 2, argv) &&
         takeResult(argv[0]);
    if (!ok)
    {
      failTrap();
    }
  }
  if (ok)
  {
    park();
  }
  teardown();
  return ok;
}

// Only a job that returned, unstopped, with its memory still small leaves
// its instance for another.
void WasmJob::park()
{
  if (!m_module->resets || hasFailure() ||
      wasm_memory_get_cur_page_count(wasm_runtime_get_default_memory(
          (wasm_module_inst_t)m_instance)) > lane_layout::kReuseMaxPages)
  {
    return;
  }
  WasmLaneModule::Instance instance = detach();
  // A stop that reached the exec env before the detach left its interrupt.
  if (cancelRequested())
  {
    WasmLaneModule::destroy(instance);
    return;
  }
  m_module->park(instance);
}

bool WasmJob::takeResult(uint32_t record)
{
  uint32_t words[2] = {};
  const void* at = wasmAppBytes(m_instance, record, 8);
  if (at != nullptr)
  {
    memcpy(words, at, 8);
  }
  // An empty result's address is never read.
  auto out = (const uint8_t*)(words[1] == 0 ? at
                                            : wasmAppBytes(
                                                  m_instance,
                                                  words[0],
                                                  words[1]));
  if (out == nullptr)
  {
    fail("job returned an invalid result");
    return false;
  }
  m_output.assign(out, out + words[1]);
  return true;
}

void WasmJob::interrupt() { interruptCall(); }
#else
// The page runner instantiates the jobs module on whichever thread claimed
// the job, which without pthreads is its joiner.
// clang-format off
EM_JS_DEPS(riveWebJobs, "$stringToNewUTF8");

// The output crosses once, from the job's memory into output. Null, or a
// malloc'd error.
EM_JS(char*, riveWebJobRun,
      (uint32_t lane, uint32_t moduleId, const uint8_t* module,
       uint32_t moduleSize, uint32_t entry, const uint8_t* input,
       uint32_t inputSize, const int32_t* cancel, void* output), {
    const result = Module['riveScripting']['runJob'](
        lane,
        moduleId,
        () => HEAPU8.slice(module, module + moduleSize),
        entry,
        HEAPU8.slice(input, input + inputSize),
        () => Atomics.load(HEAP32, cancel >> 2));
    if (!result.ok) {
        return stringToNewUTF8(result.error);
    }
    // Resizing may grow the heap, which replaces HEAPU8.
    const at = _rive_web_job_output(output, result.output.length);
    HEAPU8.set(result.output, at);
    return 0;
});
// clang-format on

extern "C" EMSCRIPTEN_KEEPALIVE uint8_t* rive_web_job_output(
    std::vector<uint8_t>* output,
    uint32_t size)
{
  output->resize(size);
  return output->data();
}

bool WasmJob::execute()
{
  if (!riveWebAttach())
  {
    fail("no script runner on this thread");
    return false;
  }
  std::vector<uint8_t> input = takeInput();
  PageThreadBudget budget(*this);
  char* error = riveWebJobRun(
      (uint32_t)(uintptr_t)(WasmLane*)this,
      m_module->id,
      m_module->bytes.data(),
      (uint32_t)m_module->bytes.size(),
      m_entry,
      input.data(),
      (uint32_t)input.size(),
      (const int32_t*)&m_cancelWord,
      &m_output);
  if (error != nullptr)
  {
    failTaken(error);
    return false;
  }
  return true;
}

// A running job's cancelled() reads the word and its fuel checks the
// interrupt, so only one that neither polls nor carries fuel runs to its end.
void WasmJob::interrupt()
{
  m_cancelWord.store(1);
  interruptCall();
}

#endif

// --- jobs --------------------------------------------------------------------

WasmJob* WasmJob::current() { return WasmLane::currentOf<WasmJob>(); }

void WasmJob::run()
{
#ifndef __EMSCRIPTEN__
  ensureLaneThreadEnv();
#endif
  WasmLane* outerLane = WasmLane::setCurrent(this);
  bool ok = execute();
  // A job that trapped inside an exclusive section never unlocked it.
  releaseTrappedRegion();
  WasmLane::setCurrent(outerLane);
  m_failed = !ok;
  if (!ok)
  {
    m_output.clear();
  }
  // A job that saw the cancel and returned stopped as one interrupted would.
  m_stopped = cancelRequested();
}

WasmJob::WasmJob(
    rcp<WasmLaneModule> module,
    uint32_t entry,
    std::vector<uint8_t> input)
    : WasmLane(kKind, std::move(module), entry, std::move(input))
{}

LaneState WasmJob::state() const
{
  switch (JobTask::state())
  {
    case State::pending:
      return LaneState::starting;
    case State::running:
      return LaneState::running;
    case State::done:
      return m_stopped  ? LaneState::stopped
             : m_failed ? LaneState::failed
                        : LaneState::done;
    case State::cancelled:
      return LaneState::stopped;
  }
  return LaneState::failed;
}

void WasmJob::stop() { WasmJobs::pool().cancel(this); }

WasmJobs::~WasmJobs()
{
  // Jobs hold the module, so a straggler outlives this safely; cancelling
  // keeps teardown from waiting on queued work.
  for (auto& job : m_lanes)
  {
    job->stop();
  }
}

rcp<WasmJob> WasmJobs::run(
    uint32_t entry,
    std::vector<uint8_t> input,
    uint32_t pageThreadBudgetMs)
{
  if (entry >= entryCount())
  {
    return nullptr;
  }
  auto job = add(make_rcp<WasmJob>(m_module, entry, std::move(input)));
  job->setPageThreadBudgetMs(pageThreadBudgetMs);
  pool().submit(job);
  return job;
}

namespace
{
// How long a join may wait, which on the page's main thread is never long.
double joinWaitSeconds(double timeoutSeconds, double usable)
{
  double seconds = std::min(timeoutSeconds, usable);
#ifdef __EMSCRIPTEN_PTHREADS__
  if (emscripten_is_main_browser_thread())
  {
    seconds = std::min(seconds, lane_timing::kMainWaitMs / 1000.0);
  }
#endif
  return seconds;
}
} // namespace

// Only a joiner without a budget runs a job here, as nothing cuts it short.
bool WasmJobs::join(WasmJob* job, double timeoutSeconds, double usable)
{
  return pool().join(
      job,
      joinWaitSeconds(timeoutSeconds, usable),
      unboundedSeconds(usable));
}

int WasmJobs::joinAny(Span<WasmJob*> jobs, double timeoutSeconds, double usable)
{
  std::vector<JobTask*> tasks(jobs.begin(), jobs.end());
  return pool().joinAny(
      tasks,
      joinWaitSeconds(timeoutSeconds, usable),
      unboundedSeconds(usable));
}

bool WasmJobs::hasUnfinished() const
{
  return std::any_of(
      m_lanes.begin(),
      m_lanes.end(),
      [](const rcp<WasmJob>& job) { return !job->finished(); });
}

#endif
