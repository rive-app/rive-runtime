#ifdef WITH_RIVE_SCRIPTING_WASM

#include "rive/wasm/wasm_workers.hpp"
#include "rive/async/job_pool.hpp"
#include "rive/wasm/wasm_jobs.hpp"
#include "rive/wasm/wasm_lane_policy.hpp"

#ifndef __EMSCRIPTEN__
#include "wasm_export.h"
#else
#include <emscripten/em_js.h>
#include <emscripten/emscripten.h>
#endif

#include <algorithm>
#include <condition_variable>
#include <cstdio>
#include <mutex>
#include <thread>

using namespace rive;

namespace
{
using Clock = std::chrono::steady_clock;
constexpr uint32_t kStopGraceMs = lane_timing::kWorkerStopGraceMs;
} // namespace

// --- the pool ----------------------------------------------------------------

#if RIVE_POOL_THREADS()
namespace
{
constexpr auto kSlice = std::chrono::milliseconds(lane_timing::kWorkerSliceMs);
// A thread the pool no longer needs lingers this long, so a worker that keeps
// taking long steps does not churn threads.
constexpr auto kRetireAfter = std::chrono::seconds(1);
} // namespace

// A few threads stepping every VM's workers, each taking whichever is due.
class rive::WasmWorkerPool
{
public:
  // Leaked, as its threads may outlive static destructors.
  static WasmWorkerPool& instance()
  {
    static WasmWorkerPool* pool = new WasmWorkerPool();
    return *pool;
  }

  // Every worker's signal waits watch it, so any notify reaches the pool.
  WakeWord& wakeWord() { return m_wake; }
  std::mutex& mutex() { return m_mutex; }

