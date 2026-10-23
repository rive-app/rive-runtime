#ifndef _RIVE_WASM_JOBS_HPP_
#define _RIVE_WASM_JOBS_HPP_

#ifdef WITH_RIVE_SCRIPTING_WASM

#include "rive/async/job_pool.hpp"
#include "rive/wasm/wasm_lane.hpp"
#include "rive/refcnt.hpp"
#include "rive/span.hpp"

#include <vector>

namespace rive
{
/// One run of a jobs module entry: bytes in, bytes out, in an instance as
/// instantiation leaves it, on whichever thread claims it.
class WasmJob : public JobTask, public WasmLane
{
public:
  static constexpr uint32_t kJoinRefused = lane_layout::kJoinRefused;
  static constexpr LaneKind kKind = LaneKind::job;

  WasmJob(
      rcp<WasmLaneModule> module,
      uint32_t entry,
      std::vector<uint8_t> input);

  LaneState state() const override;
  void stop() override;
  /// Valid once finished.
  const std::vector<uint8_t>& output() const { return m_output; }

  /// The job running on this thread, for the natives its module imports.
  static WasmJob* current();

protected:
  void run() override;
  void interrupt() override;

private:
  bool execute();
#ifndef __EMSCRIPTEN__
  // The entry answers the address of (pointer, length) of its result.
  bool takeResult(uint32_t record);
  void park();
#endif

  bool m_failed = false;
  // A cancel that reached the job while it ran ended it.
  bool m_stopped = false;
  std::vector<uint8_t> m_output;
#ifdef __EMSCRIPTEN__
  // Set by a cancel, for the job's cancelled() to read.
  std::atomic<int32_t> m_cancelWord{0};
#endif
};

/// A VM's jobs: the module carried in its main module's "rive.jobs" custom
/// section, and every job the VM started.
class WasmJobs : public WasmLaneSet<WasmJob>
{
public:
  using WasmLaneSet::WasmLaneSet;
  ~WasmJobs();

  /// Reconfigures the process wide job pool. Tests use it to run the same
  /// jobs at every thread count and schedule.
  static void configurePool(
      uint32_t threads,
      JobSchedule schedule = JobSchedule::fifo,
      uint64_t seed = 0);
  static JobPool& pool();
  /// With no pool threads a job runs only when joined or here, at each poll:
  /// at least one, more within the cooperative slice, each to its end.
  static void runQueuedInline();
  /// A run of the job holding the page's thread may take
  /// pageThreadBudgetMs, 0 for no limit.
  rcp<WasmJob> run(
      uint32_t entry,
      std::vector<uint8_t> input,
      uint32_t pageThreadBudgetMs);
  /// Joins within the usableSeconds a caller may spend, infinity for none:
  /// only a caller with none runs the job here, to its end, and no wait
  /// outlasts them.
  bool join(WasmJob* job, double timeoutSeconds, double usableSeconds);
  int joinAny(Span<WasmJob*> jobs, double timeoutSeconds, double usableSeconds);
  /// A started job that has not finished.
  bool hasUnfinished() const;
};

} // namespace rive

#endif
#endif
