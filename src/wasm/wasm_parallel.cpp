#if defined(WITH_RIVE_SCRIPTING_WASM) && !defined(__EMSCRIPTEN__)

#include "rive/wasm/wasm_parallel.hpp"
#include "rive/async/job_pool.hpp"
#include "rive/wasm/wasm_jobs.hpp"
#include "rive/wasm/wasm_lane.hpp"
#include "rive/wasm/wasm_lane_layout_gen.hpp"
#include "wasm_export.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>

#ifdef __APPLE__
#include <pthread.h>
#include <sys/qos.h>
#include <sys/sysctl.h>
#endif
#ifdef __linux__
#include <cstdio>
#endif
#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#endif

using namespace rive;

namespace
{
// As the main instance's exec env.
constexpr uint32_t kMemberStack = 512 * 1024;
// At least the main instance's 32 KiB spill zone, filling the frame
// collector's 64 KiB raw block after its 16 byte header.
constexpr uint32_t kSpillBytes = 65536 - 16;
// How long an idle team thread polls for the next call before it parks, and
// how long a joining caller polls before it parks.
constexpr std::chrono::microseconds kSpinTime(50);
// How long a parked caller sleeps before it looks at its watchdog again.
constexpr std::chrono::milliseconds kParkTime(1);
constexpr const char* kNested = "parallel calls cannot nest";

void cpuRelax()
{
#if defined(__aarch64__) || defined(_M_ARM64)
  __builtin_arm_yield();
#elif defined(__x86_64__) || defined(_M_X64)
  _mm_pause();
#endif
}

// Polls done until it holds or about kSpinTime passes, reading the clock
// only now and then since a read costs more than a poll.
template <typename Done> bool spinFor(Done done)
{
  auto until = std::chrono::steady_clock::now() + kSpinTime;
  for (uint32_t polls = 1;; polls++)
  {
    if (done())
    {
      return true;
    }
    cpuRelax();
    if ((polls & 63) == 0 && std::chrono::steady_clock::now() >= until)
    {
      return false;
    }
  }
}

uint32_t loadWord(const uint8_t* at)
{
  return __atomic_load_n((const uint32_t*)at, __ATOMIC_ACQUIRE);
}

void storeWord(uint8_t* at, uint32_t value)
{
  __atomic_store_n((uint32_t*)at, value, __ATOMIC_RELEASE);
}

// What a member's part left behind when it trapped without a VM.
struct MemberTrap
{
  bool noted = false;
  std::string text;
  std::string message;
};
thread_local MemberTrap* t_memberTrap = nullptr;
thread_local wasm_module_inst_t t_memberInstance = nullptr;
// Set while this thread runs a call's parts, as the caller or a member.
thread_local bool t_runningParts = false;

#if defined(WITH_RIVE_TOOLS) || defined(TESTING)
std::atomic<uint32_t> s_memberCap{UINT32_MAX};
std::atomic<uint32_t> s_memberFailFrom{UINT32_MAX};
std::atomic<uint32_t> s_memberFailures{0};
#endif

// Something the team runs a share of on each of its threads.
class TeamWork
{
public:
  virtual void runShare(uint32_t participant) = 0;

protected:
  ~TeamWork() = default;
};

// The threads every VM's parallel calls share, one call at a time. Each polls
// for a while after its last call, so a frame's next stage starts in well
// under a microsecond, then parks until a call wakes it.
class ParallelTeam
{
public:
  // The most threads a call runs on besides its caller's.
  static constexpr uint32_t kMaxWorkers = 63;

  // Leaked with its threads, as the job pool is.
  static ParallelTeam& instance()
  {
    static ParallelTeam* team = new ParallelTeam();
    return *team;
  }

  std::mutex& callMutex() { return m_callMutex; }

  // Hands work to the first workers threads, waking only those; the caller
  // holds callMutex.
  void start(uint32_t workers, TeamWork* work)
  {
    while (m_threads < workers)
    {
      uint32_t index = m_threads++;
      // Detached and never ending, so exit never waits on one.
      JobPool::makeThread([this, index]() { run(index); }).detach();
    }
    m_work = work;
    m_workers = workers;
    m_generation++;
    for (uint32_t i = 0; i < workers; i++)
    {
      Slot& slot = m_slots[i];
      slot.call.store(m_generation);
      if (slot.parked.load())
      {
        std::lock_guard<std::mutex> lock(slot.mutex);
        slot.cv.notify_one();
      }
    }
  }

