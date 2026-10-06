#ifdef WITH_RIVE_SCRIPTING_WASM

#include "rive/wasm/module_tier_ladder.hpp"
#include "rive/wasm/aot_artifact.hpp"

#include <cstdlib>
#include <cstring>

namespace rive
{
TierSpecies bootTierSpecies()
{
    const char* tier = getenv("RIVE_WASM_AOT_SYNC");
    if (tier != nullptr && strcmp(tier, "o0") == 0)
    {
        return TierSpecies::o0;
    }
#ifdef RIVE_WASM_HW_BOUNDS
    return TierSpecies::hw;
#else
    return TierSpecies::o3;
#endif
}
} // namespace rive

#ifdef RIVE_WASM_TIER_LADDER

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <cstdlib>
#include <cstring>
#include <sys/stat.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;
#endif

namespace rive
{

static uint64_t processId()
{
#ifdef _WIN32
    return GetCurrentProcessId();
#else
    return (uint64_t)getpid();
#endif
}

static void makeDir(const std::string& path)
{
#ifdef RIVE_NX
    // Our NX libc++ has no std::filesystem directory ops.
    mkdir(path.c_str(), 0755);
#else
    std::error_code ec;
    std::filesystem::create_directory(path, ec);
#endif
}

static bool fileExists(const std::string& path)
{
    struct stat st;
    return stat(path.c_str(), &st) == 0;
}

// Windows refuses to rename over an existing file, and another compile of
// the same artifact landing first is still a success.
static bool publishFile(const std::string& tmpPath, const std::string& path)
{
    if (rename(tmpPath.c_str(), path.c_str()) == 0)
    {
        return true;
    }
    std::remove(tmpPath.c_str());
    return fileExists(path);
}

#ifdef _WIN32
// CreateProcess takes one command line; quote each argument so the child's
// CRT splits it back into the same argv.
static std::string quoteArg(const std::string& arg)
{
    if (!arg.empty() && arg.find_first_of(" \t\"") == std::string::npos)
    {
        return arg;
    }
    std::string out = "\"";
    size_t slashes = 0;
    for (char c : arg)
    {
        if (c == '\\')
        {
            slashes++;
            continue;
        }
        out.append(c == '"' ? slashes * 2 + 1 : slashes, '\\');
        slashes = 0;
        out += c;
    }
    out.append(slashes * 2, '\\');
    return out + "\"";
}

static HANDLE inheritable(HANDLE handle)
{
    HANDLE copy = nullptr;
    if (handle == nullptr || handle == INVALID_HANDLE_VALUE ||
        !DuplicateHandle(GetCurrentProcess(),
                         handle,
                         GetCurrentProcess(),
                         &copy,
                         0,
                         TRUE,
                         DUPLICATE_SAME_ACCESS))
    {
        return nullptr;
    }
    return copy;
}

// Starts args below interactive priority with no console window. stdout goes
// to out, or nowhere; stderr is the host's unless quiet. Only these handles
// are inherited, so a concurrent child never holds another's pipe open.
static intptr_t startChild(const std::vector<std::string>& args,
                           std::string* failure,
                           HANDLE out = nullptr,
                           bool quiet = false)
{
    std::string line;
    for (const std::string& arg : args)
    {
        line += (line.empty() ? "" : " ") + quoteArg(arg);
    }
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE nul = CreateFileA("NUL",
                             GENERIC_READ | GENERIC_WRITE,
                             FILE_SHARE_READ | FILE_SHARE_WRITE,
                             &sa,
                             OPEN_EXISTING,
                             0,
                             nullptr);
    HANDLE err = quiet ? nullptr : inheritable(GetStdHandle(STD_ERROR_HANDLE));
    STARTUPINFOEXA si{};
    si.StartupInfo.cb = sizeof(si);
    si.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
    si.StartupInfo.hStdInput = nul;
    si.StartupInfo.hStdOutput = out != nullptr ? out : nul;
    si.StartupInfo.hStdError = err != nullptr ? err : nul;
    HANDLE handles[3] = {nul,
                         si.StartupInfo.hStdOutput,
                         si.StartupInfo.hStdError};
    std::sort(std::begin(handles), std::end(handles));
    DWORD handleCount =
        (DWORD)(std::unique(std::begin(handles), std::end(handles)) - handles);
    SIZE_T attrSize = 0;
    InitializeProcThreadAttributeList(nullptr, 1, 0, &attrSize);
    std::vector<uint8_t> attrs(attrSize);
    si.lpAttributeList = (LPPROC_THREAD_ATTRIBUTE_LIST)attrs.data();
    PROCESS_INFORMATION pi{};
    bool listReady =
        InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &attrSize);
    bool ok = listReady &&
              UpdateProcThreadAttribute(si.lpAttributeList,
                                        0,
                                        PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                        handles,
                                        handleCount * sizeof(HANDLE),
                                        nullptr,
                                        nullptr) &&
              CreateProcessA(nullptr,
                             line.data(),
                             nullptr,
                             nullptr,
                             TRUE,
                             CREATE_NO_WINDOW | BELOW_NORMAL_PRIORITY_CLASS |
                                 EXTENDED_STARTUPINFO_PRESENT,
                             nullptr,
                             nullptr,
                             &si.StartupInfo,
                             &pi);
    DWORD error = ok ? 0 : GetLastError();
    if (listReady)
    {
        DeleteProcThreadAttributeList(si.lpAttributeList);
    }
    CloseHandle(nul);
    if (err != nullptr)
    {
        CloseHandle(err);
    }
    if (!ok)
    {
        *failure = "could not run " + args[0] + ", windows error " +
                   std::to_string(error);
        return -1;
    }
    CloseHandle(pi.hThread);
    return (intptr_t)pi.hProcess;
}

