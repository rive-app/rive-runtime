#ifndef _RIVE_MODULE_TIER_LADDER_HPP_
#define _RIVE_MODULE_TIER_LADDER_HPP_

#ifdef WITH_RIVE_SCRIPTING_WASM

// An editor/tools feature: the ladder spawns wamrc subprocesses. Runtime
// builds stub it out and stay on interp (prelinked artifacts load by
// registry instead), and the web runs modules on the browser's engine.
#if defined(WITH_RIVE_TOOLS) && !defined(__EMSCRIPTEN__)
#define RIVE_WASM_TIER_LADDER 1
#endif

#include "rive/span.hpp"
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

#ifdef RIVE_WASM_TIER_LADDER
#include <map>
#include <mutex>
#include <set>
#include <tuple>
#include <utility>
#endif

namespace rive
{

// Compiled-artifact species, in ascending speed. Interp is the implicit
// tier 0 a module runs on without one.
enum class TierSpecies : uint8_t
{
  o0 = 0, // wamrc -O0: ~2x interp, sub-second for typical modules
  o3 = 1, // wamrc top opt level with sw bounds, runs on every build
  // No inline bounds checks; needs the guard-page trap handler, so only
  // RIVE_WASM_HW_BOUNDS builds produce or load these.
  hw = 2,
};

// The species a VM boot looks up and compiles, so other compile sites make
// exactly what a boot will load: the top species, or -O0 when
// RIVE_WASM_AOT_SYNC=o0.
TierSpecies bootTierSpecies();

// 3 when cpuid leaf 1 ecx, leaf 7 ebx, leaf 0x80000001 ecx and xcr0 show
// every x86-64-v3 feature with its AVX state saved, else 2.
int x86_64Level(
    uint32_t leaf1Ecx,
    uint32_t leaf7Ebx,
    uint32_t extEcx,
    uint64_t xcr0);

// What a compileSync call produced.
struct TierCompile
{
  // The artifact, empty when the module has to run interpreted.
  std::string path;
  // wamrc made the artifact on this call rather than the cache holding it.
  bool compiled = false;
  // Why path is empty, for a log line.
  std::string reason;
};

#ifdef RIVE_WASM_TIER_LADDER

// The absolute path of the binary holding the ladder, whose stat keys
// rejected markers, or empty where the platform cannot say.
std::string loaderBinaryPath();

// Drives wamrc subprocesses that turn wasm modules into AOT artifacts.
// File-in/file-out, no IPC: compile crashes and LLVM's RSS stay in the
// child. Process-wide like the shared module cache; safe to call from any
// thread.
class ModuleTierLadder
{
public:
  static ModuleTierLadder& instance();

  // Inert until a host opts in. Empty arguments fall back to RIVE_WAMRC,
  // RIVE_AOT_CACHE_DIR, then a user cache. compileOnBoot blocks on a miss.
  void configure(
      const std::string& wamrcPath,
      const std::string& cacheDir,
      bool compileOnBoot = false);
  // Back to the inert state a process starts in.
  void unconfigure();
  bool enabled();
  // Why enabled() is false, for a log line; empty when it is true.
  std::string disabledReason();
  // The cache dir when other users could write it, which keeps the ladder
  // off, else empty.
  std::string untrustedCacheDir();
  // The wamrc the ladder runs, for callers compiling their own way.
  std::string compilerPath();

  // Ready artifact path for a module at the given species, empty if none.
  // A target names a device to compile for instead of this machine, such
  // as "android-aarch64".
  std::string artifactPath(
      uint64_t moduleKey,
      TierSpecies species,
      const std::string& target = std::string());

  // Compile one species on the calling thread, or hand back the cached
  // artifact at once. Setting cancel, then calling cancelCompiles, stops
  // the compile, even one that has not spawned yet.
  TierCompile compileSync(
      uint64_t moduleKey,
      Span<const uint8_t> moduleBytes,
      TierSpecies species,
      const std::string& target = std::string(),
      const std::atomic<bool>* cancel = nullptr);
  // compileSync for each module keyed by its bytes, side by side, the first
  // on the calling thread, with results in module order.
  std::vector<TierCompile> compileAll(
      const std::vector<Span<const uint8_t>>& modules,
      TierSpecies species,
      const std::atomic<bool>* cancel = nullptr);
  // compileAll of a script module and the lane modules it carries, the
  // script module's result first.
  std::vector<TierCompile> compileWithLanes(
      Span<const uint8_t> moduleBytes,
      TierSpecies species,
      const std::atomic<bool>* cancel = nullptr);
  // What a VM boot loads: compileWithLanes with compileOnBoot, else only
  // moduleKey's cached artifact, since a host's frame thread may boot it.
  TierCompile bootCompile(
      uint64_t moduleKey,
      Span<const uint8_t> moduleBytes,
      TierSpecies species);

