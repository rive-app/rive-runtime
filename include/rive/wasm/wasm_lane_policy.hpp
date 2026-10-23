#ifndef _RIVE_WASM_LANE_POLICY_HPP_
#define _RIVE_WASM_LANE_POLICY_HPP_

#ifdef WITH_RIVE_SCRIPTING_WASM

#include <cstdint>

namespace rive
{

/// The limits a host sets on scripts, applied to files loaded after it.
/// Jobs and workers have none, except a call's budget on the page's thread.
struct ScriptingLimits
{
  /// How long one call into a file's scripts may run, 0 for no limit.
  uint32_t mainCallBudgetMs = 200;
  /// How long one block of an audio processor may take, in blocks of
  /// audio, 0 for no limit; a block past it silences only that processor.
  /// Offline renders have no limit.
  uint32_t audioHardBlocks = 2;
  /// The shared memory one file's scripts and their lanes may hold at once.
  uint64_t sharedBytesPerFile = 512ull << 20;
  /// The threads of the process wide job and worker pools, 0 to size them
  /// to the device; read when each pool is made.
  uint32_t jobThreads = 0;
  uint32_t workerThreads = 0;

  /// Safe from any thread.
  static void set(const ScriptingLimits& limits);
  static ScriptingLimits get();
};

/// Timings the runtime keeps for itself.
namespace lane_timing
{
/// A pool worker's step running past this gets its thread to itself.
constexpr uint32_t kWorkerSliceMs = 20;
/// A web worker stays on the thread it began on, so each gets one of its own
/// up to this many per VM, sharing only past it.
constexpr uint32_t kWebWorkerThreadsPerVM = 8;
/// How long each poll runs lanes on the VM's thread where there are no
/// threads to run them.
constexpr uint32_t kCooperativeSliceMs = 2;
/// How long a stopped worker that never awaits again runs before it is
/// interrupted.
constexpr uint32_t kWorkerStopGraceMs = 50;
/// The longest a wait lasts without a call deadline, on the web's main
/// thread and elsewhere.
constexpr uint32_t kMainWaitMs = 100;
constexpr uint32_t kLaneWaitMs = 2000;
} // namespace lane_timing

} // namespace rive

#endif
#endif