  void add(rcp<WasmWorker> worker)
  {
    bool first;
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_workers.push_back(std::move(worker));
      first = !m_supervising;
      m_supervising = true;
    }
    if (first)
    {
      JobPool::makeThread([this]() { superviseMain(); }).detach();
    }
    m_supervise.notify_one();
    m_wake.wake();
  }

  // Takes a worker no runner claimed or began, for its stop to end it at
  // once; false when a runner has it.
  bool takeUnstarted(WasmWorker* worker)
  {
    rcp<WasmWorker> taken;
    std::lock_guard<std::mutex> lock(m_mutex);
    if (worker->m_claimed || worker->m_begun || worker->over())
    {
      return false;
    }
    worker->m_claimed = true;
    taken = eraseLocked(worker);
    return true;
  }

  void join(WasmWorker* worker, Clock::time_point deadline)
  {
    while (!worker->over())
    {
      if (Clock::now() < deadline)
      {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        continue;
      }
#ifdef __EMSCRIPTEN__
      // The page's thread must not block, so a worker still running ends on
      // its own thread, if metered at its first fuel check past the grace.
      return;
#else
      bool idle;
      rcp<WasmWorker> taken;
      {
        std::lock_guard<std::mutex> lock(m_mutex);
        // A runner may have ended and dropped it since the check.
        if (worker->over())
        {
          return;
        }
        idle = !worker->m_claimed;
        if (idle)
        {
          worker->m_claimed = true;
          taken = eraseLocked(worker);
        }
      }
      // No pool thread got to its stop in time, so it ends here.
      if (idle)
      {
        worker->end();
        return;
      }
      // A worker that never awaits again never sees the stop, and one
      // with no instance yet has no call to interrupt.
      while (!worker->over())
      {
        worker->interruptCall();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      return;
#endif
    }
  }

  void configure(uint32_t threads, uint32_t promotedPerVM)
  {
    std::unique_lock<std::mutex> lock(m_mutex);
    if (threads == 0)
    {
      setDefaults();
    }
    else
    {
      m_threads = threads;
      m_promotedPerVM = promotedPerVM;
    }
    m_resizing = true;
    m_wake.wake();
    m_runnersLeft.wait(lock, [this]() { return m_runners.empty(); });
    m_resizing = false;
    lock.unlock();
    m_supervise.notify_one();
  }

private:
  enum class Slice
  {
    idle,
    timing,
    promoted,
    // Past the slice with every promotion taken, it keeps its thread.
    kept,
  };
  struct Runner
  {
    WasmWorker* stepping = nullptr;
    Clock::time_point stepStart;
    Slice slice = Slice::idle;
  };

  WasmWorkerPool() { setDefaults(); }

  void setDefaults()
  {
#ifdef __EMSCRIPTEN__
    // Browser threads come from a small pool all of librive shares.
    m_threads = 2;
    m_promotedPerVM = 2;
#else
    uint32_t hardware = std::thread::hardware_concurrency();
    m_threads = std::min<uint32_t>(hardware > 2 ? hardware - 1 : 1, 4);
    m_promotedPerVM = 4;
#endif
    if (uint32_t threads = ScriptingLimits::get().workerThreads)
    {
      m_threads = threads;
    }
  }

  void runnerMain(Runner* runner)
  {
#ifndef __EMSCRIPTEN__
    ensureLaneThreadEnv();
#endif
    Clock::time_point idleSince = Clock::now();
    std::unique_lock<std::mutex> lock(m_mutex);
    while (!m_resizing)
    {
      // Read before the scan, so a wake landing during it ends the
      // wait below at once.
      uint32_t seen = m_wake.value();
      Clock::time_point now = Clock::now();
      Clock::time_point next = Clock::time_point::max();
      if (rcp<WasmWorker> worker = takeLocked(runner, now, next))
      {
        runner->stepping = worker.get();
        runner->stepStart = now;
        runner->slice = Slice::timing;
        bool timeIt = m_superviseIdle;
        lock.unlock();
        if (timeIt)
        {
          m_supervise.notify_one();
        }
        bool ended = worker->run() == WasmWorker::Step::done;
        lock.lock();
        if (runner->slice == Slice::promoted && !worker->m_promoted)
        {
          worker->m_promoted = true;
          worker->log(
              "worker promoted to its own thread: a step "
              "ran past " +
              std::to_string(kSlice.count()) + " ms without awaiting");
        }
        // A stop this step never saw arms its grace again where it
        // is delivered.
        if (!ended && worker->stopping() && !worker->m_stopDelivered)
        {
          worker->dropInterrupt();
        }
        // What a long step held frees for one kept on its thread.
        bool freed = runner->slice != Slice::timing;
        runner->stepping = nullptr;
        runner->slice = Slice::idle;
        worker->m_claimed = false;
        rcp<WasmWorker> erased = ended ? eraseLocked(worker.get()) : nullptr;
        // Its last reference may go here, which must not happen
        // under the lock.
        lock.unlock();
        if (freed)
        {
          m_supervise.notify_one();
        }
        erased = nullptr;
        worker = nullptr;
        lock.lock();
        idleSince = Clock::now();
        continue;
      }
      if ((m_workers.empty() || servingLocked() > m_threads) &&
          !pinsLocked(runner))
      {
        Clock::time_point retire = idleSince + kRetireAfter;
        if (now >= retire)
        {
          break;
        }
        next = std::min(next, retire);
      }
      lock.unlock();
      m_wake.wait(seen, next);
      lock.lock();
    }
    m_runners.erase(std::find_if(
        m_runners.begin(),
        m_runners.end(),
        [runner](const auto& other) { return other.get() == runner; }));
    m_runnersLeft.notify_all();
    m_supervise.notify_one();
  }

  // Promotes long steps on time and keeps the pool at its size, whether or
  // not a runner is idle or a host advances.
  void superviseMain()
  {
    std::unique_lock<std::mutex> lock(m_mutex);
    for (;;)
    {
      Clock::time_point next = Clock::time_point::max();
      std::vector<Runner*> started = superviseLocked(Clock::now(), next);
      if (!started.empty())
      {
        lock.unlock();
        start(started);
        lock.lock();
        continue;
      }
      if (next == Clock::time_point::max())
      {
        m_superviseIdle = true;
        m_supervise.wait(lock);
        m_superviseIdle = false;
      }
      else
      {
        m_supervise.wait_until(lock, next);
      }
    }
  }

  // The next due worker this runner may step, taken in turn so none
  // starves; lowers next to the earliest timeout of the rest.
  rcp<WasmWorker> takeLocked(
      Runner* runner,
      Clock::time_point now,
      Clock::time_point& next)
  {
    size_t count = m_workers.size();
    for (size_t i = 0; i < count; i++)
    {
      size_t at = (m_next + i) % count;
      WasmWorker* worker = m_workers[at].get();
      if (worker->m_claimed ||
          (worker->m_pin != nullptr && worker->m_pin != runner) ||
#ifdef __EMSCRIPTEN__
          (worker->m_pin == nullptr && pinsLocked(runner) &&
           mayOwnRunnerLocked(worker->m_module.get())) ||
#endif
          overShareLocked(worker->m_module.get()) || !worker->due(now, next))
      {
        continue;
      }
      worker->m_claimed = true;
#ifdef __EMSCRIPTEN__
      worker->m_pin = runner;
#endif
      m_next = at + 1;
      return m_workers[at];
    }
    return nullptr;
  }

  // Promotes steps past their slice, then answers the runners it added to
  // keep the pool at its size, for the caller to start once unlocked.
  std::vector<Runner*> superviseLocked(
      Clock::time_point now,
      Clock::time_point& next)
  {
    if (m_resizing)
    {
      return {};
    }
    for (auto& runner : m_runners)
    {
      if (runner->slice == Slice::timing)
      {
        Clock::time_point over = runner->stepStart + kSlice;
        if (now < over)
        {
          next = std::min(next, over);
          continue;
        }
      }
      else if (runner->slice != Slice::kept)
      {
        continue;
      }
      runner->slice = promotedLocked(runner->stepping->m_module.get()) <
                              m_promotedPerVM
                          ? Slice::promoted
                          : Slice::kept;
    }
    std::vector<Runner*> started;
    for (uint32_t serving = servingLocked();
         !m_workers.empty() && serving < m_threads;
         serving++)
    {
      m_runners.push_back(std::make_unique<Runner>());
      started.push_back(m_runners.back().get());
    }
#ifdef __EMSCRIPTEN__
    // A runner each for the workers yet to take one of their own.
    uint32_t waiting = (uint32_t)std::count_if(
        m_workers.begin(),
        m_workers.end(),
        [this](const rcp<WasmWorker>& worker) {
          return worker->m_pin == nullptr &&
                 mayOwnRunnerLocked(worker->m_module.get());
        });
    uint32_t idle = (uint32_t)std::count_if(
        m_runners.begin(),
        m_runners.end(),
        [this](const auto& runner) { return !pinsLocked(runner.get()); });
    for (; idle < waiting; idle++)
    {
      m_runners.push_back(std::make_unique<Runner>());
      started.push_back(m_runners.back().get());
    }
#endif
    return started;
  }

#ifdef __EMSCRIPTEN__
  // An instance never leaves the thread it began on, so a VM's workers each
  // get a runner of their own, up to its cap, and share only past it.
  bool mayOwnRunnerLocked(const WasmLaneModule* module) const
  {
    uint32_t owned = (uint32_t)std::count_if(
        m_runners.begin(),
        m_runners.end(),
        [this, module](const auto& runner) {
          return std::any_of(
              m_workers.begin(),
              m_workers.end(),
              [&](const rcp<WasmWorker>& worker) {
                return worker->m_pin == runner.get() &&
                       worker->m_module.get() == module;
              });
        });
    return owned < lane_timing::kWebWorkerThreadsPerVM;
  }
#endif

  void start(const std::vector<Runner*>& runners)
  {
    for (Runner* runner : runners)
    {
      JobPool::makeThread([this, runner]() { runnerMain(runner); }).detach();
    }
  }

  // A VM's workers share its lane module.
  uint32_t countLocked(Slice slice, const WasmLaneModule* module) const
  {
    return (uint32_t)std::count_if(
        m_runners.begin(),
        m_runners.end(),
        [slice, module](const auto& runner) {
          return runner->slice == slice &&
                 runner->stepping->m_module.get() == module;
        });
  }

  uint32_t promotedLocked(const WasmLaneModule* module) const
  {
    return countLocked(Slice::promoted, module);
  }

  // The serving threads each VM with workers may keep with long steps, so
  // one VM's spinning workers never hold every VM's threads.
  uint32_t shareLocked() const
  {
    std::vector<const WasmLaneModule*> vms;
    for (const rcp<WasmWorker>& worker : m_workers)
    {
      if (std::find(vms.begin(), vms.end(), worker->m_module.get()) ==
          vms.end())
      {
        vms.push_back(worker->m_module.get());
      }
    }
    return vms.empty() ? m_threads : m_threads / (uint32_t)vms.size();
  }

  bool overShareLocked(const WasmLaneModule* module) const
  {
    uint32_t kept = countLocked(Slice::kept, module);
    return kept != 0 && kept >= shareLocked();
  }

  // Kept runners past their VM's share leave their slot to the others.
  uint32_t servingLocked() const
  {
    uint32_t share = shareLocked();
    uint32_t serving = 0;
    std::vector<const WasmLaneModule*> counted;
    for (const auto& runner : m_runners)
    {
      if (runner->slice == Slice::promoted)
      {
        continue;
      }
      if (runner->slice != Slice::kept)
      {
        serving++;
        continue;
      }
      const WasmLaneModule* module = runner->stepping->m_module.get();
      if (std::find(counted.begin(), counted.end(), module) == counted.end())
      {
        counted.push_back(module);
        serving += std::min(countLocked(Slice::kept, module), share);
      }
    }
    return serving;
  }

  // Answers the pool's reference, for the caller to drop unlocked.
  rcp<WasmWorker> eraseLocked(WasmWorker* worker)
  {
    auto found = std::find_if(
        m_workers.begin(),
        m_workers.end(),
        [worker](const rcp<WasmWorker>& held) { return held.get() == worker; });
    if (found == m_workers.end())
    {
      return nullptr;
    }
    rcp<WasmWorker> erased = std::move(*found);
    m_workers.erase(found);
    return erased;
  }

  bool pinsLocked(const Runner* runner) const
  {
    return std::any_of(
        m_workers.begin(),
        m_workers.end(),
        [runner](const rcp<WasmWorker>& worker) {
          return worker->m_pin == runner;
        });
  }

  std::mutex m_mutex;
  WakeWord m_wake;
  std::vector<rcp<WasmWorker>> m_workers;
  std::vector<std::unique_ptr<Runner>> m_runners;
  std::condition_variable m_runnersLeft;
  std::condition_variable m_supervise;
  // While configure waits for every runner to leave, none starts.
  bool m_resizing = false;
  bool m_supervising = false;
  // Set while no step is timed, so only a step starting then wakes it.
  bool m_superviseIdle = false;
  uint32_t m_threads;
  uint32_t m_promotedPerVM;
  size_t m_next = 0;
};
#endif