  // Waits until every thread of the call has run its share, calling poll,
  // which may stop the call, now and then.
  template <typename Poll> void join(Poll poll)
  {
    auto finished = [&]() {
      poll();
      for (uint32_t i = 0; i < m_workers; i++)
      {
        if (m_slots[i].done.load() != m_generation)
        {
          return false;
        }
      }
      return true;
    };
    if (spinFor(finished))
    {
      return;
    }
    std::unique_lock<std::mutex> lock(m_joinMutex);
    m_joinParked.store(true);
    while (!m_joinCv.wait_for(lock, kParkTime, finished))
    {
    }
    m_joinParked.store(false);
  }

private:
  void run(uint32_t index)
  {
#ifdef __APPLE__
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#endif
    ensureWasmThreadEnv();
    Slot& slot = m_slots[index];
    uint64_t seen = 0;
    auto called = [&]() { return slot.call.load() != seen; };
    while (true)
    {
      if (!spinFor(called))
      {
        std::unique_lock<std::mutex> lock(slot.mutex);
        slot.parked.store(true);
        slot.cv.wait(lock, called);
        slot.parked.store(false);
      }
      seen = slot.call.load();
      m_work->runShare(index + 1);
      slot.done.store(seen);
      if (m_joinParked.load())
      {
        std::lock_guard<std::mutex> lock(m_joinMutex);
        m_joinCv.notify_one();
      }
    }
  }

  // One thread's call and its end on a line of their own, as one shared
  // count would serialize the threads.
  struct alignas(128) Slot
  {
    std::atomic<uint64_t> call{0};
    std::atomic<uint64_t> done{0};
    std::atomic<bool> parked{false};
    std::mutex mutex;
    std::condition_variable cv;
  };

  std::mutex m_callMutex;
  // Only the caller holding callMutex reads and writes these.
  uint64_t m_generation = 0;
  uint32_t m_workers = 0;
  uint32_t m_threads = 0;
  // Written before the slots' calls move, which publishes it.
  TeamWork* m_work = nullptr;
  Slot m_slots[kMaxWorkers];
  std::mutex m_joinMutex;
  std::condition_variable m_joinCv;
  std::atomic<bool> m_joinParked{false};
};

// Keeps the memory where the members read it while this thread runs parts.
class RunningParts
{
public:
  explicit RunningParts(wasm_module_inst_t instance) : m_instance(instance)
  {
    wasm_runtime_rive_freeze_memory(m_instance, true);
    t_runningParts = true;
  }
  ~RunningParts()
  {
    t_runningParts = false;
    wasm_runtime_rive_freeze_memory(m_instance, false);
  }

private:
  wasm_module_inst_t m_instance;
};
} // namespace

struct WasmParallel::Member
{
  wasm_module_inst_t instance = nullptr;
  wasm_exec_env_t env = nullptr;
  wasm_function_inst_t function = nullptr;
  uint32_t* spillPointer = nullptr;
  uint32_t* spillFloor = nullptr;
  uint32_t spill = 0;
  // What failed this member's part without a VM, if anything did.
  MemberTrap trap;

  ~Member()
  {
    if (env != nullptr)
    {
      wasm_runtime_destroy_exec_env(env);
    }
    if (instance != nullptr)
    {
      wasm_runtime_deinstantiate(instance);
    }
  }
};

// One call: its control block, read once, and which thread failed first.
struct WasmParallel::Call : public TeamWork
{
  WasmParallel* owner = nullptr;
  uint8_t* control = nullptr;
  const uint8_t* parts = nullptr;
  uint32_t thunk = 0;
  uint32_t args = 0;
  uint32_t count = 0;
  uint32_t threads = 1;
  // The thread whose part failed first, threads when the caller's watchdog
  // fired, or -1.
  std::atomic<int32_t> firstFailure{-1};

  // Stops every other part once, at the first failure.
  void fail(int32_t who)
  {
    int32_t none = -1;
    if (!firstFailure.compare_exchange_strong(none, who))
    {
      return;
    }
    storeWord(control + lane_layout::kParallelStop, 1);
    owner->m_interruptedMembers = true;
    for (uint32_t i = 1; i < threads; i++)
    {
      wasm_runtime_rive_interrupt(owner->m_members[i - 1]->env);
    }
  }

  // Runs participant's parts, failing the call at the first that traps.
  void runShare(uint32_t participant) override
  {
    Member& member = *owner->m_members[participant - 1];
    t_memberTrap = &member.trap;
    t_memberInstance = member.instance;
    t_runningParts = true;
    for (uint32_t part = participant; part < count; part += threads)
    {
      if (loadWord(control + lane_layout::kParallelStop) != 0)
      {
        break;
      }
      if (!owner->callPart(member.env, member.function, *this, part))
      {
        fail((int32_t)participant);
        break;
      }
    }
    t_runningParts = false;
    t_memberInstance = nullptr;
    t_memberTrap = nullptr;
  }
};