  // Kills the compiles whose cancel flag is set, which then return empty.
  void cancelCompiles();

  // Failures hold until the next configure, which recompiles a rejected
  // artifact unless the runtime rejected it straight from its compile.
  bool failed(
      uint64_t moduleKey,
      TierSpecies species,
      const std::string& target = std::string());
  void rejectArtifact(
      uint64_t moduleKey,
      TierSpecies species,
      const std::string& reason);

private:
  ModuleTierLadder() = default;
  ~ModuleTierLadder() = default;

  struct Job
  {
    uint64_t moduleKey = 0;
    TierSpecies species = TierSpecies::o0;
    std::string wasmPath;
    std::string finalPath;
    std::string target;
    // The child's pid, or its process handle on Windows.
    intptr_t child = -1;
    bool cancelled = false;
    const std::atomic<bool>* cancel = nullptr;
  };

  // Why the compile failed, empty when it succeeded.
  std::string runWamrc(Job& job);
  bool enabledLocked();
  // True when the cache or a past failure settles the compile.
  bool lookupLocked(Job& job, TierCompile& result);
  std::string artifactName(uint64_t moduleKey, TierSpecies species);
  std::string keyedCacheDir();
  std::string artifactDir(const std::string& target);
  void createArtifactDir(const std::string& target);
  const std::string& wamrcVersion();

  std::mutex m_mutex;
  std::string m_wamrcPath;
  std::string m_cacheDir;
  std::string m_wamrcVersion;
  std::string m_disabledReason;
  bool m_versionProbed = false;
  bool m_compileOnBoot = false;
  // Stale key dirs were swept since the last configure.
  bool m_pruned = false;
  // The wamrc accepts our --rive-interrupt flag; true on platforms that
  // cannot probe.
  bool m_wamrcUsable = true;
  // The wamrc takes --tune-cpu; older builds compile untuned.
  bool m_wamrcTunes = false;
  // compileSync jobs with a cancel flag, which cancelCompiles kills.
  std::vector<Job*> m_cancelableJobs;
  // Host artifacts compiled since configure, whose rejection is final.
  std::set<std::pair<uint64_t, TierSpecies>> m_compiled;
  // Why each failed (module, species, target) failed.
  std::map<std::tuple<uint64_t, TierSpecies, std::string>, std::string>
      m_failures;
};

#else // RIVE_WASM_TIER_LADDER

// Same surface, all inline: every compile reports the ladder disabled.
class ModuleTierLadder
{
public:
  static ModuleTierLadder& instance()
  {
    static ModuleTierLadder ladder;
    return ladder;
  }

  void configure(const std::string&, const std::string&, bool = false) {}
  void unconfigure() {}
  bool enabled() { return false; }
  std::string disabledReason() { return "no aot compiler in this build"; }
  std::string untrustedCacheDir() { return std::string(); }
  std::string compilerPath() { return std::string(); }

  std::string artifactPath(
      uint64_t,
      TierSpecies,
      const std::string& = std::string())
  {
    return std::string();
  }
  TierCompile compileSync(
      uint64_t,
      Span<const uint8_t>,
      TierSpecies,
      const std::string& = std::string(),
      const std::atomic<bool>* = nullptr)
  {
    TierCompile result;
    result.reason = disabledReason();
    return result;
  }
  std::vector<TierCompile> compileAll(
      const std::vector<Span<const uint8_t>>& modules,
      TierSpecies species,
      const std::atomic<bool>* = nullptr)
  {
    return std::vector<TierCompile>(
        modules.size(),
        compileSync(0, Span<const uint8_t>(), species));
  }
  std::vector<TierCompile> compileWithLanes(
      Span<const uint8_t>,
      TierSpecies species,
      const std::atomic<bool>* = nullptr)
  {
    return {compileSync(0, Span<const uint8_t>(), species)};
  }
  TierCompile bootCompile(
      uint64_t key,
      Span<const uint8_t> bytes,
      TierSpecies species)
  {
    return compileSync(key, bytes, species);
  }
  void cancelCompiles() {}
  bool failed(uint64_t, TierSpecies, const std::string& = std::string())
  {
    return false;
  }
  void rejectArtifact(uint64_t, TierSpecies, const std::string&) {}

private:
  // Singleton in both configurations, so code cannot compile against one
  // and break against the other.
  ModuleTierLadder() = default;
  ~ModuleTierLadder() = default;
  ModuleTierLadder(const ModuleTierLadder&) = delete;
  ModuleTierLadder& operator=(const ModuleTierLadder&) = delete;
};

#endif // RIVE_WASM_TIER_LADDER

} // namespace rive

#endif // WITH_RIVE_SCRIPTING_WASM
#endif