namespace
{
// What a worker's signal waits watch: nothing waits without threads.
WakeWord* watchWord()
{
#if RIVE_POOL_THREADS()
  return &WasmWorkerPool::instance().wakeWord();
#else
  return nullptr;
#endif
}
} // namespace

// --- workers -----------------------------------------------------------------

WasmWorker* WasmWorker::current() { return WasmLane::currentOf<WasmWorker>(); }

WasmWorker::WasmWorker(
    rcp<WasmLaneModule> module,
    uint32_t entry,
    std::vector<uint8_t> input)
    : WasmLane(kKind, std::move(module), entry, std::move(input))
{}

// The pool holds a threaded worker until it ends, so only one stepped at a
// poll can end here.
WasmWorker::~WasmWorker()
{
  if (m_begun && !m_ended)
  {
    end();
  }
}

void WasmWorker::stop()
{
  m_stopping.store(true);
#if RIVE_POOL_THREADS()
  if (WasmWorkerPool::instance().takeUnstarted(this))
  {
    end();
    return;
  }
  // A step already running never sees the stop; the one delivering it arms
  // its own grace.
  {
    std::lock_guard<std::mutex> lock(WasmWorkerPool::instance().mutex());
    if (m_claimed)
    {
      interruptAfter(kStopGraceMs);
    }
  }
  WasmWorkerPool::instance().wakeWord().wake();
#else
  // Only the VM's thread steps it.
  if (!m_begun && !m_ended)
  {
    end();
  }
#endif
}

