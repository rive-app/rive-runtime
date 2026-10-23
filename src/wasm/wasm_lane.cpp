#ifdef WITH_RIVE_SCRIPTING_WASM

#include "rive/wasm/wasm_lane.hpp"
#include "rive/async/compare_exchange.hpp"
#include "rive/wasm/wasm_lane_policy.hpp"
#include "rive/wasm/wasm_shared.hpp"
#include "wasm_lane_imports_gen.hpp"
#include "wasm_utf16.hpp"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <emscripten/threading.h>
#else
#include "rive/wasm/wasm_scripting_vm.hpp"
#include "wasm_export.h"
#endif

#include <charconv>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string_view>

using namespace rive;

namespace
{
thread_local WasmLane* t_lane = nullptr;

bool readLeb(Span<const uint8_t> bytes, size_t& at, uint32_t& value)
{
  value = 0;
  for (uint32_t shift = 0; shift < 35; shift += 7)
  {
    if (at >= bytes.size())
    {
      return false;
    }
    uint8_t byte = bytes[at++];
    value |= (uint32_t)(byte & 0x7f) << shift;
    if ((byte & 0x80) == 0)
    {
      return true;
    }
  }
  return false;
}
} // namespace

LaneSpec rive::laneSpec(LaneKind kind)
{
  int i = (int)kind;
  return {
      kLaneSections[i],
      kLaneEntryPrefixes[i],
      kLaneModuleNames[i],
      kLaneTrapTexts[i]};
}

const char* rive::laneNoun(LaneKind kind) { return kLaneKindNames[(int)kind]; }

const char* rive::trapText(const char* exception)
{
  constexpr char kPrefix[] = "Exception: ";
  constexpr size_t kPrefixLength = sizeof(kPrefix) - 1;
  return exception != nullptr && strncmp(exception, kPrefix, kPrefixLength) == 0
             ? exception + kPrefixLength
             : exception;
}

std::string WasmLane::name() const
{
  const std::vector<std::string>& names = m_module->scriptNames;
  std::string name = laneNoun(m_kind);
  if (m_entry < names.size())
  {
    name += ' ';
    name += names[m_entry];
  }
  return name;
}

// --- text a lane reports -----------------------------------------------------

// UTF-8, or the UTF-16 a module passes, encoded as UTF-8 one byte at a time,
// so writing it into a lane's fixed buffers never allocates.
struct WasmLane::Text
{
  const uint8_t* data;
  uint32_t bytes;
  bool utf16;

  // Answers how many bytes the whole characters that fit limit take, and
  // writes the first room of them to buffer from at, wrapping by mask.
  uint32_t encode(
      uint32_t limit,
      uint8_t* buffer,
      uint32_t at,
      uint32_t room,
      uint32_t mask) const
  {
    uint32_t out = 0;
    uint8_t chunk[4];
    for (uint32_t i = 0; i < bytes;)
    {
      uint32_t size = 0;
      if (!utf16)
      {
        uint8_t lead = data[i];
        size = lead < 0xc0 ? 1 : lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4;
        size = std::min(size, bytes - i);
        memcpy(chunk, data + i, size);
        i += size;
      }
      else if (i + 1 < bytes)
      {
        size = utf8Of(nextUtf16(data, bytes, i), chunk);
      }
      else
      {
        break;
      }
      if (out + size > limit)
      {
        break;
      }
      for (uint32_t k = 0; k < size; k++, out++)
      {
        if (out < room)
        {
          buffer[(at + out) & mask] = chunk[k];
        }
      }
    }
    return out;
  }
};

// A worker's console holds long dumps, while every job makes a console of its
// own and a processor writes from the audio thread.
static constexpr uint32_t kWorkerLogBytes = 64 * 1024;
static constexpr uint32_t kLogBytes = 8 * 1024;
// The ring's counters wrap at 2^32, which only a power of two divides.
static_assert(
    (kWorkerLogBytes & (kWorkerLogBytes - 1)) == 0 &&
    (kLogBytes & (kLogBytes - 1)) == 0);

