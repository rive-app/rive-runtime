#ifndef _RIVE_WASM_WORKERS_HPP_
#define _RIVE_WASM_WORKERS_HPP_

#ifdef WITH_RIVE_SCRIPTING_WASM

#include "rive/async/threading.hpp"
#include "rive/refcnt.hpp"
#include "rive/wasm/wasm_lane.hpp"
#include "rive/wasm/wasm_shared.hpp"
#include "rive/wasm/wasm_signal_wait.hpp"

#include <atomic>
#include <chrono>
#include <vector>

namespace rive
{
class WasmWorkerPool;

/// A long lived async @worker in its own module instance. It never blocks:
/// the host resumes its parked awaits in steps on the pool or at each poll.
class WasmWorker : public RefCnt<WasmWorker>, public WasmLane
{
public:
  static constexpr LaneKind kKind = LaneKind::worker;

  WasmWorker(
      rcp<WasmLaneModule> module,
      uint32_t entry,
      std::vector<uint8_t> input);
  ~WasmWorker() override;

  /// Its steps are done.
  bool over() const override { return m_ended.load(); }
  /// Asks the worker to finish: its awaits settle as stopping.
  void stop() override;

  LaneState state() const override { return m_state.load(); }
  /// What its promise resolved with; valid once done, after which it never
  /// changes.
  const std::vector<uint8_t>& result() const { return m_result; }

  // The worker's natives, on the thread stepping it.
  uint32_t addWait(uint32_t signal, double after, double timeoutSeconds);
  bool stopping() const { return m_stopping.load(); }
  void finished(bool ok, const uint8_t* result, uint32_t size);
  static WasmWorker* current();

private:
  friend class WasmWorkerPool;
  friend class WasmWorkers;
  using Clock = std::chrono::steady_clock;
  static constexpr double kStopping = lane_layout::kWaitStopping;

  // The lifecycle both backends share: begin runs the entry to its first
  // await, each step wakes what fired, and end tears the instance down.
  enum class Step
  {
    progressed,
    idle,
    done,
  };
  Step run();
  /// Whether a step would do anything: it has not begun, a wait fired or
  /// a stop is pending. Lowers earliest to its next timeout.
  bool due(Clock::time_point now, Clock::time_point& earliest) const;
  bool begin();
  Step step();
  void end();
  void dropWaits();
  void unwatch(const SignalWait& wait);

  // What each backend runs a worker on: WAMR natively, the page runner on
  // the web.
  bool startInstance();
  bool wake(uint32_t id, double value);
  bool collect();
  void endInstance();

  std::atomic<LaneState> m_state{LaneState::starting};
  std::atomic<bool> m_stopping{false};
  std::atomic<bool> m_ended{false};
  bool m_finished = false;
  std::vector<uint8_t> m_result;
  // Touched only by the thread stepping it, which the pool hands over
  // under its lock.
  std::vector<SignalWait> m_waits;
  uint32_t m_nextWait = 1;
  bool m_stopDelivered = false;
  bool m_begun = false;
#if RIVE_POOL_THREADS()
  // Guarded by the pool's lock.
  bool m_claimed = false;
  // Set once a long step of it got its own thread.
  bool m_promoted = false;
  // The thread its instance lives on, where instances cannot move.
  const void* m_pin = nullptr;
#endif
#ifdef __EMSCRIPTEN__
  // The page runner's id for this worker's instance.
  uint32_t m_webInstance = 0;
#else
  void* m_wakeFn = nullptr;
  void* m_collectFn = nullptr;
#endif
};

/// A VM's workers: the module carried in its main module's "rive.workers"
/// section, and every worker the VM spawned.
class WasmWorkers : public WasmLaneSet<WasmWorker>
{
public:
  using WasmLaneSet::WasmLaneSet;
  /// Stops every worker and waits for them.
  ~WasmWorkers();

  /// For tests, with every worker done or ending: sizes the pool and the
  /// threads long steps may take per VM, zero restoring the platform's sizes.
  static void configurePool(uint32_t threads, uint32_t promotedPerVM);

  /// A step of the worker holding the page's thread may take
  /// pageThreadBudgetMs, 0 for no limit.
  rcp<WasmWorker> spawn(
      uint32_t entry,
      std::vector<uint8_t> input,
      uint32_t pageThreadBudgetMs);
  /// At each poll, steps workers where there are no threads.
  void pump();
  /// Whether a worker needs a step now, or without threads at a timeout,
  /// so its host keeps advancing.
  bool due() const;
  /// No worker is mid step or needs one now; a wait on time that has not
  /// passed counts as idle.
  bool idle() const;
  /// A line for each wait of a worker parked on awaits, naming the worker,
  /// what it awaits and how long it has waited.
  void parkedLines(std::vector<std::string>& lines) const;
};

} // namespace rive

#endif
#endif