void WasmWorker::finished(bool ok, const uint8_t* result, uint32_t size)
{
  // A crafted module may call again, while the VM's thread reads the
  // result.
  if (m_finished)
  {
    return;
  }
  m_finished = true;
  // On the web the page copied the result in already.
  if (ok && result != nullptr)
  {
    m_result.assign(result, result + size);
  }
  m_state.store(ok ? LaneState::done : LaneState::failed);
}

// --- waits -------------------------------------------------------------------

uint32_t WasmWorker::addWait(
    uint32_t signal,
    double after,
    double timeoutSeconds)
{
  rcp<SharedSignal> found = SignalWait::find(signal, sharedOwner());
  m_waits.emplace_back(
      m_nextWait++,
      sharedOwner(),
      signal,
      found.get(),
      after,
      timeoutSeconds);
  WakeWord* word = watchWord();
  if (found != nullptr && word != nullptr)
  {
    found->addWatcher(word);
  }
  return m_waits.back().id;
}

// A signal already gone keeps no watchers anyone could notify.
void WasmWorker::unwatch(const SignalWait& wait)
{
  WakeWord* word = watchWord();
  rcp<SharedSignal> found = SignalWait::find(wait.signal, wait.owner);
  if (found != nullptr && word != nullptr)
  {
    found->removeWatcher(word);
  }
}