WasmLane::WasmLane(
    LaneKind kind,
    rcp<WasmLaneModule> module,
    uint32_t entry,
    std::vector<uint8_t> input)
    : m_module(std::move(module))
    , m_entry(entry)
    , m_input(std::move(input))
    , m_kind(kind)
    , m_logCapacity(kind == LaneKind::worker ? kWorkerLogBytes : kLogBytes)
    , m_logBytes(new uint8_t[m_logCapacity])
{}

WasmLane::~WasmLane()
{
  // What it created outlives it only where another hold remains, as the
  // main lane takes on decoding its result.
  for (const auto* ids : {&m_holds, &m_created})
  {
    for (uint32_t id : *ids)
    {
      SharedRegistry::release(id, m_sharedOwner);
    }
  }
}

static bool eraseId(std::vector<uint32_t>& ids, uint32_t id)
{
  auto found = std::find(ids.begin(), ids.end(), id);
  if (found == ids.end())
  {
    return false;
  }
  *found = ids.back();
  ids.pop_back();
  return true;
}

bool WasmLane::holds(uint32_t id) const
{
  return std::find(m_holds.begin(), m_holds.end(), id) != m_holds.end();
}

void WasmLane::holdShared(Span<const uint32_t> ids)
{
  std::lock_guard<std::mutex> guard(m_holdsMutex);
  for (uint32_t id : ids)
  {
    if (!holds(id) && SharedRegistry::retain(id, m_sharedOwner))
    {
      m_holds.push_back(id);
    }
  }
}

void WasmLane::adoptShared(Span<const uint32_t> ids)
{
  std::lock_guard<std::mutex> guard(m_holdsMutex);
  for (uint32_t id : ids)
  {
    if (holds(id))
    {
      SharedRegistry::release(id, m_sharedOwner);
    }
    else
    {
      m_holds.push_back(id);
    }
  }
}

bool WasmLane::dropShared(uint32_t id)
{
  std::lock_guard<std::mutex> guard(m_holdsMutex);
  return eraseId(m_created, id) || eraseId(m_holds, id);
}

WasmLane* WasmLane::current() { return t_lane; }

WasmLane* WasmLane::setCurrent(WasmLane* lane)
{
  WasmLane* outer = t_lane;
  t_lane = lane;
  return outer;
}

// Never waits on the reader: a line that does not fit is dropped.
void WasmLane::writeLine(const Text& text)
{
  // Cut, so a line always fits an empty ring.
  constexpr uint32_t kLineBytes = 1024;
  uint32_t write = m_logWrite.load(std::memory_order_relaxed);
  uint32_t read = m_logRead.load(std::memory_order_acquire);
  uint32_t space = m_logCapacity - (write - read);
  uint32_t mask = m_logCapacity - 1;
  // Writes only into free space, which the reader sees once published.
  uint32_t length = text.encode(
      kLineBytes,
      m_logBytes.get(),
      write + 2,
      space - std::min(space, 2u),
      mask);
  if (2 + length > space)
  {
    m_logsDropped.fetch_add(1);
    return;
  }
  m_logBytes[write & mask] = (uint8_t)length;
  m_logBytes[(write + 1) & mask] = (uint8_t)(length >> 8);
  m_logWrite.store(write + 2 + length, std::memory_order_release);
}

void WasmLane::writeFailure(const Text& text)
{
  if (m_failureClaimed.exchange(true))
  {
    return;
  }
  m_failureLength.store(
      text.encode(
          kFailureBytes,
          (uint8_t*)m_failure,
          0,
          kFailureBytes,
          UINT32_MAX),
      std::memory_order_release);
}

void WasmLane::log(std::string_view line)
{
  writeLine({(const uint8_t*)line.data(), (uint32_t)line.size(), false});
}

void WasmLane::logUtf16(const uint8_t* text, uint32_t bytes)
{
  writeLine({text, bytes, true});
}

void WasmLane::fail(std::string_view message)
{
  writeFailure(
      {(const uint8_t*)message.data(), (uint32_t)message.size(), false});
}

void WasmLane::failUtf16(const uint8_t* text, uint32_t bytes)
{
  writeFailure({text, bytes, true});
}

