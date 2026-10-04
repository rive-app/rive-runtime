#ifdef WITH_RIVE_SCRIPTING_WASM

#include "rive/wasm/module_tier_ladder.hpp"
#include "rive/wasm/aot_artifact.hpp"

#ifdef RIVE_WASM_TIER_LADDER

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <fcntl.h>
#include <cstdlib>
#include <cstring>
#include <signal.h>
#include <spawn.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;

namespace rive
{

ModuleTierLadder& ModuleTierLadder::instance()
{
    static ModuleTierLadder* ladder = new ModuleTierLadder();
    return *ladder;
}

ModuleTierLadder::~ModuleTierLadder()
{
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_terminating = true;
        for (Job* job : m_running)
        {
            job->cancelled = true;
#ifndef RIVE_NX
            if (job->pid > 0)
            {
                kill(job->pid, SIGKILL);
            }
#endif
        }
    }
    m_workAvailable.notify_all();
    for (auto& worker : m_workers)
    {
        worker.join();
    }
}

void ModuleTierLadder::configure(const std::string& wamrcPath,
                                 const std::string& cacheDir)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_wamrcPath = wamrcPath;
    m_versionProbed = false;
    if (m_wamrcPath.empty())
    {
        if (const char* env = getenv("RIVE_WAMRC"))
        {
            m_wamrcPath = env;
        }
    }
    m_cacheDir = cacheDir;
}

bool ModuleTierLadder::enabled()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_wamrcPath.empty() && getenv("RIVE_WAMRC") != nullptr)
    {
        m_wamrcPath = getenv("RIVE_WAMRC");
    }
    // Self-configure from env so the first module load, which happens
    // before any host configure call, can stage its pristine bytes.
    if (m_cacheDir.empty() && !m_wamrcPath.empty())
    {
        const char* dirEnv = getenv("RIVE_AOT_CACHE_DIR");
#ifdef RIVE_NX
        // No temp_directory_path in this libc++, and the platform cannot map
        // executable pages anyway, so the ladder only runs when pointed at a
        // directory explicitly.
        m_cacheDir = dirEnv != nullptr ? dirEnv : "";
#else
        m_cacheDir =
            dirEnv != nullptr
                ? dirEnv
                : (std::filesystem::temp_directory_path() / "rive_aot_cache")
                      .string();
#endif
    }
    if (m_wamrcPath.empty() || m_cacheDir.empty())
    {
        return false;
    }
    wamrcVersion();
    return m_wamrcUsable;
}

std::string ModuleTierLadder::compilerPath()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    return m_wamrcPath;
}

void ModuleTierLadder::onArrival(ArrivalCallback callback)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_arrival = std::move(callback);
}

const std::string& ModuleTierLadder::wamrcVersion()
{
    // Callers hold m_mutex. The version keys the cache so a compiler swap
    // reads as misses, never as stale native code.
    if (!m_versionProbed)
    {
        m_versionProbed = true;
        m_wamrcVersion = "unknown";
#ifndef RIVE_NX
        std::string cmd = m_wamrcPath + " --version 2>/dev/null";
        if (FILE* pipe = popen(cmd.c_str(), "r"))
        {
            char line[128] = {0};
            if (fgets(line, sizeof(line), pipe) != nullptr)
            {
                std::string v(line);
                while (!v.empty() && (v.back() == '\n' || v.back() == ' '))
                {
                    v.pop_back();
                }
                for (char& c : v)
                {
                    if (c == ' ' || c == '/')
                    {
                        c = '-';
                    }
                }
                if (!v.empty())
                {
                    m_wamrcVersion = v;
                }
            }
            pclose(pipe);
        }
        auto accepts = [&](const char* flags) {
            std::string probe =
                m_wamrcPath + " " + flags + " --version >/dev/null 2>&1";
            FILE* probePipe = popen(probe.c_str(), "r");
            return probePipe != nullptr && pclose(probePipe) == 0;
        };
        // Every compile passes --rive-interrupt, which an unpatched wamrc
        // rejects; say so once rather than fail each compile quietly.
        m_wamrcUsable = accepts("--rive-interrupt");
        m_wamrcTunes = accepts("--tune-cpu=generic");
        if (!m_wamrcUsable)
        {
            fprintf(stderr,
                    "wasm aot: %s lacks --rive-interrupt; build wamrc from "
                    "our patched WAMR (runtime/scripting/wamr_patches). AOT "
                    "is off.\n",
                    m_wamrcPath.c_str());
        }
#endif
    }
    return m_wamrcVersion;
}