static void killChild(intptr_t child) { TerminateProcess((HANDLE)child, 1); }

// The exit code, or -1 when the child did not exit on its own.
static int waitChild(intptr_t child)
{
    DWORD code = 1;
    if (WaitForSingleObject((HANDLE)child, INFINITE) != WAIT_OBJECT_0 ||
        !GetExitCodeProcess((HANDLE)child, &code))
    {
        return -1;
    }
    return (int)code;
}

static void releaseChild(intptr_t child) { CloseHandle((HANDLE)child); }

// Runs wamrc with flag and --version: true when it exits cleanly, with the
// first line of its output when asked.
static bool probeWamrc(const std::string& wamrc,
                       const char* flag,
                       std::string* firstLine)
{
    std::vector<std::string> args = {wamrc};
    if (flag != nullptr)
    {
        args.push_back(flag);
    }
    args.push_back("--version");
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE readEnd = nullptr;
    HANDLE writeEnd = nullptr;
    if (!CreatePipe(&readEnd, &writeEnd, &sa, 0))
    {
        return false;
    }
    SetHandleInformation(readEnd, HANDLE_FLAG_INHERIT, 0);
    std::string failure;
    intptr_t child = startChild(args, &failure, writeEnd, true);
    CloseHandle(writeEnd);
    std::string text;
    char buffer[256];
    DWORD read = 0;
    while (ReadFile(readEnd, buffer, sizeof(buffer), &read, nullptr) &&
           read > 0)
    {
        text.append(buffer, read);
    }
    CloseHandle(readEnd);
    if (child == -1)
    {
        return false;
    }
    int code = waitChild(child);
    releaseChild(child);
    if (firstLine != nullptr)
    {
        *firstLine = text.substr(0, text.find('\n'));
    }
    return code == 0;
}
#elif defined(RIVE_ANDROID) || defined(RIVE_NX)
// No wamrc on device, and neither platform can spawn one.
static intptr_t startChild(const std::vector<std::string>&,
                           std::string* failure)
{
    *failure = "wamrc cannot run on this platform";
    return -1;
}

static void killChild(intptr_t) {}

static int waitChild(intptr_t) { return -1; }