void WasmLane::takeLogs(
    void (*deliver)(void* context, std::string_view line),
    void* context)
{
  uint32_t read = m_logRead.load(std::memory_order_relaxed);
  uint32_t write = m_logWrite.load(std::memory_order_acquire);
  uint32_t mask = m_logCapacity - 1;
  std::string lines(write - read, '\0');
  for (uint32_t i = 0; i < lines.size(); i++)
  {
    lines[i] = (char)m_logBytes[(read + i) & mask];
  }
  m_logRead.store(write, std::memory_order_release);
  uint32_t dropped = m_logsDropped.exchange(0);
  for (size_t at = 0; at < lines.size();)
  {
    size_t length = (uint8_t)lines[at] | ((uint8_t)lines[at + 1] << 8);
    deliver(context, std::string_view(lines.data() + at + 2, length));
    at += 2 + length;
  }
  if (dropped != 0)
  {
    char note[96];
    snprintf(
        note,
        sizeof(note),
        " dropped %u console lines: it wrote more than %u KB between two "
        "handovers",
        dropped,
        m_logCapacity / 1024);
    deliver(context, name() + note);
  }
}

std::string WasmLane::failure() const
{
  return std::string(
      m_failure,
      m_failureLength.load(std::memory_order_acquire));
}

namespace
{
int64_t steadyNanos()
{
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}
} // namespace

void rive::startConsoleTimer(ConsoleTimers& timers, const std::string& label)
{
  timers[label] = steadyNanos();
}

std::string rive::reportConsoleTimer(
    ConsoleTimers& timers,
    const std::string& label,
    bool end)
{
  auto timer = timers.find(label);
  if (timer == timers.end())
  {
    return "Timer '" + label + "' does not exist";
  }
  char elapsed[32];
  snprintf(
      elapsed,
      sizeof(elapsed),
      ": %.3fms",
      (double)(steadyNanos() - timer->second) / 1e6);
  if (end)
  {
    timers.erase(timer);
  }
  return label + elapsed;
}

// --- lane modules ------------------------------------------------------------

namespace
{
// Calls visit with each section's id and bytes until it answers true.
template <typename Visit>
void visitSections(Span<const uint8_t> module, Visit&& visit)
{
  static const uint8_t kHeader[] = {0x00, 0x61, 0x73, 0x6d};
  if (module.size() < 8 || memcmp(module.data(), kHeader, 4) != 0)
  {
    return;
  }
  size_t at = 8;
  while (at < module.size())
  {
    uint8_t id = module[at++];
    uint32_t size = 0;
    if (!readLeb(module, at, size) || size > module.size() - at ||
        visit(id, module.subset(at, size)))
    {
      return;
    }
    at += size;
  }
}

// Reads a length prefixed name; false when it runs past bytes.
bool readName(Span<const uint8_t> bytes, size_t& at, std::string_view& name)
{
  uint32_t length = 0;
  if (!readLeb(bytes, at, length) || length > bytes.size() - at)
  {
    return false;
  }
  name = std::string_view((const char*)bytes.data() + at, length);
  at += length;
  return true;
}

// Calls visit with each function export's name until it answers true.
template <typename Visit>
void visitFunctionExports(Span<const uint8_t> module, Visit&& visit)
{
  visitSections(module, [&](uint8_t id, Span<const uint8_t> section) {
    constexpr uint8_t kExportSection = 7;
    constexpr uint8_t kFunctionExport = 0;
    if (id != kExportSection)
    {
      return false;
    }
    size_t at = 0;
    uint32_t count = 0;
    readLeb(section, at, count);
    for (uint32_t i = 0; i < count; i++)
    {
      std::string_view name;
      uint32_t index = 0;
      if (!readName(section, at, name) || at >= section.size())
      {
        break;
      }
      uint8_t kind = section[at++];
      if (!readLeb(section, at, index) ||
          (kind == kFunctionExport && visit(name, count)))
      {
        break;
      }
    }
    return true;
  });
}

// One past the highest index of the functions a module exports under prefix.
uint32_t laneEntryCount(Span<const uint8_t> module, std::string_view prefix)
{
  uint32_t entries = 0;
  visitFunctionExports(module, [&](std::string_view name, uint32_t count) {
    uint32_t entry = 0;
    if (name.size() > prefix.size() &&
        name.substr(0, prefix.size()) == prefix &&
        std::from_chars(
            name.data() + prefix.size(),
            name.data() + name.size(),
            entry)
                .ec == std::errc() &&
        entry < count)
    {
      entries = std::max(entries, entry + 1);
    }
    return false;
  });
  return entries;
}

// The name a names section gives each entry; none unless it names every
// entry, so a line never names the wrong one.
std::vector<std::string> readScriptNames(
    Span<const uint8_t> section,
    uint32_t entries)
{
  size_t at = 0;
  uint32_t count = 0;
  std::vector<std::string> names;
  if (!readLeb(section, at, count) || count != entries)
  {
    return names;
  }
  for (uint32_t i = 0; i < count; i++)
  {
    std::string_view name;
    if (!readName(section, at, name))
    {
      return {};
    }
    names.emplace_back(name);
  }
  return names;
}
} // namespace