void WasmWorker::dropWaits()
{
  for (const SignalWait& wait : m_waits)
  {
    unwatch(wait);
  }
  m_waits.clear();
}

// --- the lifecycle -----------------------------------------------------------

WasmWorker::Step WasmWorker::run()
{
  Step result;
  if (!m_begun)
  {
    // A worker stopped before it began never runs.
    result = !m_stopping.load() && begin() ? Step::progressed : Step::done;
  }
  else
  {
    result = step();
  }
  if (result == Step::done)
  {
    // Only the stepping thread can hold what its trap left locked.
    releaseTrappedRegion();
    end();
  }
  return result;
}

bool WasmWorker::due(Clock::time_point now, Clock::time_point& earliest) const
{
  if (m_ended)
  {
    return false;
  }
  if (!m_begun || m_stopping.load())
  {
    return true;
  }
  for (const SignalWait& wait : m_waits)
  {
    double value;
    if (wait.fired(now, value))
    {
      return true;
    }
    earliest = std::min(earliest, wait.deadline);
  }
  return false;
}

bool WasmWorker::begin()
{
  m_begun = true;
  WasmLane* outerLane = WasmLane::setCurrent(this);
  bool ok = startInstance() && collect();
  if (ok && !m_finished)
  {
    m_state.store(LaneState::running);
  }
  WasmLane::setCurrent(outerLane);
  return ok && !m_finished;
}

WasmWorker::Step WasmWorker::step()
{
  // A worker that awaits again after its stop was delivered would be woken
  // with it forever; it is done.
  if (m_finished || m_stopDelivered)
  {
    return Step::done;
  }
  std::vector<std::pair<uint32_t, double>> ready;
  auto now = Clock::now();
  bool stopping = m_stopping.load();
  for (auto it = m_waits.begin(); it != m_waits.end();)
  {
    double value = kStopping;
    if (!stopping && !it->fired(now, value))
    {
      ++it;
      continue;
    }
    ready.push_back({it->id, value});
    unwatch(*it);
    it = m_waits.erase(it);
  }
  if (ready.empty())
  {
    // A stopped worker with nothing parked is stalled on a promise no
    // host wakes; it is done.
    return stopping ? Step::done : Step::idle;
  }
  if (stopping)
  {
    interruptAfter(kStopGraceMs);
  }
  WasmLane* outerLane = WasmLane::setCurrent(this);
  bool ok = true;
  for (const auto& [id, value] : ready)
  {
    m_stopDelivered = m_stopDelivered || value == kStopping;
    if (!wake(id, value))
    {
      ok = false;
      break;
    }
  }
  ok = ok && collect();
  WasmLane::setCurrent(outerLane);
  return ok && !m_finished ? Step::progressed : Step::done;
}

void WasmWorker::end()
{
  dropWaits();
  endInstance();
  // Published after the instance ends, so a stop it prompts finds none.
  if (!m_finished && m_state.load() < LaneState::done)
  {
    m_state.store(m_stopping.load() ? LaneState::stopped : LaneState::failed);
  }
  m_ended.store(true);
}

// --- the WAMR backend --------------------------------------------------------

