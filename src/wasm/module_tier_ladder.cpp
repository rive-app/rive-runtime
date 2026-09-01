#ifdef WITH_RIVE_SCRIPTING_WASM

#include "rive/wasm/module_tier_ladder.hpp"

#ifdef RIVE_WASM_TIER_LADDER

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <filesystem>
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
            if (job->pid > 0)
            {
                kill(job->pid, SIGKILL);
            }
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
        m_cacheDir =
            dirEnv != nullptr
                ? dirEnv
                : (std::filesystem::temp_directory_path() / "rive_aot_cache")
                      .string();
    }
    return !m_wamrcPath.empty() && !m_cacheDir.empty();
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
    }
    return m_wamrcVersion;
}

std::string ModuleTierLadder::keyedCacheDir()
{
    // The revision folds our wamrc flag choices into the cache key; bump
    // it whenever species flags change or stale artifacts (like the
    // pre-codegen-cap -O1 ones) get served on a cache hit.
    std::string dir = m_cacheDir + "/" + wamrcVersion() + "-r3";
    mkdir(m_cacheDir.c_str(), 0755);
    mkdir(dir.c_str(), 0755);
    return dir;
}

std::string ModuleTierLadder::artifactName(uint64_t moduleKey,
                                           TierSpecies species)
{
    const char* pattern = "%016llx.aot";
    if (species == TierSpecies::o0)
    {
        pattern = "%016llx.o0.aot";
    }
    else if (species == TierSpecies::hw)
    {
        pattern = "%016llx.hw.aot";
    }
    char name[64];
    snprintf(name, sizeof(name), pattern, (unsigned long long)moduleKey);
    return name;
}

std::string ModuleTierLadder::artifactPath(uint64_t moduleKey,
                                           TierSpecies species)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_wamrcPath.empty() || m_cacheDir.empty())
    {
        return std::string();
    }
    std::string path = keyedCacheDir() + "/" + artifactName(moduleKey, species);
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
    char wasmName[64];
    snprintf(wasmName,
             sizeof(wasmName),
             "%016llx.wasm",
             (unsigned long long)moduleKey);
    std::string wasmPath = keyedCacheDir() + "/" + wasmName;
    struct stat st;
    if (stat(wasmPath.c_str(), &st) == 0)
    {
        return;
    }
    writeFileAtomic(wasmPath, moduleBytes);
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
    for (Job* job : m_running)
    {
        if (job->laneId == laneId && job->generation < generation &&
            job->pid > 0)
        {
            job->cancelled = true;
            kill(job->pid, SIGKILL);
        }
    }

    std::string dir = keyedCacheDir();
    char wasmName[64];
    snprintf(wasmName,
             sizeof(wasmName),
             "%016llx.wasm",
             (unsigned long long)moduleKey);
    std::string wasmPath = dir + "/" + wasmName;

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
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        finalPath =
            keyedCacheDir() + "/" + artifactName(job.moduleKey, job.species);
        wamrc = m_wamrcPath;
    }
    std::string tmpPath = finalPath + ".tmp";

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
    if (job.species == TierSpecies::o0 || job.species == TierSpecies::hw)
    {
        args.push_back("--opt-level=0");
    }
    else
    {
        // LLVM's optimizing backend miscompiles the strict (constrained)
        // FP wamrc emits, probabilistically corrupting float-heavy modules
        // (box2d was the repro; the town's draco trap was the same class).
        // Our vendored wamrc pins codegen conservative while the IR still
        // optimizes; lift only against the box2d strict-probe soak.
        args.push_back("--opt-level=1");
        args.push_back("--codegen-opt-level=0");
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

#ifdef RIVE_ANDROID
    // No wamrc on device, and posix_spawn needs API 28; the ladder never
    // schedules compiles here.
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