bool rive::exportsFunction(Span<const uint8_t> module, std::string_view name)
{
  bool found = false;
  visitFunctionExports(module, [&](std::string_view exported, uint32_t) {
    found = exported == name;
    return found;
  });
  return found;
}

namespace
{
void writeLeb(std::vector<uint8_t>& out, uint32_t value)
{
  do
  {
    uint8_t byte = value & 0x7f;
    value >>= 7;
    out.push_back(value != 0 ? byte | 0x80 : byte);
  } while (value != 0);
}

// Exports the start function, which runs rasc's global initializers, for the
// host to call where a stop or bound reaches it, and drops its section.
std::vector<uint8_t> liftStart(Span<const uint8_t> module)
{
  constexpr uint8_t kExportSection = 7;
  constexpr uint8_t kStartSection = 8;
  uint32_t start = 0;
  bool hasStart = false;
  bool hasExports = false;
  visitSections(module, [&](uint8_t id, Span<const uint8_t> body) {
    size_t at = 0;
    hasStart |= id == kStartSection && readLeb(body, at, start);
    hasExports |= id == kExportSection;
    return false;
  });
  if (!hasStart || !hasExports)
  {
    return std::vector<uint8_t>(module.begin(), module.end());
  }
  std::vector<uint8_t> out(module.begin(), module.begin() + 8);
  std::vector<uint8_t> exports;
  visitSections(module, [&](uint8_t id, Span<const uint8_t> body) {
    if (id == kStartSection)
    {
      return false;
    }
    if (id == kExportSection)
    {
      size_t at = 0;
      uint32_t count = 0;
      readLeb(body, at, count);
      writeLeb(exports, count + 1);
      exports.insert(exports.end(), body.begin() + at, body.end());
      std::string_view name = lane_layout::kLiftedStartExport;
      writeLeb(exports, (uint32_t)name.size());
      exports.insert(exports.end(), name.begin(), name.end());
      constexpr uint8_t kFunctionExport = 0;
      exports.push_back(kFunctionExport);
      writeLeb(exports, start);
      body = Span<const uint8_t>(exports.data(), exports.size());
    }
    out.push_back(id);
    writeLeb(out, (uint32_t)body.size());
    out.insert(out.end(), body.begin(), body.end());
    return false;
  });
  return out;
}
#ifdef __EMSCRIPTEN__
// Runners cache compiled lane modules by id, so equal bytes, not just equal
// hashes, share one whichever file carries them.
uint32_t laneModuleId(const std::vector<uint8_t>& bytes)
{
  // Past this many modules the list starts over, which costs only a miss.
  constexpr uint32_t kKeptModules = 32;
  static std::mutex* mutex = new std::mutex();
  static auto* kept = new std::vector<uint8_t>[kKeptModules];
  static uint32_t count = 0;
  // The first kept module's id; each one after it has the next.
  static uint32_t first = 1;
  std::lock_guard<std::mutex> lock(*mutex);
  for (uint32_t i = 0; i < count; i++)
  {
    if (kept[i] == bytes)
    {
      return first + i;
    }
  }
  if (count == kKeptModules)
  {
    first += count;
    // Assigning {} would keep each copy's memory.
    while (count > 0)
    {
      kept[--count] = std::vector<uint8_t>();
    }
  }
  kept[count] = bytes;
  return first + count++;
}
#endif
} // namespace