WasmParallel::WasmParallel(void* instance) : m_instance(instance) {}

// Members share the main instance's memory, so they go first.
WasmParallel::~WasmParallel() = default;

#if defined(WITH_RIVE_TOOLS) || defined(TESTING)
void WasmParallel::capMembers(uint32_t most) { s_memberCap.store(most); }

void WasmParallel::failMembersFrom(uint32_t first)
{
  s_memberFailFrom.store(first);
}

uint32_t WasmParallel::memberFailures() { return s_memberFailures.load(); }
#endif

// Any core at least half as fast as the fastest counts, as a phone's big
// cores vary.
uint32_t WasmParallel::performanceCores()
{
  static const uint32_t cores = []() -> uint32_t {
#if defined(__APPLE__)
    int levels = 0;
    int fast = 0;
    size_t size = sizeof(int);
    if (sysctlbyname("hw.nperflevels", &levels, &size, nullptr, 0) == 0 &&
        levels > 1 &&
        sysctlbyname("hw.perflevel0.logicalcpu", &fast, &size, nullptr, 0) ==
            0 &&
        fast > 0)
    {
      return (uint32_t)fast;
    }
#elif defined(__linux__)
    // Offline cores keep their ids, so count from the ones the kernel could
    // ever run, such as 0-3,6-7.
    std::vector<uint32_t> cpus;
    if (FILE* possible = fopen("/sys/devices/system/cpu/possible", "r"))
    {
      unsigned first = 0;
      unsigned last = 0;
      int read = 0;
      while ((read = fscanf(possible, "%u-%u", &first, &last)) >= 1)
      {
        for (unsigned cpu = first; cpu <= (read == 2 ? last : first); cpu++)
        {
          cpus.push_back(cpu);
        }
        if (fgetc(possible) != ',')
        {
          break;
        }
      }
      fclose(possible);
    }
    std::vector<uint32_t> capacities;
    for (uint32_t cpu : cpus)
    {
      char path[64];
      snprintf(
          path,
          sizeof(path),
          "/sys/devices/system/cpu/cpu%u/cpu_capacity",
          cpu);
      FILE* file = fopen(path, "r");
      unsigned capacity = 0;
      bool read = file != nullptr && fscanf(file, "%u", &capacity) == 1;
      if (file != nullptr)
      {
        fclose(file);
      }
      if (!read)
      {
        return UINT32_MAX;
      }
      capacities.push_back(capacity);
    }
    if (!capacities.empty())
    {
      uint32_t fastest = *std::max_element(
          capacities.begin(),
          capacities.end());
      return (uint32_t)std::count_if(
          capacities.begin(),
          capacities.end(),
          [fastest](uint32_t capacity) { return capacity * 2 >= fastest; });
    }
#endif
    return UINT32_MAX;
  }();
  return cores;
}

uint32_t WasmParallel::size()
{
  bool chosen = false;
  uint32_t threads = std::min(
                         WasmJobs::poolThreads(&chosen),
                         ParallelTeam::kMaxWorkers) +
                     1;
  // A part on a slower core holds back every sync, so left to the default a
  // call keeps to the fast ones.
  return chosen ? threads : std::min(threads, performanceCores());
}

void WasmParallel::noteMemberTrap(
    const char* text,
    const char* message,
    uint32_t length)
{
  MemberTrap* trap = t_memberTrap;
  if (trap == nullptr)
  {
    return;
  }
  trap->noted = true;
  trap->text = text;
  trap->message.assign(message != nullptr ? message : "", length);
}

void WasmParallel::refuseMemberCall()
{
  if (t_memberInstance != nullptr)
  {
    noteMemberTrap(kNested, nullptr, 0);
    wasm_runtime_set_exception(t_memberInstance, kNested);
  }
}

bool WasmParallel::runningParts() { return t_runningParts; }

bool WasmParallel::callPart(
    void* execEnv,
    void* function,
    const Call& call,
    uint32_t part)
{
  uint32_t partObject;
  memcpy(&partObject, call.parts + part * 4, 4);
  uint32_t argv[3] = {call.thunk, partObject, call.args};
  auto env = (wasm_exec_env_t)execEnv;
  auto fn = (wasm_function_inst_t)function;
  return execEnv == nullptr || fn == nullptr ? false
         : wasm_runtime_get_module_inst(env) == m_instance
             ? wasm_runtime_call_wasm_nested(env, fn, 3, argv)
             : wasm_runtime_call_wasm(env, fn, 3, argv);
}

