#ifdef WITH_RIVE_SCRIPTING_WASM

#include "rive/wasm/module_tier_ladder.hpp"

#ifndef _WIN32

#include <algorithm>
#include <cerrno>
#include <cstdio>
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
    std::string dir = m_cacheDir + "/" + wamrcVersion();
    mkdir(m_cacheDir.c_str(), 0755);
    mkdir(dir.c_str(), 0755);
    return dir;
}

std::string ModuleTierLadder::artifactName(uint64_t moduleKey,
                                           TierSpecies species)
{
    char name[64];
    snprintf(name,
             sizeof(name),
             species == TierSpecies::o0 ? "%016llx.o0.aot" : "%016llx.aot",
             (unsigned long long)moduleKey);
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

    std::vector<TierSpecies> wanted;
    if (moduleBytes.size() <= kStraightToO3Bytes)
    {
        wanted = {TierSpecies::o3};
    }
    else
    {
        wanted = {TierSpecies::o0, TierSpecies::o3};
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
            FILE* f = fopen(wasmPath.c_str(), "wb");
            if (f == nullptr)
            {
                return;
            }
            fwrite(moduleBytes.data(), 1, moduleBytes.size(), f);
            fclose(f);
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
    if (job.species == TierSpecies::o0)
    {
        args.push_back("--opt-level=0");
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
}

void ModuleTierLadder::drain()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_idle.wait(lock, [this] { return m_queue.empty() && m_running.empty(); });
}

} // namespace rive

#else // _WIN32

namespace rive
{
// Windows editor execution is a deferred decision; the ladder reports
// disabled and every module stays on interp.
ModuleTierLadder& ModuleTierLadder::instance()
{
    static ModuleTierLadder* ladder = new ModuleTierLadder();
    return *ladder;
}
ModuleTierLadder::~ModuleTierLadder() {}
void ModuleTierLadder::configure(const std::string&, const std::string&) {}
bool ModuleTierLadder::enabled() { return false; }
void ModuleTierLadder::onArrival(ArrivalCallback) {}
void ModuleTierLadder::schedule(const std::string&,
                                uint64_t,
                                Span<const uint8_t>)
{}
std::string ModuleTierLadder::artifactPath(uint64_t, TierSpecies)
{
    return std::string();
}
void ModuleTierLadder::drain() {}
} // namespace rive

#endif // _WIN32
#endif // WITH_RIVE_SCRIPTING_WASM