std::vector<std::vector<uint8_t>> rive::laneModules(
    Span<const uint8_t> mainModule)
{
  std::vector<std::vector<uint8_t>> modules;
  for (const char* name : kLaneSections)
  {
    Span<const uint8_t> section = findLaneSection(mainModule, name);
    if (!section.empty())
    {
      modules.push_back(liftStart(section));
    }
  }
  return modules;
}

Span<const uint8_t> rive::findLaneSection(
    Span<const uint8_t> module,
    const char* name)
{
  constexpr uint8_t kCustomSection = 0;
  Span<const uint8_t> found;
  visitSections(module, [&](uint8_t id, Span<const uint8_t> section) {
    size_t at = 0;
    std::string_view sectionName;
    if (id != kCustomSection || !readName(section, at, sectionName) ||
        sectionName != name)
    {
      return false;
    }
    found = section.subset(at, section.size() - at);
    return true;
  });
  return found;
}

// --- host limits -------------------------------------------------------------

namespace
{
std::mutex& limitsMutex()
{
  static std::mutex mutex;
  return mutex;
}

ScriptingLimits& limitsSlot()
{
  static ScriptingLimits limits;
  return limits;
}
} // namespace

void ScriptingLimits::set(const ScriptingLimits& limits)
{
  std::lock_guard<std::mutex> lock(limitsMutex());
  limitsSlot() = limits;
}

ScriptingLimits ScriptingLimits::get()
{
  std::lock_guard<std::mutex> lock(limitsMutex());
  return limitsSlot();
}

#ifdef __EMSCRIPTEN__
// The page runner reports a lane's throws and console lines through these,
// with the lane it was handed.
extern "C" EMSCRIPTEN_KEEPALIVE void rive_web_lane_fail(
    uint32_t lane,
    const char* text)
{
  ((WasmLane*)(uintptr_t)lane)->fail(text);
}

extern "C" EMSCRIPTEN_KEEPALIVE void rive_web_lane_log(
    uint32_t lane,
    const char* text)
{
  ((WasmLane*)(uintptr_t)lane)->log(text);
}

// A metered lane's fuel check throws what this answers.
extern "C" EMSCRIPTEN_KEEPALIVE const char* rive_web_lane_fuel(uint32_t lane)
{
  return ((WasmLane*)(uintptr_t)lane)->fuelStop();
}

void WasmLane::failTaken(char* text)
{
  fail(text);
  free(text);
}

void WasmLane::interruptCall() { m_interruptAt.store(0); }

void WasmLane::interruptAfter(uint32_t ms)
{
  int64_t unarmed = kNoInterrupt;
  compareExchangeStrong(
      m_interruptAt,
      unarmed,
      steadyNanos() + (int64_t)ms * 1000000);
}

void WasmLane::dropInterrupt() { m_interruptAt.store(kNoInterrupt); }

const char* WasmLane::fuelStop() const
{
  using Clock = std::chrono::steady_clock;
  int64_t interruptAt = m_interruptAt.load();
  if (interruptAt != kNoInterrupt && steadyNanos() >= interruptAt)
  {
    return "lane was stopped";
  }
  Clock::time_point deadline = callDeadline();
  return deadline != Clock::time_point::max() && Clock::now() >= deadline
             ? "execution exceeded timeout on the page's thread"
             : nullptr;
}

PageThreadBudget::PageThreadBudget(const WasmLane& lane)
{
  using Clock = std::chrono::steady_clock;
  uint32_t ms = emscripten_is_main_browser_thread() ? lane.pageThreadBudgetMs()
                                                    : 0;
  m_outer = armCallDeadline(
      ms == 0 ? Clock::time_point::max()
              : Clock::now() + std::chrono::milliseconds(ms));
}

PageThreadBudget::~PageThreadBudget() { restoreCallDeadline(m_outer); }
#else

namespace
{
void laneError(wasm_exec_env_t env, uint32_t text, uint32_t bytes)
{
  WasmLane* lane = WasmLane::current();
  auto data = (const uint8_t*)
      wasmAppBytes(wasm_runtime_get_module_inst(env), text, bytes);
  if (lane != nullptr && data != nullptr)
  {
    lane->failUtf16(data, bytes);
  }
}

// The watchdog's interrupts stop and bound every lane here, so a metered
// lane's check has nothing to ask.
void laneFuelCheck(wasm_exec_env_t) {}

NativeSymbol kLaneNatives[] = {
    {"error", (void*)laneError, "(ii)", nullptr},
    {"fuel_check", (void*)laneFuelCheck, "()", nullptr},
};
} // namespace

