/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/async/compare_exchange.hpp"
#include "rive/async/threading.hpp"
#include "rive/refcnt.hpp"
#include "rive/span.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

// Where a lane thread chooses its own stack, as std::thread cannot; a web
// build's pthread stacks come out of its own wasm memory.
#if defined(_WIN32)
#define RIVE_LANE_NATIVE_THREADS() 1
#elif !defined(__EMSCRIPTEN__) && (defined(__APPLE__) || defined(__linux__))
#define RIVE_LANE_NATIVE_THREADS() 1
#include <pthread.h>
#else
#define RIVE_LANE_NATIVE_THREADS() 0
#endif

namespace rive
{

/// One unit of script job work. A task runs at most once, on whichever thread
/// claims it first: a pool thread, or a joiner that finds it unstarted.
class JobTask : public RefCnt<JobTask>
{
public:
  enum class State : uint8_t
  {
    pending,
    running,
    done,
    cancelled,
  };

  virtual ~JobTask() = default;

  State state() const { return (State)m_state.load(std::memory_order_acquire); }
  bool finished() const
  {
    State s = state();
    return s == State::done || s == State::cancelled;
  }

  /// Pending to running, exactly once across every thread.
  bool tryClaim()
  {
    uint8_t expected = (uint8_t)State::pending;
    return compareExchangeStrong(
        m_state,
        expected,
        (uint8_t)State::running,
        std::memory_order_acq_rel);
  }

  bool cancelRequested() const
  {
    return m_cancelRequested.load(std::memory_order_acquire);
  }

protected:
  /// Runs on the claiming thread, to its end.
  virtual void run() = 0;
  /// A cancel reached the task while it runs; stop it promptly.
  virtual void interrupt() {}

private:
  friend class JobPool;
  std::atomic<uint8_t> m_state{(uint8_t)State::pending};
  std::atomic<bool> m_cancelRequested{false};
};

/// The order pool threads take queued tasks in. Results never depend on it;
/// the non default orders exist to prove that in tests.
enum class JobSchedule : uint8_t
{
  fifo,
#ifdef WITH_RIVE_TOOLS
  reverse,
  shuffle,
#endif
};

#if RIVE_LANE_NATIVE_THREADS()
#ifdef _WIN32
// A HANDLE, without windows.h.
using LaneNativeThread = void*;
#else
using LaneNativeThread = pthread_t;
#endif
#endif

/// A thread that runs script lanes: the factory's, or one with kStackBytes of
/// stack.
class LaneThread
{
public:
  /// As much as a main thread has on desktops and Android, so code that runs
  /// on the main lane runs on every lane.
  static constexpr size_t kStackBytes = 8u << 20;

  LaneThread() = default;
  explicit LaneThread(std::thread thread) : m_thread(std::move(thread)) {}
  LaneThread(LaneThread&& other) noexcept;
  LaneThread& operator=(LaneThread&&) = delete;
  LaneThread(const LaneThread&) = delete;
  ~LaneThread();

  static LaneThread start(std::function<void()> body);
  void join();
  void detach();

private:
  std::thread m_thread;
#if RIVE_LANE_NATIVE_THREADS()
  LaneNativeThread m_native{};
  bool m_hasNative = false;
#endif
};

/// Script jobs' pool, apart from the decodes' WorkPool. Without threads a
/// task runs only when joined or pumped, every platform's inline fallback.
class JobPool
{
public:
  using ThreadFactory = std::function<std::thread(std::function<void()>)>;

  JobPool(
      uint32_t threadCount,
      JobSchedule schedule = JobSchedule::fifo,
      uint64_t seed = 0);
  ~JobPool();

  JobPool(const JobPool&) = delete;
  JobPool& operator=(const JobPool&) = delete;

  /// Consoles set this to pick cores, priorities and stack sizes.
  static void setThreadFactory(ThreadFactory factory);
  /// A thread from the factory, for every thread that runs script lanes.
  static LaneThread makeThread(std::function<void()> body);

  uint32_t threadCount() const
  {
#if RIVE_POOL_THREADS()
    return (uint32_t)m_threads.size();
#else
    return 0;
#endif
  }

  void submit(rcp<JobTask> task);

  /// Runs an unstarted task here to its end when runHere, else waits up
  /// to timeoutSeconds for a thread to, if there are any; false when it
  /// is still unfinished.
  bool join(JobTask* task, double timeoutSeconds, bool runHere = true);

  /// Index of the first finished task, else with runHere of the lowest
  /// unstarted one, which it runs as join does; -1 when none finished.
  int joinAny(Span<JobTask*> tasks, double timeoutSeconds, bool runHere = true);

  /// Pending tasks are dropped; a running task is asked to stop.
  void cancel(JobTask* task);

  /// Runs up to max queued tasks on the caller; the zero thread pump.
  uint32_t runPending(uint32_t max);
  /// Runs queued tasks on the caller while within seconds, at least one;
  /// nothing bounds a task once it runs.
  uint32_t runPendingFor(double seconds);

  bool hasQueued();

private:
  void workerLoop();
  rcp<JobTask> takeLocked();
  uint32_t runQueued(uint32_t max, std::chrono::steady_clock::time_point until);
  void execute(rcp<JobTask> task);
  void notifyFinished();

  // rive-cli defines TESTING for its tests only, not its library.
#ifdef WITH_RIVE_TOOLS
  JobSchedule m_schedule;
  std::mt19937_64 m_rng;
#endif
  std::mutex m_mutex;
  std::condition_variable m_workCv;
  std::condition_variable m_doneCv;
  std::deque<rcp<JobTask>> m_queue;
  std::vector<LaneThread> m_threads;
  bool m_stopping = false;
};

} // namespace rive