// wamrc flags naming a device target, null for one we cannot compile for.
static const char* const* crossTargetArgs(const std::string& target)
{
    // Android reserves x18 for its shadow call stack.
    static const char* const androidAarch64[] = {"--target=aarch64v8",
                                                 "--target-abi=gnu",
                                                 "--cpu=generic",
                                                 "--cpu-features=+reserve-x18",
                                                 nullptr};
    static const char* const androidX64[] = {"--target=x86_64",
                                             "--target-abi=gnu",
                                             nullptr};
    if (target == kAotTargetAndroidAarch64)
    {
        return androidAarch64;
    }
    if (target == kAotTargetAndroidX64)
    {
        return androidX64;
    }
    return nullptr;
}

// The module exports __riveHostBudget. A byte search suffices: a false
// hit only adds interrupt checks.
static bool asksHostBudget(const std::string& wasmPath)
{
    std::ifstream in(wasmPath, std::ios::binary);
    std::string bytes((std::istreambuf_iterator<char>(in)),
                      std::istreambuf_iterator<char>());
    return bytes.find("__riveHostBudget") != std::string::npos;
}

std::string ModuleTierLadder::keyedCacheDir()
{
    // The revision folds our wamrc flag choices into the cache key; bump
    // it whenever species flags change or stale artifacts get served on a
    // cache hit.
    const std::string& version = wamrcVersion();
    std::string dir =
        m_cacheDir + "/" + version + "-r9" + (m_wamrcTunes ? "t" : "");
    mkdir(m_cacheDir.c_str(), 0755);
    mkdir(dir.c_str(), 0755);
    return dir;
}

// Device artifacts sit apart from this machine's, which share their names.
std::string ModuleTierLadder::artifactDir(const std::string& target)
{
    std::string dir = keyedCacheDir();
    if (target.empty())
    {
        return dir;
    }
    dir += "/" + target;
    mkdir(dir.c_str(), 0755);
    return dir;
}

std::string ModuleTierLadder::artifactName(uint64_t moduleKey,
                                           TierSpecies species)
{
    if (species == TierSpecies::o0)
    {
        return aotArtifactName(moduleKey, ".o0");
    }
    if (species == TierSpecies::hw)
    {
        return aotArtifactName(moduleKey, ".hw");
    }
    return aotArtifactName(moduleKey);
}

std::string ModuleTierLadder::artifactPath(uint64_t moduleKey,
                                           TierSpecies species,
                                           const std::string& target)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    // Targets come from link peers, so only known ones reach a path.
    if (m_wamrcPath.empty() || m_cacheDir.empty() ||
        (!target.empty() && crossTargetArgs(target) == nullptr))
    {
        return std::string();
    }
    std::string path =
        artifactDir(target) + "/" + artifactName(moduleKey, species);
    struct stat st;
    if (stat(path.c_str(), &st) == 0)
    {
        return path;
    }
    return std::string();
}

// The cache is shared across processes and a name must never be visible
// half-written: write a process-unique temp, then atomically rename.
static bool writeFileAtomic(const std::string& path, Span<const uint8_t> bytes)
{
    std::string tmpPath = path + "." + std::to_string(getpid()) + ".tmp";
    FILE* f = fopen(tmpPath.c_str(), "wb");
    if (f == nullptr)
    {
        return false;
    }
    size_t written = fwrite(bytes.data(), 1, bytes.size(), f);
    if (fclose(f) != 0 || written != bytes.size())
    {
        unlink(tmpPath.c_str());
        return false;
    }
    if (rename(tmpPath.c_str(), path.c_str()) != 0)
    {
        unlink(tmpPath.c_str());
        return false;
    }
    return true;
}