void rive::registerLaneNatives()
{
  static std::once_flag once;
  std::call_once(once, []() {
    wasm_runtime_register_natives(
        "rive_lane_v1",
        kLaneNatives,
        sizeof(kLaneNatives) / sizeof(kLaneNatives[0]));
  });
}

uint32_t rive::wasmCallCells(void* instance, void* function, uint32_t limit)
{
  auto inst = (wasm_module_inst_t)instance;
  auto fn = (wasm_function_inst_t)function;
  auto cells = [](wasm_valkind_t kind) -> uint32_t {
    return kind == WASM_I64 || kind == WASM_F64 ? 2 : kind == WASM_V128 ? 4 : 1;
  };
  constexpr uint32_t kMaxCells = 8;
  limit = std::min(limit, kMaxCells);
  uint32_t params = wasm_func_get_param_count(fn, inst);
  uint32_t results = wasm_func_get_result_count(fn, inst);
  if (params > limit || results > limit)
  {
    return ~0u;
  }
  wasm_valkind_t kinds[kMaxCells];
  uint32_t paramCells = 0;
  wasm_func_get_param_types(fn, inst, kinds);
  for (uint32_t i = 0; i < params; i++)
  {
    paramCells += cells(kinds[i]);
  }
  uint32_t resultCells = 0;
  wasm_func_get_result_types(fn, inst, kinds);
  for (uint32_t i = 0; i < results; i++)
  {
    resultCells += cells(kinds[i]);
  }
  return resultCells > limit ? ~0u : paramCells;
}

void* rive::wasmAppBytes(void* instance, uint32_t appAddr, uint32_t size)
{
  auto inst = (wasm_module_inst_t)instance;
  return wasm_runtime_validate_app_addr(inst, appAddr, size)
             ? wasm_runtime_addr_app_to_native(inst, appAddr)
             : nullptr;
}

void rive::ensureLaneThreadEnv()
{
#ifdef RIVE_WASM_HW_BOUNDS
  struct ThreadEnv
  {
    ThreadEnv() { wasm_runtime_init_thread_env(); }
    ~ThreadEnv() { wasm_runtime_destroy_thread_env(); }
  };
  static thread_local ThreadEnv env;
  (void)env;
#endif
}

const uint8_t* rive::wasmConsoleText(
    void* instance,
    uint32_t text,
    uint32_t& bytes)
{
  bytes = 0;
  if (text >= 4)
  {
    const void* length = wasmAppBytes(instance, text - 4, 4);
    if (length == nullptr)
    {
      return nullptr;
    }
    memcpy(&bytes, length, 4);
  }
  // WAMR has no address for an empty string at the end of memory.
  return (const uint8_t*)(bytes == 0 ? ""
                                     : wasmAppBytes(instance, text, bytes));
}

// The std's console string as its UTF-16 bytes, null when out of bounds.
static std::string_view consoleUtf16(void* execEnv, uint32_t text)
{
  uint32_t bytes = 0;
  const uint8_t* data = wasmConsoleText(
      wasm_runtime_get_module_inst((wasm_exec_env_t)execEnv),
      text,
      bytes);
  return data != nullptr ? std::string_view((const char*)data, bytes)
                         : std::string_view();
}

void WasmLane::logConsole(void* execEnv, uint32_t text)
{
  std::string_view line = consoleUtf16(execEnv, text);
  if (line.data() != nullptr)
  {
    logUtf16((const uint8_t*)line.data(), (uint32_t)line.size());
  }
}

void WasmLane::consoleTimer(
    void* execEnv,
    uint32_t label,
    bool report,
    bool end)
{
  std::string_view utf16 = consoleUtf16(execEnv, label);
  Text text{(const uint8_t*)utf16.data(), (uint32_t)utf16.size(), true};
  std::string name(text.encode(UINT32_MAX, nullptr, 0, 0, 0), '\0');
  text.encode(UINT32_MAX, (uint8_t*)name.data(), 0, UINT32_MAX, UINT32_MAX);
  if (!report)
  {
    startConsoleTimer(m_consoleTimers, name);
    return;
  }
  log(reportConsoleTimer(m_consoleTimers, name, end));
}