#ifndef __EMSCRIPTEN__
namespace
{
void workerFinished(
    wasm_exec_env_t,
    uint32_t ok,
    const uint8_t* result,
    uint32_t size)
{
  if (WasmWorker* worker = WasmWorker::current())
  {
    worker->finished(ok != 0, result, size);
  }
}

uint32_t workerWait(
    wasm_exec_env_t,
    uint32_t signal,
    double after,
    double timeoutSeconds)
{
  WasmWorker* worker = WasmWorker::current();
  return worker != nullptr ? worker->addWait(signal, after, timeoutSeconds) : 0;
}

uint32_t workerStopping(wasm_exec_env_t)
{
  WasmWorker* worker = WasmWorker::current();
  return worker != nullptr && worker->stopping() ? 1 : 0;
}

NativeSymbol kWorkerNatives[] = {
    {"finished", (void*)workerFinished, "(i*~)", nullptr},
    {"wait", (void*)workerWait, "(iFF)i", nullptr},
    {"stopping", (void*)workerStopping, "()i", nullptr},
};

constexpr uint32_t kWorkerStack = 512 * 1024;
} // namespace

void rive::registerWorkerNatives()
{
  static std::once_flag once;
  std::call_once(once, []() {
    wasm_runtime_register_natives(
        "rive_worker_v1",
        kWorkerNatives,
        sizeof(kWorkerNatives) / sizeof(kWorkerNatives[0]));
  });
}

bool WasmWorker::startInstance()
{
  if (!instantiate(kWorkerStack))
  {
    return false;
  }
  if (m_stopping.load())
  {
    interruptAfter(kStopGraceMs);
  }
  m_collectFn = lookup(lane_layout::kFrameMinorExport, 0, 1, true);
  uint32_t argv[2] = {};
  if (!runStart(argv))
  {
    failTrap();
    return false;
  }
  uint32_t at = argv[0];
  if (!call(m_module->entryNames[m_entry].c_str(), 2, argv))
  {
    failTrap();
    return false;
  }
  argv[0] = at;
  if (!call(lane_layout::kLaneFreeExport, 1, argv))
  {
    failTrap();
    return false;
  }
  return true;
}

bool WasmWorker::wake(uint32_t id, double value)
{
  if (m_wakeFn == nullptr &&
      (m_wakeFn = lookup(lane_layout::kWorkerWakeExport, 3, 3)) == nullptr)
  {
    return false;
  }
  wasm_val_t args[2];
  args[0].kind = WASM_I32;
  args[0].of.i32 = (int32_t)id;
  args[1].kind = WASM_F64;
  args[1].of.f64 = value;
  if (!wasm_runtime_call_wasm_a(
          (wasm_exec_env_t)m_execEnv,
          (wasm_function_inst_t)m_wakeFn,
          0,
          nullptr,
          2,
          args))
  {
    failTrap();
    return false;
  }
  return true;
}

// Back at the host no frame is live, so the frame collector may run.
bool WasmWorker::collect()
{
  uint32_t argv[1] = {0};
  if (m_collectFn != nullptr && !call(m_collectFn, 0, argv))
  {
    failTrap();
    return false;
  }
  return true;
}

void WasmWorker::endInstance()
{
  m_wakeFn = nullptr;
  m_collectFn = nullptr;
  teardown();
}

#else
// --- the browser backend -----------------------------------------------------

// The page runner calls these back from a worker's natives, with the worker
// it bound them to.
extern "C" EMSCRIPTEN_KEEPALIVE uint32_t rive_web_worker_wait(
    uint32_t worker,
    uint32_t signal,
    double after,
    double timeout)
{
  return ((WasmWorker*)(uintptr_t)worker)->addWait(signal, after, timeout);
}

extern "C" EMSCRIPTEN_KEEPALIVE void rive_web_worker_finished(
    uint32_t worker,
    uint32_t ok)
{
  ((WasmWorker*)(uintptr_t)worker)->finished(ok != 0, nullptr, 0);
}

extern "C" EMSCRIPTEN_KEEPALIVE uint32_t
rive_web_worker_stopping(uint32_t worker)
{
  return ((WasmWorker*)(uintptr_t)worker)->stopping() ? 1 : 0;
}