static uint32_t memoryPages(wasm_module_inst_t instance)
{
  wasm_memory_inst_t memory = wasm_runtime_get_default_memory(instance);
  return memory != nullptr ? (uint32_t)wasm_memory_get_cur_page_count(memory)
                           : 0;
}

// Makes members up to wanted, each with a spill zone of its own, and
// answers how many there are. size is what size() answered for the call.
uint32_t WasmParallel::members(void* execEnv, uint32_t wanted, uint32_t size)
{
#if defined(WITH_RIVE_TOOLS) || defined(TESTING)
  wanted = std::min(wanted, s_memberCap.load());
#endif
  auto main = (wasm_module_inst_t)m_instance;
  if (m_memberLimit != UINT32_MAX &&
      (memoryPages(main) != m_limitPages || size != m_limitSize))
  {
    m_memberLimit = UINT32_MAX;
  }
  wanted = std::min(wanted, m_memberLimit);
  wasm_function_inst_t rawAlloc = nullptr;
  while (m_members.size() < wanted)
  {
    auto member = std::make_unique<Member>();
    char error[128] = {};
    member->instance = wasm_runtime_instantiate_parallel_member(
        main,
        kMemberStack,
        error,
        sizeof(error));
    member->env = member->instance != nullptr ? wasm_runtime_create_exec_env(
                                                    member->instance,
                                                    kMemberStack)
                                              : nullptr;
    member->function = member->env != nullptr ? wasm_runtime_lookup_function(
                                                    member->instance,
                                                    "__riveParallelCall")
                                              : nullptr;
    bool made = member->function != nullptr;
#if defined(WITH_RIVE_TOOLS) || defined(TESTING)
    made = made && m_members.size() < s_memberFailFrom.load();
#endif
    wasm_global_inst_t floor;
    wasm_global_inst_t pointer;
    // A module whose code never spills exports no spill floor.
    if (made && wasm_runtime_get_export_global_inst(
                    member->instance,
                    "__riveSpillFloor",
                    &floor))
    {
      if (rawAlloc == nullptr)
      {
        rawAlloc = wasm_runtime_lookup_function(main, "__riveRawAlloc");
      }
      uint32_t argv[1] = {kSpillBytes};
      made = rawAlloc != nullptr &&
             wasm_runtime_get_export_global_inst(
                 member->instance,
                 "__riveCallSpill",
                 &pointer) &&
             wasm_runtime_call_wasm_nested(
                 (wasm_exec_env_t)execEnv,
                 rawAlloc,
                 1,
                 argv) &&
             argv[0] != 0;
      if (made)
      {
        member->spill = argv[0];
        member->spillPointer = (uint32_t*)pointer.global_data;
        member->spillFloor = (uint32_t*)floor.global_data;
      }
      else
      {
        wasm_runtime_clear_exception(main);
      }
    }
    if (!made)
    {
#if defined(WITH_RIVE_TOOLS) || defined(TESTING)
      s_memberFailures++;
#endif
      m_memberLimit = (uint32_t)m_members.size();
      m_limitPages = memoryPages(main);
      m_limitSize = size;
      break;
    }
    m_members.push_back(std::move(member));
  }
  return (uint32_t)std::min<size_t>(m_members.size(), wanted);
}