bool WasmLane::instantiate(uint32_t stackBytes)
{
  char error[256] = {};
  auto inst = wasm_runtime_instantiate(
      (wasm_module_t)m_module->module.get(),
      stackBytes,
      0,
      error,
      sizeof(error));
  wasm_exec_env_t env = inst != nullptr
                            ? wasm_runtime_create_exec_env(inst, stackBytes)
                            : nullptr;
  if (env == nullptr)
  {
    if (inst != nullptr)
    {
      wasm_runtime_deinstantiate(inst);
    }
    fail(std::string(laneNoun(m_kind)) + " instantiation failed: " + error);
    return false;
  }
  std::lock_guard<std::mutex> lock(m_envMutex);
  m_instance = inst;
  m_execEnv = env;
  return true;
}

bool WasmLane::reuse()
{
  WasmLaneModule::Instance parked = m_module->unpark();
  std::lock_guard<std::mutex> lock(m_envMutex);
  m_instance = parked.instance;
  m_execEnv = parked.execEnv;
  return m_instance != nullptr;
}

bool WasmLane::runStart(uint32_t* entry, bool reused)
{
  std::vector<uint8_t> input = takeInput();
  uint32_t argv[1] = {};
  // The reset runs the start itself.
  void* start = reused ? lookup(lane_layout::kLaneResetExport, 0, 1)
                       : lookup(lane_layout::kLiftedStartExport, 0, 1, true);
  if (hasFailure() || (start != nullptr && !call(start, 0, argv)))
  {
    return false;
  }
  argv[0] = (uint32_t)input.size();
  if (!call(lane_layout::kLaneAllocExport, 1, argv))
  {
    return false;
  }
  void* at = wasmAppBytes(m_instance, argv[0], (uint32_t)input.size());
  if (at == nullptr)
  {
    fail("lane allocated its input outside its memory");
    return false;
  }
  if (!input.empty())
  {
    memcpy(at, input.data(), input.size());
  }
  entry[0] = argv[0];
  entry[1] = (uint32_t)input.size();
  return true;
}

void* WasmLane::lookup(
    const char* name,
    uint32_t argc,
    uint32_t cells,
    bool optional)
{
  wasm_function_inst_t fn = wasm_runtime_lookup_function(
      (wasm_module_inst_t)m_instance,
      name);
  if (fn == nullptr)
  {
    if (!optional)
    {
      fail(std::string(spec().module) + " has no " + name);
    }
    return nullptr;
  }
  // WAMR writes results over the argument cells.
  if (wasmCallCells(m_instance, fn, cells) != argc)
  {
    fail(
        std::string(spec().module) + " exports " + name +
        " with the wrong signature");
    return nullptr;
  }
  return fn;
}

bool WasmLane::call(void* fn, uint32_t argc, uint32_t* argv)
{
  return wasm_runtime_call_wasm(
      (wasm_exec_env_t)m_execEnv,
      (wasm_function_inst_t)fn,
      argc,
      argv);
}

void WasmLane::failTrap(std::string_view message)
{
  auto inst = (wasm_module_inst_t)m_instance;
  const char* exception = trapText(wasm_runtime_get_exception(inst));
  fail(
      !message.empty()       ? message
      : exception != nullptr ? std::string_view(exception)
                             : std::string_view(spec().trapped));
  wasm_runtime_clear_exception(inst);
}

void WasmLane::interruptCall()
{
  std::lock_guard<std::mutex> lock(m_envMutex);
  if (m_execEnv != nullptr)
  {
    wasm_runtime_rive_interrupt((wasm_exec_env_t)m_execEnv);
  }
}

void WasmLane::interruptAfter(uint32_t ms)
{
  std::lock_guard<std::mutex> lock(m_envMutex);
  // Replacing an armed one would postpone it, or clear an interrupt it fired.
  if (m_execEnv != nullptr && m_interruptBudget == nullptr)
  {
    m_interruptBudget = std::make_unique<WasmCallBudget>(m_execEnv);
    m_interruptBudget->arm(ms);
  }
}