static void releaseChild(intptr_t) {}
#else
// Starts args below interactive priority; a compiler's stdout is noise at
// editor runtime, stderr stays for diagnosing failed compiles.
static intptr_t startChild(const std::vector<std::string>& args,
                           std::string* failure)
{
    std::vector<char*> argv;
    for (const std::string& arg : args)
    {
        argv.push_back(const_cast<char*>(arg.c_str()));
    }
    argv.push_back(nullptr);
    pid_t pid = -1;
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_addopen(&actions,
                                     STDOUT_FILENO,
                                     "/dev/null",
                                     O_WRONLY,
                                     0);
    int rc =
        posix_spawn(&pid, argv[0], &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    if (rc != 0)
    {
        *failure = "could not run " + args[0] + ", " + strerror(rc);
        return -1;
    }
    // The editor's frame loop must never contend with LLVM.
    setpriority(PRIO_PROCESS, pid, 10);
    return pid;
}

static void killChild(intptr_t child) { kill((pid_t)child, SIGKILL); }

// The exit code, or -1 when the child did not exit on its own.
static int waitChild(intptr_t child)
{
    int status = 0;
    while (waitpid((pid_t)child, &status, 0) < 0 && errno == EINTR)
    {
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

static void releaseChild(intptr_t) {}
#endif

#if !defined(_WIN32) && !defined(RIVE_NX)
// Installed apps put wamrc under paths with spaces.
static std::string shellQuote(const std::string& text)
{
    std::string quoted = "'";
    for (char c : text)
    {
        quoted += c == '\'' ? std::string("'\\''") : std::string(1, c);
    }
    return quoted + "'";
}

// Runs wamrc with flag and --version: true when it exits cleanly, with the
// first line of its output when asked.
static bool probeWamrc(const std::string& wamrc,
                       const char* flag,
                       std::string* firstLine)
{
    std::string cmd =
        shellQuote(wamrc) + (flag != nullptr ? " " + std::string(flag) : "") +
        " --version" +
        (firstLine != nullptr ? " 2>/dev/null" : " >/dev/null 2>&1");
    FILE* pipe = popen(cmd.c_str(), "r");
    if (pipe == nullptr)
    {
        return false;
    }
    char line[128] = {0};
    if (firstLine != nullptr && fgets(line, sizeof(line), pipe) != nullptr)
    {
        *firstLine = line;
    }
    return pclose(pipe) == 0;
}
#endif

ModuleTierLadder& ModuleTierLadder::instance()
{
    static ModuleTierLadder* ladder = new ModuleTierLadder();
    return *ladder;
}

void ModuleTierLadder::configure(const std::string& wamrcPath,
                                 const std::string& cacheDir,
                                 bool compileOnBoot)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_wamrcPath = wamrcPath;
    m_compileOnBoot = compileOnBoot;
    m_versionProbed = false;
    if (m_wamrcPath.empty())
    {
        if (const char* env = getenv("RIVE_WAMRC"))
        {
            m_wamrcPath = env;
        }
    }
    m_cacheDir = cacheDir;
    m_failures.clear();
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
}

void ModuleTierLadder::unconfigure()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_wamrcPath.clear();
    m_cacheDir.clear();
    m_failures.clear();
    m_versionProbed = false;
    m_compileOnBoot = false;
}

bool ModuleTierLadder::enabled()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    return enabledLocked();
}

bool ModuleTierLadder::enabledLocked()
{
    if (m_wamrcPath.empty())
    {
        m_disabledReason = "no wamrc, pass --aot or set RIVE_WAMRC";
        return false;
    }
    if (m_cacheDir.empty())
    {
        m_disabledReason = "no aot cache, set RIVE_AOT_CACHE_DIR";
        return false;
    }
    wamrcVersion();
    m_disabledReason =
        m_wamrcUsable ? "" : m_wamrcPath + " is not our patched wamrc";
    return m_wamrcUsable;
}

std::string ModuleTierLadder::disabledReason()
{
    if (enabled())
    {
        return std::string();
    }
    std::unique_lock<std::mutex> lock(m_mutex);
    return m_disabledReason;
}

std::string ModuleTierLadder::compilerPath()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    return m_wamrcPath;
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
        std::string v;
        probeWamrc(m_wamrcPath, nullptr, &v);
        while (!v.empty() &&
               (v.back() == '\n' || v.back() == '\r' || v.back() == ' '))
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
        auto accepts = [&](const char* flag) {
            return probeWamrc(m_wamrcPath, flag, nullptr);
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
    return m_cacheDir + "/" + version + "-r9" + (m_wamrcTunes ? "t" : "");
}

// Device artifacts sit apart from this machine's, which share their names.
std::string ModuleTierLadder::artifactDir(const std::string& target)
{
    std::string dir = keyedCacheDir();
    return target.empty() ? dir : dir + "/" + target;
}

void ModuleTierLadder::createArtifactDir(const std::string& target)
{
    makeDir(m_cacheDir);
    makeDir(keyedCacheDir());
    if (!target.empty())
    {
        makeDir(artifactDir(target));
    }
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
    return fileExists(path) ? path : std::string();
}

// The cache is shared across processes and a name must never be visible
// half-written: write a process-unique temp, then atomically rename.
static bool writeFileAtomic(const std::string& path, Span<const uint8_t> bytes)
{
    std::string tmpPath = path + "." + std::to_string(processId()) + ".tmp";
    FILE* f = fopen(tmpPath.c_str(), "wb");
    if (f == nullptr)
    {
        return false;
    }
    size_t written = fwrite(bytes.data(), 1, bytes.size(), f);
    if (fclose(f) != 0 || written != bytes.size())
    {
        std::remove(tmpPath.c_str());
        return false;
    }
    return publishFile(tmpPath, path);
}