WasmParallel::Outcome WasmParallel::run(
    void* execEnv,
    uint32_t control,
    bool oneThread)
{
  Outcome outcome;
  auto trap = [&](const char* message) {
    outcome.kind = Outcome::Kind::trapped;
    outcome.message = message;
    return outcome;
  };
  if (t_runningParts)
  {
    return trap(kNested);
  }
  auto main = (wasm_module_inst_t)m_instance;
  auto env = (wasm_exec_env_t)execEnv;
  // A word an atomic reads must be aligned, or some arm64 cores fault.
  if ((control & 3) != 0)
  {
    return trap("parallel call control block is not aligned");
  }
  auto words = (const uint8_t*)
      wasmAppBytes(main, control, lane_layout::kParallelBytes);
  if (words == nullptr)
  {
    return trap("parallel call control block out of bounds");
  }
  if (m_function == nullptr)
  {
    m_function = wasm_runtime_lookup_function(main, "__riveParallelCall");
    if (m_function == nullptr)
    {
      return trap("the module exports no __riveParallelCall");
    }
  }
  Call call;
  call.owner = this;
  call.thunk = loadWord(words + lane_layout::kParallelThunk);
  call.args = loadWord(words + lane_layout::kParallelArgs);
  call.count = loadWord(words + lane_layout::kParallelCount);
  uint32_t parts = loadWord(words + lane_layout::kParallelParts);
  bool atOnce = loadWord(words + lane_layout::kParallelAtOnce) != 0;
  if (call.count == 0 || call.count > 0x10000000 ||
      wasmAppBytes(main, parts, call.count * 4) == nullptr)
  {
    return trap("parallel call parts out of bounds");
  }
  ParallelTeam& team = ParallelTeam::instance();
  // Another VM's call holds the team, so this one runs on its caller alone
  // rather than wait.
  std::unique_lock<std::mutex> lock(team.callMutex(), std::defer_lock);
  uint32_t most = oneThread || !lock.try_lock() ? 1 : size();
  uint32_t wanted = std::min(call.count, most);
  call.threads = 1 + (wanted > 1 ? members(execEnv, wanted - 1, most) : 0);
  outcome.threads = call.threads;
  if (call.threads == 1 && lock.owns_lock())
  {
    lock.unlock();
  }
  // Making members may have grown the memory, which moves it.
  call.control = (uint8_t*)
      wasmAppBytes(main, control, lane_layout::kParallelBytes);
  call.parts = (const uint8_t*)wasmAppBytes(main, parts, call.count * 4);
  // A run's parts may sync, so it gets one part per thread, however few.
  if (atOnce)
  {
    call.count = call.threads;
    storeWord(call.control + lane_layout::kParallelCount, call.count);
  }
  storeWord(
      call.control + lane_layout::kParallelAtOnce,
      call.threads >= call.count ? 1 : 0);
  for (uint32_t word :
       {lane_layout::kParallelGeneration,
        lane_layout::kParallelArrived,
        lane_layout::kParallelSleepers,
        lane_layout::kParallelClaim,
        lane_layout::kParallelClaim + 4,
        lane_layout::kParallelStop})
  {
    storeWord(call.control + word, 0);
  }
  // A native caller may park at a sync, unlike a page's thread.
  storeWord(call.control + lane_layout::kParallelCallerParks, 1);
  RunningParts running(main);
  if (call.threads == 1)
  {
    for (uint32_t part = 0; part < call.count; part++)
    {
      if (!callPart(execEnv, m_function, call, part))
      {
        outcome.kind = Outcome::Kind::callerFailed;
        return outcome;
      }
    }
    return outcome;
  }
  if (m_interruptedMembers)
  {
    for (const std::unique_ptr<Member>& member : m_members)
    {
      wasm_runtime_rive_clear_interrupt(member->env);
    }
    m_interruptedMembers = false;
  }
  for (uint32_t i = 0; i + 1 < call.threads; i++)
  {
    Member& member = *m_members[i];
    // Members see globals as the call found them, and spill apart.
    wasm_runtime_copy_globals(member.instance, main);
    if (member.spillPointer != nullptr)
    {
      *member.spillFloor = member.spill;
      *member.spillPointer = member.spill + kSpillBytes;
    }
    member.trap = {};
  }
  team.start(call.threads - 1, &call);
  for (uint32_t part = 0; part < call.count; part += call.threads)
  {
    if (loadWord(call.control + lane_layout::kParallelStop) != 0)
    {
      break;
    }
    if (!callPart(execEnv, m_function, call, part))
    {
      call.fail(0);
      break;
    }
  }
  // The watchdog interrupts the caller's env; while it waits here, that
  // stops every member instead.
  team.join([&]() {
    if (call.firstFailure.load() < 0 && wasm_runtime_rive_interrupted(env))
    {
      call.fail((int32_t)call.threads);
    }
  });
  int32_t first = call.firstFailure.load();
  if (first == 0)
  {
    outcome.kind = Outcome::Kind::callerFailed;
  }
  else if (first == (int32_t)call.threads)
  {
    outcome.kind = Outcome::Kind::interrupted;
  }
  else if (first > 0)
  {
    Member& member = *m_members[first - 1];
    const char* exception = trapText(
        wasm_runtime_get_exception(member.instance));
    if (member.trap.noted && member.trap.text == "execution exceeded timeout")
    {
      outcome.kind = Outcome::Kind::budget;
    }
    else if (member.trap.noted && !member.trap.message.empty())
    {
      outcome.kind = Outcome::Kind::threw;
      outcome.message = member.trap.message;
    }
    else
    {
      outcome.kind = Outcome::Kind::trapped;
      outcome.message = exception != nullptr ? exception
                                             : "parallel part trapped";
    }
  }
  for (uint32_t i = 0; i + 1 < call.threads; i++)
  {
    wasm_runtime_clear_exception(m_members[i]->instance);
  }
  return outcome;
}

#endif