void WasmLane::dropInterrupt()
{
  std::lock_guard<std::mutex> lock(m_envMutex);
  m_interruptBudget = nullptr;
}

WasmLaneModule::Instance WasmLane::detach()
{
  std::lock_guard<std::mutex> lock(m_envMutex);
  m_interruptBudget = nullptr;
  WasmLaneModule::Instance detached{m_instance, m_execEnv};
  m_execEnv = nullptr;
  m_instance = nullptr;
  return detached;
}

void WasmLane::teardown() { WasmLaneModule::destroy(detach()); }

void WasmLaneModule::destroy(Instance instance)
{
  if (instance.execEnv != nullptr)
  {
    wasm_runtime_destroy_exec_env((wasm_exec_env_t)instance.execEnv);
  }
  if (instance.instance != nullptr)
  {
    wasm_runtime_deinstantiate((wasm_module_inst_t)instance.instance);
  }
}

WasmLaneModule::~WasmLaneModule()
{
  for (Instance parked : m_parked)
  {
    destroy(parked);
  }
}

void WasmLaneModule::park(Instance instance)
{
  std::lock_guard<std::mutex> lock(m_parkedMutex);
  m_parked.push_back(instance);
}

WasmLaneModule::Instance WasmLaneModule::unpark()
{
  std::lock_guard<std::mutex> lock(m_parkedMutex);
  if (m_parked.empty())
  {
    return {};
  }
  Instance parked = m_parked.back();
  m_parked.pop_back();
  return parked;
}

static bool checkLaneImports(
    wasm_module_t module,
    LaneKind kind,
    std::string& error)
{
  int32_t count = wasm_runtime_get_import_count(module);
  for (int32_t i = 0; i < count; i++)
  {
    wasm_import_t import;
    wasm_runtime_get_import_type(module, i, &import);
    if (laneImportRefusal((uint32_t)kind, import.module_name, import.name) !=
        nullptr)
    {
      error = lane_layout::laneImportError(import.module_name, import.name);
      return false;
    }
  }
  return true;
}

#endif

rcp<WasmLaneModule> rive::loadLaneModule(
    Span<const uint8_t> mainModule,
    LaneKind kind,
    std::string& error)
{
  const LaneSpec& spec = laneSpec(kind);
  Span<const uint8_t> section = findLaneSection(mainModule, spec.section);
  if (section.empty())
  {
    return nullptr;
  }
  auto lane = make_rcp<WasmLaneModule>();
  lane->entryCount = laneEntryCount(section, spec.prefix);
  if (lane->entryCount == 0)
  {
    error = std::string(spec.module) + ": it exports no entries";
    return nullptr;
  }
  lane->scriptNames = readScriptNames(
      findLaneSection(mainModule, kLaneNameSections[(int)kind]),
      lane->entryCount);
  std::vector<uint8_t> lifted = liftStart(section);
#ifdef __EMSCRIPTEN__
  lane->bytes = std::move(lifted);
  lane->id = laneModuleId(lane->bytes);
  return lane;
#else
  WasmScriptingVM::ExecutionTier tier;
  std::string interpReason;
  std::string loadError;
  lane->module = WasmScriptingVM::loadSharedModule(
      Span<const uint8_t>(lifted.data(), lifted.size()),
      tier,
      interpReason,
      loadError);
  if (lane->module == nullptr)
  {
    error = std::string(spec.module) +
            ": lane module failed to load: " + loadError;
    return nullptr;
  }
  lane->compiled = tier != WasmScriptingVM::ExecutionTier::interp;
  lane->tier = WasmScriptingVM::tierName(tier, interpReason);
  lane->resets = kind == LaneKind::job &&
                 exportsFunction(
                     Span<const uint8_t>(lifted.data(), lifted.size()),
                     lane_layout::kLaneResetExport);
  for (uint32_t i = 0; i < lane->entryCount; i++)
  {
    lane->entryNames.push_back(spec.prefix + std::to_string(i));
  }
  if (!checkLaneImports((wasm_module_t)lane->module.get(), kind, error))
  {
    error = std::string(spec.module) + ": " + error;
    return nullptr;
  }
  return lane;
#endif
}

#endif