// clang-format off
EM_JS_DEPS(riveWebWorkers, "$stringToNewUTF8");

// These answer null, or a malloc'd error.
EM_JS(char*, riveWebWorkerStart,
      (uint32_t worker, uint32_t lane, uint32_t moduleId,
       const uint8_t* module, uint32_t moduleSize, uint32_t entry,
       const uint8_t* input, uint32_t inputSize, void* output,
       uint32_t* idPtr), {
    let finished = false;
    const natives = {
        wait: (signal, after, timeout) =>
            _rive_web_worker_wait(worker, signal, after, timeout),
        finished: (ok, result) => {
            // The VM's thread may be reading the first result.
            if (finished) {
                return;
            }
            finished = true;
            if (ok) {
                // Resizing may grow the heap, which replaces HEAPU8.
                const at = _rive_web_job_output(output, result.length);
                HEAPU8.set(result, at);
            }
            _rive_web_worker_finished(worker, ok);
        },
        stopping: () => _rive_web_worker_stopping(worker),
    };
    const result = Module['riveScripting']['workerStart'](
        lane,
        moduleId,
        () => HEAPU8.slice(module, module + moduleSize),
        entry,
        HEAPU8.slice(input, input + inputSize),
        natives);
    if (!result.ok) {
        return stringToNewUTF8(result.error);
    }
    HEAPU32[idPtr >> 2] = result.id;
    return 0;
});

EM_JS(char*, riveWebWorkerWake, (uint32_t id, uint32_t waitId, double value), {
    const error = Module['riveScripting']['workerWake'](id, waitId, value);
    return error === null ? 0 : stringToNewUTF8(error);
});

EM_JS(char*, riveWebWorkerCollect, (uint32_t id), {
    const error = Module['riveScripting']['workerCollect'](id);
    return error === null ? 0 : stringToNewUTF8(error);
});

EM_JS(void, riveWebWorkerEnd, (uint32_t id), {
    Module['riveScripting']['workerEnd'](id);
});
// clang-format on

bool WasmWorker::startInstance()
{
  if (!riveWebAttach())
  {
    fail("no script runner on this thread");
    return false;
  }
  if (m_stopping.load())
  {
    interruptAfter(kStopGraceMs);
  }
  std::vector<uint8_t> input = takeInput();
  char* error = riveWebWorkerStart(
      (uint32_t)(uintptr_t)this,
      (uint32_t)(uintptr_t)(WasmLane*)this,
      m_module->id,
      m_module->bytes.data(),
      (uint32_t)m_module->bytes.size(),
      m_entry,
      input.data(),
      (uint32_t)input.size(),
      &m_result,
      &m_webInstance);
  if (error != nullptr)
  {
    failTaken(error);
    return false;
  }
  return true;
}

bool WasmWorker::wake(uint32_t id, double value)
{
  char* error = riveWebWorkerWake(m_webInstance, id, value);
  if (error != nullptr)
  {
    failTaken(error);
    return false;
  }
  return true;
}

bool WasmWorker::collect()
{
  char* error = riveWebWorkerCollect(m_webInstance);
  if (error != nullptr)
  {
    failTaken(error);
    return false;
  }
  return true;
}

void WasmWorker::endInstance()
{
  if (m_webInstance != 0)
  {
    riveWebWorkerEnd(m_webInstance);
    m_webInstance = 0;
  }
}

#endif

// --- the set -----------------------------------------------------------------

void WasmWorkers::configurePool(uint32_t threads, uint32_t promotedPerVM)
{
#if RIVE_POOL_THREADS()
  WasmWorkerPool::instance().configure(threads, promotedPerVM);
#else
  (void)threads;
  (void)promotedPerVM;
#endif
}

WasmWorkers::~WasmWorkers()
{
  for (auto& worker : m_lanes)
  {
    worker->stop();
  }
#if RIVE_POOL_THREADS()
  auto deadline = Clock::now() + std::chrono::milliseconds(kStopGraceMs);
  for (auto& worker : m_lanes)
  {
    WasmWorkerPool::instance().join(worker.get(), deadline);
  }
#endif
}