TierCompile ModuleTierLadder::compileSync(uint64_t moduleKey,
                                          Span<const uint8_t> moduleBytes,
                                          TierSpecies species,
                                          const std::string& target,
                                          const std::atomic<bool>* cancel)
{
    TierCompile result;
    Job job{moduleKey, species};
    job.target = target;
    job.cancel = cancel;
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (lookupLocked(job, result))
        {
            return result;
        }
        createArtifactDir(target);
        job.wasmPath = keyedCacheDir() + "/" + wasmModuleName(moduleKey);
        if (!fileExists(job.wasmPath) &&
            !writeFileAtomic(job.wasmPath, moduleBytes))
        {
            result.reason =
                "could not write the module into the aot cache " + m_cacheDir;
            m_failures[{moduleKey, species, target}] = result.reason;
            return result;
        }
        if (!target.empty())
        {
            m_crossJobs.push_back(&job);
        }
    }
    result.reason = runWamrc(job);
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (!target.empty())
        {
            m_crossJobs.erase(
                std::find(m_crossJobs.begin(), m_crossJobs.end(), &job));
        }
        if (!result.reason.empty() && !job.cancelled)
        {
            m_failures[{moduleKey, species, target}] = result.reason;
        }
    }
    if (result.reason.empty())
    {
        result.path = job.finalPath;
        result.compiled = true;
    }
    return result;
}

bool ModuleTierLadder::lookupLocked(Job& job, TierCompile& result)
{
    if (!enabledLocked())
    {
        result.reason = m_disabledReason;
        return true;
    }
    if (!job.target.empty() && crossTargetArgs(job.target) == nullptr)
    {
        result.reason = "wamrc cannot compile for " + job.target;
        return true;
    }
    auto failure = m_failures.find({job.moduleKey, job.species, job.target});
    if (failure != m_failures.end())
    {
        result.reason = failure->second;
        return true;
    }
    job.finalPath = artifactDir(job.target) + "/" +
                    artifactName(job.moduleKey, job.species);
    if (fileExists(job.finalPath))
    {
        result.path = job.finalPath;
        return true;
    }
    return false;
}

TierCompile ModuleTierLadder::bootCompile(uint64_t moduleKey,
                                          Span<const uint8_t> moduleBytes,
                                          TierSpecies species)
{
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (!m_compileOnBoot)
        {
            TierCompile result;
            Job job{moduleKey, species};
            if (!lookupLocked(job, result))
            {
                result.reason = "no aot artifact cached for this module yet";
            }
            return result;
        }
    }
    return compileSync(moduleKey, moduleBytes, species);
}

bool ModuleTierLadder::failed(uint64_t moduleKey,
                              TierSpecies species,
                              const std::string& target)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    return m_failures.count({moduleKey, species, target}) != 0;
}

void ModuleTierLadder::rejectArtifact(uint64_t moduleKey,
                                      TierSpecies species,
                                      const std::string& reason)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    m_failures[{moduleKey, species, std::string()}] = reason;
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
        if (job->child != -1)
        {
            killChild(job->child);
        }
    }
}

std::string ModuleTierLadder::runWamrc(Job& job)
{
    // Callers dropped m_mutex; only job fields and immutable config are
    // touched off-lock, except child which cancelCrossCompiles reads under
    // lock.
    const std::string& finalPath = job.finalPath;
    std::string wamrc;
    std::string cacheDir;
    [[maybe_unused]] bool tunes = false;
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        wamrc = m_wamrcPath;
        cacheDir = m_cacheDir;
        tunes = m_wamrcTunes;
    }
    // Unique temp per compile: the same artifact can be raced by two boots
    // on different threads, or by two processes sharing the cache.
    static std::atomic<uint64_t> tmpSerial{0};
    std::string tmpPath = finalPath + "." + std::to_string(processId()) + "-" +
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
            return "wamrc cannot compile for " + job.target;
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
    std::string failure;
    intptr_t child = startChild(args, &failure);
    if (child == -1)
    {
        fprintf(stderr, "[wasm] %s\n", failure.c_str());
        return failure;
    }
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        job.child = child;
        // A cancel that landed before the spawn had nothing to kill.
        if (job.cancel != nullptr && job.cancel->load())
        {
            job.cancelled = true;
        }
        if (job.cancelled)
        {
            killChild(child);
        }
    }
    int exitCode = waitChild(child);
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        job.child = -1;
    }
    releaseChild(child);
    if (job.cancelled || exitCode != 0)
    {
        std::remove(tmpPath.c_str());
        if (job.cancelled)
        {
            return "the compile was cancelled";
        }
        // wamrc prints usage errors to stdout, which is discarded; an
        // unlogged exit here is otherwise a silently dead lane.
        fprintf(stderr,
                "[wasm] wamrc failed (exit %d) on %s\n",
                exitCode,
                job.wasmPath.c_str());
        return "wamrc could not compile the module";
    }
    // Atomic arrival: a partial artifact can never carry the final name.
    if (!publishFile(tmpPath, finalPath))
    {
        return "could not write the artifact into the aot cache " + cacheDir;
    }
    return std::string();
}

} // namespace rive

#endif // RIVE_WASM_TIER_LADDER
#endif // WITH_RIVE_SCRIPTING_WASM
