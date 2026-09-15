#ifndef _RIVE_MODULE_TIER_LADDER_HPP_
#define _RIVE_MODULE_TIER_LADDER_HPP_

#ifdef WITH_RIVE_SCRIPTING_WASM

// An editor/tools feature: the ladder spawns wamrc subprocesses. Runtime
// builds stub it out and stay on interp (prelinked artifacts load by
// registry instead); windows tools builds stay stubbed pending a
// CreateProcess port.
#if defined(WITH_RIVE_TOOLS) && !defined(_WIN32)
#define RIVE_WASM_TIER_LADDER 1
#endif

#include "rive/span.hpp"
#include <cstdint>
#include <functional>
#include <string>

#ifdef RIVE_WASM_TIER_LADDER
#include <condition_variable>
#include <deque>
#include <mutex>
#include <sys/types.h>
#include <thread>
#include <unordered_map>
#include <vector>
#endif

namespace rive
{

// Compiled-artifact species, in ascending speed. Interp is the implicit
// tier 0 every module already runs on; the ladder only produces artifacts.
enum class TierSpecies : uint8_t
{
    o0 = 0, // wamrc -O0: ~2x interp, sub-second for typical modules
    o3 = 1, // wamrc top opt level with sw bounds, runs on every build
    // No inline bounds checks; needs the guard-page trap handler, so only
    // RIVE_WASM_HW_BOUNDS builds produce or load these.
    hw = 2,
};

#ifdef RIVE_WASM_TIER_LADDER

// Drives wamrc subprocesses that turn wasm modules into AOT artifacts and
// reports arrivals for frame-boundary hot swap. File-in/file-out, no IPC:
// compile crashes and LLVM's RSS stay in the child. Process-wide like the
// shared module cache; safe to call from any thread.
class ModuleTierLadder
{
public:
    static ModuleTierLadder& instance();

    // wamrc discovery: explicit path wins, RIVE_WAMRC env second. The
    // ladder is inert (schedule() == no-op) until both a compiler and a
    // cache directory are configured.
    void configure(const std::string& wamrcPath, const std::string& cacheDir);
    bool enabled();

    struct Artifact
    {
        uint64_t moduleKey = 0;
        TierSpecies species = TierSpecies::o0;
        std::string path;
    };
    using ArrivalCallback = std::function<void(const Artifact&)>;
    // One process-wide sink; the editor fans out. Called from ladder worker
    // threads — receivers defer the swap to a frame boundary.
    void onArrival(ArrivalCallback callback);

    // Schedule compiles for a module. laneId groups requests for the same
    // logical script: a newer schedule on the lane kills superseded
    // in-flight compiles (newest-hash-wins). Small modules go straight to
    // -O3; larger ones run -O0 and -O3 in parallel. Cache hits report
    // arrival immediately without spawning.
    void schedule(const std::string& laneId,
                  uint64_t moduleKey,
                  Span<const uint8_t> moduleBytes);

    // Write the module's wamrc input before wasm_runtime_load touches the
    // buffer: the fast-interp loader rewrites it in place, so bytes staged
    // at schedule time are no longer valid wasm.
    void stagePristine(uint64_t moduleKey, Span<const uint8_t> moduleBytes);

    // Ready artifact path for a module at the given species, empty if none.
    std::string artifactPath(uint64_t moduleKey, TierSpecies species);

    // Compile one species on the calling thread, bypassing the worker pool
    // and lane supersession, and return the artifact path (or an existing
    // artifact's path immediately). Empty on failure. This is the sync-boot
    // path: a host that would rather block for seconds than run a module on
    // the interpreter at all.
    std::string compileSync(uint64_t moduleKey,
                            Span<const uint8_t> moduleBytes,
                            TierSpecies species);

    // Test hooks: block until the lane has no queued or running compiles.
    void drain();

    // Straight -O3 cutoff; modules at or under skip the -O0 rung entirely.
    static constexpr size_t kStraightToO3Bytes = 50 * 1024;

private:
    ModuleTierLadder() = default;
    ~ModuleTierLadder();

    struct Job
    {
        std::string laneId;
        uint64_t moduleKey = 0;
        TierSpecies species = TierSpecies::o0;
        std::string wasmPath;
        uint64_t generation = 0;
        pid_t pid = -1;
        bool cancelled = false;
    };

    void ensureWorkers();
    void workerLoop();
    bool runWamrc(Job& job);
    std::string artifactName(uint64_t moduleKey, TierSpecies species);
    std::string keyedCacheDir();
    const std::string& wamrcVersion();

    std::mutex m_mutex;
    std::condition_variable m_workAvailable;
    std::condition_variable m_idle;
    std::string m_wamrcPath;
    std::string m_cacheDir;
    std::string m_wamrcVersion;
    bool m_versionProbed = false;
    ArrivalCallback m_arrival;
    std::deque<Job> m_queue;
    std::vector<Job*> m_running;
    // Latest schedule generation per lane; older jobs are stale and are
    // killed (running) or skipped (queued).
    std::unordered_map<std::string, uint64_t> m_laneGeneration;
    uint64_t m_nextGeneration = 1;
    std::vector<std::thread> m_workers;
    bool m_terminating = false;

    // Two concurrent compiles saturate the edit loop without starving the
    // machine; the -O3 tail rides behind the -O0 rung it bridges.
    static constexpr unsigned kWorkerCount = 2;
};

#else // RIVE_WASM_TIER_LADDER

// Same surface, all inline: the ladder reports disabled, so the tier
// paths behind enabled() checks compile away.
class ModuleTierLadder
{
public:
    static ModuleTierLadder& instance()
    {
        static ModuleTierLadder ladder;
        return ladder;
    }

    void configure(const std::string&, const std::string&) {}
    bool enabled() { return false; }

    struct Artifact
    {
        uint64_t moduleKey = 0;
        TierSpecies species = TierSpecies::o0;
        std::string path;
    };
    using ArrivalCallback = std::function<void(const Artifact&)>;
    void onArrival(ArrivalCallback) {}

    void schedule(const std::string&, uint64_t, Span<const uint8_t>) {}
    void stagePristine(uint64_t, Span<const uint8_t>) {}
    std::string artifactPath(uint64_t, TierSpecies) { return std::string(); }
    std::string compileSync(uint64_t, Span<const uint8_t>, TierSpecies)
    {
        return std::string();
    }
    void drain() {}

    static constexpr size_t kStraightToO3Bytes = 50 * 1024;

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