rcp<WasmWorker> WasmWorkers::spawn(
    uint32_t entry,
    std::vector<uint8_t> input,
    uint32_t pageThreadBudgetMs)
{
  if (entry >= entryCount())
  {
    return nullptr;
  }
  auto worker = add(make_rcp<WasmWorker>(m_module, entry, std::move(input)));
  worker->setPageThreadBudgetMs(pageThreadBudgetMs);
#if RIVE_POOL_THREADS()
  WasmWorkerPool::instance().add(worker);
#endif
  return worker;
}

void WasmWorkers::pump()
{
#if !RIVE_POOL_THREADS()
  // A worker parked on something already true is always due, so the bound
  // is time.
  auto until = Clock::now() +
               std::chrono::milliseconds(lane_timing::kCooperativeSliceMs);
  bool progressed = true;
  while (progressed && Clock::now() < until)
  {
    progressed = false;
    for (auto& worker : m_lanes)
    {
      if (worker->over())
      {
        continue;
      }
      PageThreadBudget budget(*worker);
      if (worker->run() == WasmWorker::Step::progressed)
      {
        progressed = true;
      }
    }
  }
#endif
}

bool WasmWorkers::due() const
{
  Clock::time_point now = Clock::now();
  Clock::time_point earliest = Clock::time_point::max();
#if RIVE_POOL_THREADS()
  std::lock_guard<std::mutex> lock(WasmWorkerPool::instance().mutex());
#endif
  bool due = std::any_of(
      m_lanes.begin(),
      m_lanes.end(),
      [&](const rcp<WasmWorker>& worker) {
#if RIVE_POOL_THREADS()
        if (worker->m_claimed)
        {
          return false;
        }
#endif
        return worker->due(now, earliest);
      });
#if !RIVE_POOL_THREADS()
  // Only polls step a worker here, so a timed wait keeps them coming.
  due = due || earliest != Clock::time_point::max();
#endif
  return due;
}

bool WasmWorkers::idle() const
{
  Clock::time_point now = Clock::now();
  Clock::time_point earliest = Clock::time_point::max();
#if RIVE_POOL_THREADS()
  std::lock_guard<std::mutex> lock(WasmWorkerPool::instance().mutex());
#endif
  return std::none_of(
      m_lanes.begin(),
      m_lanes.end(),
      [&](const rcp<WasmWorker>& worker) {
#if RIVE_POOL_THREADS()
        // A stop that took an unstarted worker leaves it
        // claimed.
        if (worker->m_claimed && !worker->over())
        {
          return true;
        }
#endif
        return worker->due(now, earliest);
      });
}

void WasmWorkers::parkedLines(std::vector<std::string>& lines) const
{
  Clock::time_point now = Clock::now();
  std::vector<std::pair<const WasmWorker*, std::vector<SignalWait>>> parked;
  {
#if RIVE_POOL_THREADS()
    std::lock_guard<std::mutex> lock(WasmWorkerPool::instance().mutex());
#endif
    Clock::time_point earliest = Clock::time_point::max();
    for (const auto& worker : m_lanes)
    {
#if RIVE_POOL_THREADS()
      if (worker->m_claimed)
      {
        continue;
      }
#endif
      if (!worker->over() && !worker->due(now, earliest))
      {
        parked.emplace_back(worker.get(), worker->m_waits);
      }
    }
  }
  for (const auto& [worker, waits] : parked)
  {
    std::string name = worker->name();
    if (waits.empty())
    {
      lines.push_back(
          name + " is still waiting on a promise nothing will settle");
    }
    for (const SignalWait& wait : waits)
    {
      std::string what = "sleeping";
      if (wait.signal != 0)
      {
        bool space = false;
        uint32_t ring = SharedRegistry::ringOf(wait.signal, wait.owner, space);
        if (ring == 0)
        {
          what = "waiting for signal " + std::to_string(wait.signal);
        }
        else
        {
          what = std::string("waiting for a ") +
                 (space ? "read from" : "write to") + " ring " +
                 std::to_string(ring);
        }
      }
      char seconds[32];
      snprintf(
          seconds,
          sizeof(seconds),
          "%.2f",
          std::chrono::duration<double>(now - wait.since).count());
      lines.push_back(name + " is still " + what + " after " + seconds + " s");
    }
  }
}

#endif