void ModuleTierLadder::stagePristine(uint64_t moduleKey,
                                     Span<const uint8_t> moduleBytes)
{
    if (!enabled())
    {
        return;
    }
    std::unique_lock<std::mutex> lock(m_mutex);
    std::string wasmPath = keyedCacheDir() + "/" + wasmModuleName(moduleKey);
    struct stat st;
    if (stat(wasmPath.c_str(), &st) == 0)
    {
        return;
    }
    writeFileAtomic(wasmPath, moduleBytes);
}

std::string ModuleTierLadder::compileSync(uint64_t moduleKey,
                                          Span<const uint8_t> moduleBytes,
                                          TierSpecies species,
                                          const std::string& target,
                                          const std::atomic<bool>* cancel)
{
    if (!enabled() || (!target.empty() && crossTargetArgs(target) == nullptr))
    {
        return std::string();
    }
    std::string existing = artifactPath(moduleKey, species, target);
    if (!existing.empty())
    {
        return existing;
    }
    stagePristine(moduleKey, moduleBytes);
    std::string wasmPath;
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        wasmPath = keyedCacheDir() + "/" + wasmModuleName(moduleKey);
    }
    // Runs on the caller's thread and never joins m_running, so lane
    // supersession cannot kill it out from under the boot.
    Job job{std::string(), moduleKey, species, wasmPath, 0};
    job.target = target;
    job.cancel = cancel;
    if (!target.empty())
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_crossJobs.push_back(&job);
    }
    bool ok = runWamrc(job);
    if (!target.empty())
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_crossJobs.erase(
            std::find(m_crossJobs.begin(), m_crossJobs.end(), &job));
    }
    return ok ? artifactPath(moduleKey, species, target) : std::string();
}

void ModuleTierLadder::cancelCrossCompiles()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    for (Job* job : m_crossJobs)
    {
        if (job->cancel == nullptr || !job->cancel->load())
        {
            continue;
        }
        job->cancelled = true;
        if (job->pid > 0)
        {
            kill(job->pid, SIGKILL);
        }
    }
}

void ModuleTierLadder::schedule(const std::string& laneId,
                                uint64_t moduleKey,
                                Span<const uint8_t> moduleBytes)
{
    if (!enabled())
    {
        return;
    }
    std::unique_lock<std::mutex> lock(m_mutex);
    uint64_t generation = m_nextGeneration++;
    m_laneGeneration[laneId] = generation;
    // Supersede the lane: queued stale jobs get skipped by the generation
    // check; running ones die now so the pool frees up for the new hash.
    // Same-hash jobs are not stale — a rebuild of identical bytes must not
    // kill its own compile — so they ride forward on the new generation
    // and their species skip re-queueing below.
    std::vector<TierSpecies> inFlight;
    for (Job* job : m_running)
    {
        if (job->laneId != laneId || job->generation >= generation)
        {
            continue;
        }
        // A job already cancelled by an earlier supersede will not produce
        // an artifact, so it cannot stand in for this request even when the
        // bytes match: let its species re-queue below.
        if (job->moduleKey == moduleKey && !job->cancelled)
        {
            job->generation = generation;
            inFlight.push_back(job->species);
        }
        else if (job->moduleKey != moduleKey && job->pid > 0 && !job->cancelled)
        {
            job->cancelled = true;
#ifndef RIVE_NX
            kill(job->pid, SIGKILL);
#endif
        }
    }
    for (Job& job : m_queue)
    {
        if (job.laneId == laneId && job.moduleKey == moduleKey &&
            !job.cancelled)
        {
            job.generation = generation;
            inFlight.push_back(job.species);
        }
    }

    std::string dir = keyedCacheDir();
    std::string wasmPath = dir + "/" + wasmModuleName(moduleKey);

    // On guard-page builds the top rung is the hw species: no inline
    // bounds checks and growth never moves the memory base.
#ifdef RIVE_WASM_HW_BOUNDS
    constexpr TierSpecies kTopSpecies = TierSpecies::hw;
#else
    constexpr TierSpecies kTopSpecies = TierSpecies::o3;
#endif
    std::vector<TierSpecies> wanted;
    if (moduleBytes.size() <= kStraightToO3Bytes)
    {
        wanted = {kTopSpecies};
    }
    else
    {
        wanted = {TierSpecies::o0, kTopSpecies};
    }

    bool queued = false;
    for (TierSpecies species : wanted)
    {
        if (std::find(inFlight.begin(), inFlight.end(), species) !=
            inFlight.end())
        {
            continue;
        }
        std::string artifact = dir + "/" + artifactName(moduleKey, species);
        struct stat st;
        if (stat(artifact.c_str(), &st) == 0)
        {
            if (m_arrival)
            {
                Artifact ready{moduleKey, species, artifact};
                lock.unlock();
                m_arrival(ready);
                lock.lock();
            }
            continue;
        }
        if (!queued)
        {
            // A pristine stage from before the in-place load wins; the bytes
            // passed here may already be loader-rewritten.
            struct stat wasmStat;
            if (stat(wasmPath.c_str(), &wasmStat) != 0 &&
                !writeFileAtomic(wasmPath, moduleBytes))
            {
                return;
            }
            queued = true;
        }
        m_queue.push_back(
            Job{laneId, moduleKey, species, wasmPath, generation});
    }
    if (queued)
    {
        ensureWorkers();
        m_workAvailable.notify_all();
    }
}

