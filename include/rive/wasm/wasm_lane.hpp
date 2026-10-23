#ifndef _RIVE_WASM_LANE_HPP_
#define _RIVE_WASM_LANE_HPP_

#ifdef WITH_RIVE_SCRIPTING_WASM

#include "rive/refcnt.hpp"
#include "rive/span.hpp"
#include "rive/wasm/wasm_lane_layout_gen.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace rive
{
#ifndef __EMSCRIPTEN__
/// A slot in the watchdog every VM shares, which an exec env keeps across
/// calls, so arming it neither allocates nor registers, as the audio thread
/// needs. It disarms as it goes.
class WasmCallBudget
{
public:
  explicit WasmCallBudget(void* execEnv);
  ~WasmCallBudget();
  /// Interrupts the exec env's call once ms pass, until disarmed.
  void arm(uint32_t timeoutMs);
  /// Waits out a firing and clears its interrupt; true when it fired.
  bool disarm();
#if defined(WITH_RIVE_TOOLS) || defined(TESTING)
  /// Tests stand in for a slow machine: every lane interrupt lands this
  /// much past its budget.
  static void delayInterrupts(uint32_t ms);
#endif

private:
  struct State;
  std::unique_ptr<State> m_state;
};
#endif

/// What a kind of lane runs: the module section it rides in, its entries'
/// export prefix, and how its messages name its module and a trap.
struct LaneSpec
{
  const char* section;
  const char* prefix;
  const char* module;
  const char* trapped;
};
LaneSpec laneSpec(LaneKind kind);
/// How messages name a kind of lane.
const char* laneNoun(LaneKind kind);
/// A trap's text without the "Exception: " WAMR puts before it.
const char* trapText(const char* exception);

/// A loaded lane module, shared by every instance of it.
class WasmLaneModule : public RefCnt<WasmLaneModule>
{
public:
#ifdef __EMSCRIPTEN__
  std::vector<uint8_t> bytes;
#else
  struct Instance
  {
    void* instance = nullptr;
    void* execEnv = nullptr;
  };
  ~WasmLaneModule();
  static void destroy(Instance instance);
  /// Keeps an instance a job finished in for a later job to reset.
  void park(Instance instance);
  /// A parked instance, or an empty one when there is none.
  Instance unpark();

  /// A lease on the WAMR module, which keeps it loaded.
  std::shared_ptr<void> module;
  /// Exports the reset that lets one job run in another's instance.
  bool resets = false;
  /// The tier its lanes run on, as a host's log names it.
  std::string tier;
#endif
  bool compiled = false;
  /// One past the highest entry index it exports under its prefix.
  uint32_t entryCount = 0;
  /// Each entry's processor class, or job or worker function, as the bake
  /// named it; empty for a bake that named none.
  std::vector<std::string> scriptNames;
  /// The page runner's cache key for its compiled module on each thread,
  /// the same for the same bytes.
  uint32_t id = 0;
#ifndef __EMSCRIPTEN__
  /// Each entry's export name, so a run builds none.
  std::vector<std::string> entryNames;

private:
  // Whichever thread runs the next job takes one.
  std::mutex m_parkedMutex;
  std::vector<Instance> m_parked;
#endif
};

/// Console timers by label, at the steady clock's nanoseconds they started.
using ConsoleTimers = std::unordered_map<std::string, int64_t>;
void startConsoleTimer(ConsoleTimers& timers, const std::string& label);
/// What console.timeLog prints for label, or timeEnd with end, which drops it.
std::string reportConsoleTimer(
    ConsoleTimers& timers,
    const std::string& label,
    bool end);

/// Whether module itself exports a function named name, leaving out the
/// lane modules riding in its custom sections.
bool exportsFunction(Span<const uint8_t> module, std::string_view name);

/// The custom section of module called name, empty when absent.
Span<const uint8_t> findLaneSection(
    Span<const uint8_t> module,
    const char* name);
/// The lane modules a main module carries, each as hosts load it, so tools
/// compile the bytes the runtime looks artifacts up by.
std::vector<std::vector<uint8_t>> laneModules(Span<const uint8_t> mainModule);

/// A kind's lane module from its main module's section, null with no error
/// without one. It may import only what bindings.py allows its kind.
rcp<WasmLaneModule> loadLaneModule(
    Span<const uint8_t> mainModule,
    LaneKind kind,
    std::string& error);

/// Script running off its VM's thread in its kind's lane module, maybe on
/// the audio thread, so its lines and failure are written without waiting.
class WasmLane
{
public:
  WasmLane(
      LaneKind kind,
      rcp<WasmLaneModule> module,
      uint32_t entry,
      std::vector<uint8_t> input);
  virtual ~WasmLane();

  LaneKind kind() const { return m_kind; }
  LaneSpec spec() const { return laneSpec(m_kind); }
  /// How lines name the lane: its kind, then what it runs when the bake
  /// named its entries.
  std::string name() const;
  virtual LaneState state() const = 0;
  /// Asks the lane to end: a job not yet started is dropped, a worker's
  /// awaits settle as stopping, a processor leaves the mix.
  virtual void stop() = 0;
  /// Ended, so its set may drop it once no script holds it.
  virtual bool over() const { return state() >= LaneState::done; }

  /// A console line, cut to a bound; dropped when the reader is behind.
  void log(std::string_view line);
  void logUtf16(const uint8_t* text, uint32_t bytes);
  /// Why the lane failed; the first failure stays.
  void fail(std::string_view message);
  void failUtf16(const uint8_t* text, uint32_t bytes);

  // On the VM's thread.
  /// Frees the ring, then hands deliver each console line since the last
  /// take and a note of any it dropped.
  void takeLogs(
      void (*deliver)(void* context, std::string_view line),
      void* context);
  std::string failure() const;
  /// Whether it failed, from any thread.
  bool hasFailure() const { return m_failureClaimed.load(); }
#if defined(WITH_RIVE_TOOLS) || defined(TESTING)
  /// The input bytes the host still holds for it, for tests to read once it
  /// started.
  size_t heldInputBytes() const { return m_input.capacity(); }
#endif
  /// Set once the VM reported the lane's failure.
  bool reportedFailure = false;

  static WasmLane* current();
  /// The current lane when it is a T, else null.
  template <class T> static T* currentOf()
  {
    WasmLane* lane = current();
    return lane != nullptr && lane->kind() == T::kKind ? static_cast<T*>(lane)
                                                       : nullptr;
  }
  /// Returns the lane it replaced, for a lane run nested inside another.
  static WasmLane* setCurrent(WasmLane* lane);

  /// The VM whose shared memory the lane holds, and a worker or a job
  /// creates.
  uint64_t sharedOwner() const { return m_sharedOwner; }
  void setSharedOwner(uint64_t owner) { m_sharedOwner = owner; }
  /// How long a run of the lane may hold the page's thread, as a call may,
  /// 0 for no limit; only the web runs lanes there.
  uint32_t pageThreadBudgetMs() const { return m_pageThreadBudgetMs; }
  void setPageThreadBudgetMs(uint32_t ms) { m_pageThreadBudgetMs = ms; }
#ifdef __EMSCRIPTEN__
  /// Why a metered lane's fuel check must end it now, or null: a stop
  /// whose interrupt is due, or a run on the page's thread past its budget.
  const char* fuelStop() const;
#endif
  /// Holds the shared objects it was started with. A lane holds each
  /// object at most once, until it releases it or is gone.
  void holdShared(Span<const uint32_t> ids);
  /// Takes over one hold on each of ids, giving back the ones it already
  /// holds.
  void adoptShared(Span<const uint32_t> ids);
  /// What a worker or a job created, which only it may release until it
  /// is gone.
  void addCreated(uint32_t id)
  {
    std::lock_guard<std::mutex> guard(m_holdsMutex);
    m_created.push_back(id);
  }
  /// Forgets one hold on id that it created or holds, for the caller to
  /// give back; false when it has none.
  bool dropShared(uint32_t id);

#ifndef __EMSCRIPTEN__
  /// The std's console string (UTF-16 after its byte length) as a line.
  void logConsole(void* execEnv, uint32_t text);
  /// console.time on the lane's own timers, or with report timeLog, and
  /// timeEnd with end too.
  void consoleTimer(void* execEnv, uint32_t label, bool report, bool end);
#endif

protected:
  /// Ends the call running on another thread, on the web at its next fuel
  /// check.
  void interruptCall();
  /// Interrupts whatever call runs after ms, unless the instance ends
  /// first; once armed, later calls keep the first deadline.
  void interruptAfter(uint32_t ms);
  /// Disarms what interruptAfter armed, clearing an interrupt it fired.
  void dropInterrupt();
#ifndef __EMSCRIPTEN__
  /// A fresh instance of the lane module and its exec env; false, failed,
  /// when either fails.
  bool instantiate(uint32_t stackBytes);
  /// Takes an instance the module parked; false when it has none.
  bool reuse();
  /// Runs the lifted start or a reused instance's reset, then moves the input
  /// into the lane; false when a step fails, leaving any trap for failTrap.
  bool runStart(uint32_t* entry, bool reused = false);
  /// An export of the instance taking argc cells, its results fitting
  /// cells; null, failing the lane, when it is missing, unless optional, or
  /// takes other cells.
  void* lookup(
      const char* name,
      uint32_t argc,
      uint32_t cells,
      bool optional = false);
  /// Calls an export; false when lookup fails, or it trapped, which leaves
  /// the trap for failTrap.
  template <uint32_t N>
  bool call(const char* name, uint32_t argc, uint32_t (&argv)[N])
  {
    void* fn = lookup(name, argc, N);
    return fn != nullptr && call(fn, argc, argv);
  }
  /// Calls what lookup answered. Nothing puts call state back after a trap,
  /// so the instance must not run again.
  bool call(void* fn, uint32_t argc, uint32_t* argv);
  /// Fails with message, or with the trap the instance holds when it is
  /// empty, and clears the trap.
  void failTrap(std::string_view message = {});
  /// Takes the instance from the lane, so no later interrupt reaches it.
  WasmLaneModule::Instance detach();
  void teardown();

  void* m_instance = nullptr;
  void* m_execEnv = nullptr;
  // Guards the exec env against an interrupt from another thread.
  std::mutex m_envMutex;
  std::unique_ptr<WasmCallBudget> m_interruptBudget;
#else
  /// Fails with the text the page runner allocated, and frees it.
  void failTaken(char* text);
#endif
  /// Hands over the input for its one copy into the lane, so a kept handle
  /// does not pin the host's.
  std::vector<uint8_t> takeInput() { return std::exchange(m_input, {}); }
  rcp<WasmLaneModule> m_module;
  uint32_t m_entry;
  std::vector<uint8_t> m_input;

private:
  struct Text;
  void writeLine(const Text& text);
  void writeFailure(const Text& text);

  LaneKind m_kind;
  uint64_t m_sharedOwner = 0;
  uint32_t m_pageThreadBudgetMs = 0;
  bool holds(uint32_t id) const;
  // The VM's thread adds holds while the lane may already run.
  std::mutex m_holdsMutex;
  std::vector<uint32_t> m_holds;
  std::vector<uint32_t> m_created;
  // Console lines, each a two byte length then its bytes: one writer and
  // one reader, so neither waits.
  const uint32_t m_logCapacity;
  std::unique_ptr<uint8_t[]> m_logBytes;
  std::atomic<uint32_t> m_logWrite{0};
  std::atomic<uint32_t> m_logRead{0};
  std::atomic<uint32_t> m_logsDropped{0};
#ifdef __EMSCRIPTEN__
  // When a stop's interrupt is due, in steady clock nanoseconds: a web call
  // ends only at its own fuel check.
  static constexpr int64_t kNoInterrupt = INT64_MAX;
  std::atomic<int64_t> m_interruptAt{kNoInterrupt};
#else
  ConsoleTimers m_consoleTimers;
#endif
  // The first failure, published by its length once written.
  static constexpr uint32_t kFailureBytes = 1024;
  char m_failure[kFailureBytes];
  std::atomic<bool> m_failureClaimed{false};
  std::atomic<uint32_t> m_failureLength{0};
};

/// A VM's lanes of one kind: the module its main module's section carries,
/// and every lane the VM started.
template <class T> class WasmLaneSet
{
public:
  explicit WasmLaneSet(rcp<WasmLaneModule> module) : m_module(std::move(module))
  {}

  const std::vector<rcp<T>>& lanes() const { return m_lanes; }
  /// The VM whose shared memory its lanes hold.
  void setSharedOwner(uint64_t owner) { m_sharedOwner = owner; }
  uint32_t entryCount() const { return m_module->entryCount; }
  /// True when its lanes run compiled code rather than the interpreter.
  bool compiled() const { return m_module->compiled; }
#ifndef __EMSCRIPTEN__
  /// The tier its lanes run on, as a host's log names it.
  const std::string& tier() const { return m_module->tier; }
#endif
  /// Drops lanes that are over and that no script holds, once their
  /// output is out.
  void prune()
  {
    m_lanes.erase(
        std::remove_if(m_lanes.begin(), m_lanes.end(), &unneeded),
        m_lanes.end());
  }
  /// As prune, for one lane whose output is out.
  void prune(const WasmLane* lane)
  {
    auto found = std::find_if(
        m_lanes.begin(),
        m_lanes.end(),
        [lane](const rcp<T>& held) { return held.get() == lane; });
    if (found != m_lanes.end() && unneeded(*found))
    {
      m_lanes.erase(found);
    }
  }

protected:
  static bool unneeded(const rcp<T>& lane)
  {
    return lane->debugging_refcnt() == 1 && lane->over();
  }

  rcp<T> add(rcp<T> lane)
  {
    lane->setSharedOwner(m_sharedOwner);
    m_lanes.push_back(lane);
    return lane;
  }

  rcp<WasmLaneModule> m_module;
  std::vector<rcp<T>> m_lanes;
  uint64_t m_sharedOwner = 0;
};

#ifdef __EMSCRIPTEN__
/// Hands this thread's view of librive to its page runner; false when the
/// page installed none.
extern "C" int riveWebAttach();

/// Holds a lane's run on the page's thread to its page budget while this
/// lives, through the call deadline its fuel checks and region locks read.
class PageThreadBudget
{
public:
  explicit PageThreadBudget(const WasmLane& lane);
  ~PageThreadBudget();

private:
  std::chrono::steady_clock::time_point m_outer;
};
#else
/// Hardware bounds checks trap through a per thread signal handler.
void ensureLaneThreadEnv();
/// The 32 bit cells a WAMR function's parameters take, or ~0u when its
/// parameters or results would not fit in limit cells, at most 8.
uint32_t wasmCallCells(void* instance, void* function, uint32_t limit);
/// Where size bytes at appAddr in an instance's memory are, null when they
/// fall outside it.
void* wasmAppBytes(void* instance, uint32_t appAddr, uint32_t size);
/// The std's console string at text, UTF-16 after its byte length, in an
/// instance's memory: empty when text is null, null when it falls outside it.
const uint8_t* wasmConsoleText(void* instance, uint32_t text, uint32_t& bytes);
/// The natives lane modules import. A main module that compiled a lane's
/// body in links them too, where they do nothing.
void registerLaneNatives();
void registerJobNatives();
void registerWorkerNatives();
#endif

} // namespace rive

#endif
#endif