void ModuleTierLadder::ensureWorkers()
{
    if (!m_workers.empty())
    {
        return;
    }
    for (unsigned i = 0; i < kWorkerCount; i++)
    {
        m_workers.emplace_back([this] { workerLoop(); });
    }
}

void ModuleTierLadder::workerLoop()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    for (;;)
    {
        m_workAvailable.wait(lock, [this] {
            return m_terminating || !m_queue.empty();
        });
        if (m_terminating)
        {
            return;
        }
        Job job = m_queue.front();
        m_queue.pop_front();
        auto lane = m_laneGeneration.find(job.laneId);
        if (lane != m_laneGeneration.end() && job.generation < lane->second)
        {
            continue;
        }
        m_running.push_back(&job);
        lock.unlock();
        bool ok = runWamrc(job);
        lock.lock();
        m_running.erase(std::find(m_running.begin(), m_running.end(), &job));
        if (ok && !job.cancelled && m_arrival)
        {
            Artifact ready{job.moduleKey,
                           job.species,
                           keyedCacheDir() + "/" +
                               artifactName(job.moduleKey, job.species)};
            lock.unlock();
            m_arrival(ready);
            lock.lock();
        }
        if (m_queue.empty() && m_running.empty())
        {
            m_idle.notify_all();
        }
    }
}

bool ModuleTierLadder::runWamrc(Job& job)
{
    // Callers dropped m_mutex; only job fields and immutable config are
    // touched off-lock, except pid which the scheduler reads under lock to
    // kill superseded compiles.
    std::string finalPath;
    std::string wamrc;
    bool tunes = false;
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        finalPath = artifactDir(job.target) + "/" +
                    artifactName(job.moduleKey, job.species);
        wamrc = m_wamrcPath;
        tunes = m_wamrcTunes;
    }
    // Unique temp per compile: the same artifact can be raced by a worker
    // and a sync boot, or by two processes sharing the cache.
    static std::atomic<uint64_t> tmpSerial{0};
    std::string tmpPath = finalPath + "." + std::to_string(getpid()) + "-" +
                          std::to_string(++tmpSerial) + ".tmp";

    std::vector<std::string> args = {wamrc};
    if (job.species == TierSpecies::hw)
    {
        // Guard-page bounds; native stack checks stay sw (the runtime
        // builds with WASM_DISABLE_STACK_HW_BOUND_CHECK).
        args.push_back("--bounds-checks=0");
        args.push_back("--stack-bounds-checks=1");
    }
    else
    {
        // sw bounds run on every build; wamrc's default hw-bounds output
        // segfaults on runtimes without the guard-page trap handler.
        args.push_back("--bounds-checks=1");
    }
    if (job.species == TierSpecies::o0)
    {
        args.push_back("--opt-level=0");
    }
    else
    {
        args.push_back("--opt-level=3");
    }
    if (!job.target.empty())
    {
        const char* const* targetArgs = crossTargetArgs(job.target);
        if (targetArgs == nullptr)
        {
            return false;
        }
        for (; *targetArgs != nullptr; targetArgs++)
        {
            args.push_back(*targetArgs);
        }
    }
#if (defined(__APPLE__) || defined(_WIN32)) &&                                 \
    (defined(__aarch64__) || defined(_M_ARM64))
    else
    {
        // The platform clobbers x18; reserve it here so safety does not
        // depend on the wamrc we were handed. Features need an explicit cpu.
        args.push_back("--cpu=generic");
        args.push_back("--cpu-features=+reserve-x18");
#if defined(__APPLE__)
        // Generic scheduling turns float compare chains into mispredicted
        // branches (up to 1.5x slower); tuning leaves the ISA baseline alone.
        if (tunes)
        {
            args.push_back("--tune-cpu=apple-m1");
        }
#endif
    }
#endif
    // Loop back-edges and calls check the watchdog only in modules that ask
    // for it; the module bytes, and so its cache key, decide.
    if (asksHostBudget(job.wasmPath))
    {
        args.push_back("--rive-interrupt");
    }
    args.push_back("-o");
    args.push_back(tmpPath);
    args.push_back(job.wasmPath);
    std::vector<char*> argv;
    for (auto& a : args)
    {
        argv.push_back(const_cast<char*>(a.c_str()));
    }
    argv.push_back(nullptr);

#if defined(RIVE_ANDROID) || defined(RIVE_NX)
    // No wamrc on device, and neither platform has posix_spawn; the ladder
    // never schedules compiles here.
    return false;
#else
    pid_t pid = -1;
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    // A compiler's stdout is noise at editor runtime; stderr stays for
    // diagnosing failed compiles.
    posix_spawn_file_actions_addopen(&actions,
                                     STDOUT_FILENO,
                                     "/dev/null",
                                     O_WRONLY,
                                     0);
    int rc = posix_spawn(&pid,
                         wamrc.c_str(),
                         &actions,
                         nullptr,
                         argv.data(),
                         environ);
    posix_spawn_file_actions_destroy(&actions);
    if (rc != 0)
    {
        return false;
    }
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        job.pid = pid;
        // A cancel that landed before the spawn had no pid to kill.
        if (job.cancel != nullptr && job.cancel->load())
        {
            job.cancelled = true;
        }
        if (job.cancelled)
        {
            kill(pid, SIGKILL);
        }
    }
    // The whole child runs below interactive priority; the editor's frame
    // loop must never contend with LLVM.
    setpriority(PRIO_PROCESS, pid, 10);

    int status = 0;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
    {
    }
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        job.pid = -1;
    }
    if (job.cancelled || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
    {
        if (!job.cancelled)
        {
            // wamrc prints usage errors to stdout, which is discarded; an
            // unlogged exit here is otherwise a silently dead lane.
            fprintf(stderr,
                    "[wasm] wamrc failed (exit %d) on %s\n",
                    WIFEXITED(status) ? WEXITSTATUS(status) : -1,
                    job.wasmPath.c_str());
        }
        unlink(tmpPath.c_str());
        return false;
    }
    // Atomic arrival: a partial artifact can never carry the final name.
    return rename(tmpPath.c_str(), finalPath.c_str()) == 0;
#endif
}

void ModuleTierLadder::drain()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_idle.wait(lock, [this] { return m_queue.empty() && m_running.empty(); });
}

} // namespace rive

#endif // RIVE_WASM_TIER_LADDER
#endif // WITH_RIVE_SCRIPTING_WASM
