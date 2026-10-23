#ifdef WITH_RIVE_SCRIPTING_WASM

#include "rive/wasm/wasm_shared.hpp"
#include "rive/async/compare_exchange.hpp"
#include "rive/async/timeouts.hpp"
#include "rive/wasm/wasm_lane_policy.hpp"
#include "rive/wasm/wasm_processors.hpp"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif
#ifdef __EMSCRIPTEN_PTHREADS__
#include <emscripten/threading.h>
#endif

#include <algorithm>
#include <climits>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>

using namespace rive;

using Clock = std::chrono::steady_clock;
using namespace rive::shared_layout;

static_assert(kChunkCount * kChunkSlots == 1u << (31 - kIdSlotShift), "");
static_assert(kSlotLive == kIdGenerationMask + 1, "");
static_assert(kSlotPin == kSlotLive << 1, "");
#ifdef __EMSCRIPTEN__
static_assert(offsetof(SharedHeader, kind) == kHeaderKind, "");
static_assert(offsetof(SharedHeader, lock) == kHeaderLock, "");
static_assert(offsetof(SharedHeader, capacity) == kHeaderCapacity, "");
static_assert(offsetof(SharedHeader, closed) == kHeaderClosed, "");
static_assert(sizeof(SharedHeader) == kHeaderSize, "");
static_assert(offsetof(SharedRegion::Block, poisoned) == kRegionPoisoned, "");
static_assert(sizeof(SharedRegion::Block) == kRegionData, "");
static_assert(offsetof(SharedRing::Block, head) == kRingHead, "");
static_assert(offsetof(SharedRing::Block, used) == kRingUsed, "");
static_assert(offsetof(SharedRing::Block, dataSignal) == kRingDataSignal, "");
static_assert(offsetof(SharedRing::Block, spaceSignal) == kRingSpaceSignal, "");
static_assert(sizeof(SharedRing::Block) == kRingData, "");
static_assert(offsetof(SharedSnapshot::Block, latest) == kSnapshotLatest, "");
static_assert(offsetof(SharedSnapshot::Block, seq.low) == kSnapshotSeq, "");
static_assert(
    offsetof(SharedSnapshot::Block, seq.high) == kSnapshotSeqHigh,
    "");
static_assert(
    offsetof(SharedSnapshot::Block, versions) == kSnapshotVersions,
    "");
static_assert(offsetof(SharedSnapshot::Block, sizes) == kSnapshotSizes, "");
static_assert(sizeof(SharedSnapshot::Block) == kSnapshotData, "");
static_assert(
    offsetof(SharedSignal::Block, watcherCount) == kSignalWatcherCount,
    "");
static_assert(
    offsetof(SharedSignal::Block, watcherCapacity) == kSignalWatcherCapacity,
    "");
static_assert(offsetof(SharedSignal::Block, seq.low) == kSignalSeq, "");
static_assert(offsetof(SharedSignal::Block, seq.high) == kSignalSeqHigh, "");
static_assert(offsetof(SharedSignal::Block, watchers) == kSignalWatchers, "");
static_assert(sizeof(SharedHeader) == kBlobData, "");
static_assert(offsetof(SharedTable, lock) == kTableLock, "");
static_assert(offsetof(SharedTable, count) == kTableCount, "");
static_assert(offsetof(SharedTable, freeHead) == kTableFreeHead, "");
static_assert(offsetof(SharedTable, freeTail) == kTableFreeTail, "");
static_assert(offsetof(SharedTable, lanes) == kTableLanes, "");
static_assert(offsetof(SharedTable, tokens) == kTableTokens, "");
static_assert(offsetof(SharedTable, chunks) == kTableChunks, "");
static_assert(offsetof(SharedSlot, state) == kSlotState, "");
static_assert(offsetof(SharedSlot, block) == kSlotBlock, "");
static_assert(offsetof(SharedSlot, owner) == kSlotOwner, "");
static_assert(kSlotOwnerHigh == kSlotOwner + 4, "");
static_assert(sizeof(SharedSlot) == kSlotSize, "");
#endif
static_assert((uint32_t)SharedObject::Kind::region == kKindRegion, "");
static_assert((uint32_t)SharedObject::Kind::ring == kKindRing, "");
static_assert((uint32_t)SharedObject::Kind::snapshot == kKindSnapshot, "");
static_assert((uint32_t)SharedObject::Kind::signal == kKindSignal, "");
static_assert((uint32_t)SharedObject::Kind::blob == kKindBlob, "");

void rive::lockWord(std::atomic<int32_t>& word)
{
  spinUntil([&word]() {
    int32_t expected = 0;
    return compareExchangeWeak(word, expected, 1, std::memory_order_acquire);
  });
}

void rive::unlockWord(std::atomic<int32_t>& word)
{
  word.store(0, std::memory_order_release);
}

namespace
{
thread_local Clock::time_point t_callDeadline = Clock::time_point::max();

struct WordGuard
{
  explicit WordGuard(std::atomic<int32_t>& word) : word(word)
  {
    lockWord(word);
  }
  ~WordGuard() { unlockWord(word); }
  std::atomic<int32_t>& word;
};
} // namespace

Clock::time_point rive::armCallDeadline(Clock::time_point deadline)
{
  Clock::time_point outer = t_callDeadline;
  t_callDeadline = std::min(outer, deadline);
  return outer;
}

void rive::restoreCallDeadline(Clock::time_point outer)
{
  t_callDeadline = outer;
}

Clock::time_point rive::callDeadline() { return t_callDeadline; }

std::chrono::milliseconds rive::waitLimit()
{
#ifdef __EMSCRIPTEN_PTHREADS__
  if (emscripten_is_main_browser_thread())
  {
    return std::chrono::milliseconds(lane_timing::kMainWaitMs);
  }
#endif
  return std::chrono::milliseconds(lane_timing::kLaneWaitMs);
}

// --- wake word ---------------------------------------------------------------

void WakeWord::wake()
{
#ifdef __EMSCRIPTEN__
  static_assert(offsetof(WakeWord, m_word) == kWakeWordWord, "");
#endif
  m_word.fetch_add(1, std::memory_order_acq_rel);
#ifdef __EMSCRIPTEN_PTHREADS__
  emscripten_futex_wake(&m_word, INT_MAX);
#elif !defined(__EMSCRIPTEN__)
  // The waiter checks the word under the mutex, so taking it here, after
  // the bump, means it either sees the bump or is already waiting.
  lockSpinning(m_mutex);
  m_mutex.unlock();
  m_cv.notify_all();
#endif
}

void WakeWord::wait(uint32_t seen, Clock::time_point deadline)
{
#ifdef __EMSCRIPTEN_PTHREADS__
  double ms = INFINITY;
  if (deadline != Clock::time_point::max())
  {
    ms = std::max(
        0.0,
        std::chrono::duration<double, std::milli>(deadline - Clock::now())
            .count());
  }
  emscripten_futex_wait(&m_word, seen, ms);
#elif !defined(__EMSCRIPTEN__)
  std::unique_lock<std::mutex> lock(m_mutex);
  auto moved = [this, seen]() { return value() != seen; };
  if (deadline == Clock::time_point::max())
  {
    m_cv.wait(lock, moved);
  }
  else
  {
    m_cv.wait_until(lock, deadline, moved);
  }
#else
  // Without threads nothing waits: a cooperative worker steps instead.
  (void)seen;
  (void)deadline;
#endif
}

// --- objects -----------------------------------------------------------------

SharedObject::SharedObject(
    Kind kind,
    size_t blockSize,
    uint32_t capacity,
    size_t dataSize)
    : m_block((SharedHeader*)calloc(1, blockSize + dataSize))
    , m_blockSize(blockSize)
{
  if (m_block != nullptr)
  {
    m_block->kind = (uint32_t)kind;
    m_block->capacity = capacity;
  }
}

SharedObject::~SharedObject() { free(m_block); }

// --- signal ------------------------------------------------------------------

SharedSignal::SharedSignal() : SharedObject(Kind::signal, sizeof(Block), 0, 0)
{}

SharedSignal::~SharedSignal()
{
  if (made())
  {
    free(signal()->watchers);
  }
  free(m_watchCounts);
}

uint64_t SharedSignal::seq() const { return signal()->seq.load(); }

void SharedSignal::notify()
{
  Block* block = signal();
  block->seq.bump();
  WordGuard guard(block->header.lock);
  for (uint32_t i = 0; i < block->watcherCount; i++)
  {
    block->watchers[i]->wake();
  }
}

void SharedSignal::takeHolds(std::vector<uint32_t>&) { notify(); }

void SharedSignal::addWatcher(WakeWord* watcher)
{
  Block* block = signal();
  WordGuard guard(block->header.lock);
  for (uint32_t i = 0; i < block->watcherCount; i++)
  {
    if (block->watchers[i] == watcher)
    {
      m_watchCounts[i]++;
      return;
    }
  }
  if (block->watcherCount == block->watcherCapacity)
  {
    // Without memory it stays unwatched, and its waiter sees the notify
    // at its next wake.
    uint32_t capacity = std::max(4u, block->watcherCapacity * 2);
    auto* watchers = (WakeWord**)realloc(
        block->watchers,
        capacity * sizeof(WakeWord*));
    if (watchers == nullptr)
    {
      return;
    }
    block->watchers = watchers;
    auto* counts = (uint32_t*)realloc(
        m_watchCounts,
        capacity * sizeof(uint32_t));
    if (counts == nullptr)
    {
      return;
    }
    m_watchCounts = counts;
    block->watcherCapacity = capacity;
  }
  m_watchCounts[block->watcherCount] = 1;
  block->watchers[block->watcherCount++] = watcher;
}

void SharedSignal::removeWatcher(WakeWord* watcher)
{
  Block* block = signal();
  WordGuard guard(block->header.lock);
  for (uint32_t i = 0; i < block->watcherCount; i++)
  {
    if (block->watchers[i] == watcher)
    {
      if (--m_watchCounts[i] == 0)
      {
        uint32_t last = --block->watcherCount;
        block->watchers[i] = block->watchers[last];
        m_watchCounts[i] = m_watchCounts[last];
      }
      return;
    }
  }
}

// --- region ------------------------------------------------------------------

SharedRegion::SharedRegion(uint32_t size)
    : SharedObject(Kind::region, sizeof(Block), size, size)
{}

#if defined(WITH_RIVE_TOOLS) || defined(TESTING)
static std::atomic<uint32_t> s_wakeDelayMs{0};

void SharedRegion::delayWakes(uint32_t ms) { s_wakeDelayMs = ms; }
#endif

bool SharedRegion::lock(double timeoutSeconds)
{
  Clock::time_point deadline = std::min(
      Clock::now() + waitLimit(),
      t_callDeadline);
  if (!unboundedSeconds(timeoutSeconds))
  {
    deadline = std::min(deadline, deadlineAfter(timeoutSeconds));
  }
#if defined(WITH_RIVE_TOOLS) || defined(TESTING)
  if (s_wakeDelayMs != 0 && deadline > Clock::now())
  {
    deadline += std::chrono::milliseconds(s_wakeDelayMs.load());
  }
#endif
#if RIVE_ONE_THREAD()
  // Nothing can unlock it while the only thread waits, so a held region
  // times out at the deadline as the timed wait would.
  if (m_locked)
  {
    while (Clock::now() < deadline)
    {
    }
    return false;
  }
  m_locked = true;
  return true;
#elif defined(_GLIBCXX_TSAN)
  // libstdc++ 14 before 14.4 and 15 before 15.3 cannot compile a steady clock
  // deadline under TSan.
  return m_lock.try_lock_for(deadline - Clock::now());
#else
  return m_lock.try_lock_until(deadline);
#endif
}

void SharedRegion::unlock()
{
#if RIVE_ONE_THREAD()
  m_locked = false;
#else
  m_lock.unlock();
#endif
}

std::atomic<uint32_t>* SharedRegion::word(uint32_t offset)
{
  if ((offset & 3) != 0 || (uint64_t)offset + 4 > size())
  {
    return nullptr;
  }
  return reinterpret_cast<std::atomic<uint32_t>*>(bytes() + offset);
}

bool SharedRegion::poisoned() const
{
  return ((Block*)block())->poisoned.load() != 0;
}

void SharedRegion::poison() { ((Block*)block())->poisoned.store(1); }

// --- ring --------------------------------------------------------------------

SharedRing::SharedRing(uint32_t capacity)
    : SharedObject(Kind::ring, sizeof(Block), capacity, capacity)
    , m_data(make_rcp<SharedSignal>())
    , m_space(make_rcp<SharedSignal>())
{}

bool SharedRing::made() const
{
  return SharedObject::made() && m_data->made() && m_space->made();
}

void SharedRing::setSignalIds(uint32_t data, uint32_t space)
{
  ring()->dataSignal = data;
  ring()->spaceSignal = space;
}

void SharedRing::copyIn(const uint8_t* from, uint32_t size)
{
  Block* block = ring();
  uint32_t capacity = block->header.capacity;
  uint32_t at = (block->head + block->used) % capacity;
  uint32_t first = std::min(size, capacity - at);
  memcpy(data() + at, from, first);
  memcpy(data(), from + first, size - first);
  block->used += size;
}

void SharedRing::copyOut(uint32_t at, uint8_t* to, uint32_t size) const
{
  uint32_t capacity = ring()->header.capacity;
  at %= capacity;
  uint32_t first = std::min(size, capacity - at);
  memcpy(to, data() + at, first);
  memcpy(to + first, data(), size - first);
}

namespace
{
// The holds after a value, and their count.
uint64_t holdBytes(size_t count) { return count == 0 ? 0 : 4 * count + 4; }

uint32_t sizeWord(size_t size, size_t holdCount)
{
  return (uint32_t)(size + holdBytes(holdCount)) |
         (holdCount != 0 ? kRecordHolds : 0);
}

// A publisher may overwrite the copy meanwhile and the version check then
// discards it, so tsan_suppressions.txt names this race.
void copyTorn(void* to, const void* from, size_t size)
{
  memcpy(to, from, size);
}
} // namespace

uint64_t SharedRing::recordBytes(size_t size, size_t holdCount)
{
  return 4 + size + holdBytes(holdCount);
}

bool SharedRing::write(Span<const uint8_t> record, Span<const uint32_t> holds)
{
  uint32_t size = sizeWord(record.size(), holds.size());
  uint32_t count = (uint32_t)holds.size();
  Block* block = ring();
  {
    WordGuard guard(block->header.lock);
    if (block->header.closed != 0 ||
        block->used + recordBytes(record.size(), holds.size()) >
            block->header.capacity)
    {
      return false;
    }
    copyIn((const uint8_t*)&size, 4);
    copyIn(record.data(), (uint32_t)record.size());
    if (count != 0)
    {
      copyIn((const uint8_t*)holds.data(), count * 4);
      copyIn((const uint8_t*)&count, 4);
    }
  }
  m_data->notify();
  return true;
}

void SharedRing::peekRecord(uint32_t at, uint32_t& bytes, uint32_t& count) const
{
  uint32_t word;
  copyOut(at, (uint8_t*)&word, 4);
  bytes = word & ~kRecordHolds;
  count = 0;
  if ((word & kRecordHolds) != 0)
  {
    copyOut(at + bytes, (uint8_t*)&count, 4);
  }
}

void SharedRing::copyHolds(
    uint32_t at,
    uint32_t bytes,
    uint32_t count,
    std::vector<uint32_t>& out) const
{
  for (uint32_t i = 0; i < count; i++)
  {
    uint32_t id;
    copyOut(at + bytes - (count - i) * 4, (uint8_t*)&id, 4);
    out.push_back(id);
  }
}

template <typename Take>
uint32_t SharedRing::takeHead(
    std::vector<uint32_t>* holds,
    bool& popped,
    Take&& take)
{
  popped = false;
  uint32_t size;
  Block* block = ring();
  {
    WordGuard guard(block->header.lock);
    if (block->used == 0)
    {
      return 0;
    }
    uint32_t bytes;
    uint32_t count;
    peekRecord(block->head, bytes, count);
    if (holds == nullptr && count != 0)
    {
      return kCarriesHolds;
    }
    size = bytes - (uint32_t)holdBytes(count);
    if (!take(size))
    {
      return size + 1;
    }
    pop(bytes, count, holds);
  }
  popped = true;
  m_space->notify();
  return size + 1;
}

uint32_t SharedRing::read(
    uint8_t* out,
    uint32_t capacity,
    std::vector<uint32_t>* holds)
{
  bool popped;
  return takeHead(holds, popped, [&](uint32_t size) {
    if (out == nullptr || size > capacity)
    {
      return false;
    }
    copyOut(ring()->head + 4, out, size);
    return true;
  });
}

void SharedRing::pop(
    uint32_t bytes,
    uint32_t count,
    std::vector<uint32_t>* holds)
{
  Block* block = ring();
  if (holds != nullptr)
  {
    copyHolds(block->head, bytes, count, *holds);
  }
  block->head = (block->head + 4 + bytes) % block->header.capacity;
  block->used -= 4 + bytes;
}

uint32_t SharedRing::drop(uint32_t capacity, std::vector<uint32_t>* holds)
{
  bool popped;
  uint32_t answer = takeHead(holds, popped, [capacity](uint32_t size) {
    return size > capacity;
  });
  return popped || answer == kCarriesHolds ? answer : 0;
}

void SharedRing::takeHolds(std::vector<uint32_t>& out)
{
  out.push_back(dataSignalId());
  out.push_back(spaceSignalId());
  Block* block = ring();
  {
    WordGuard guard(block->header.lock);
    for (uint32_t at = 0; at < block->used;)
    {
      uint32_t bytes;
      uint32_t count;
      peekRecord(block->head + at, bytes, count);
      copyHolds(block->head + at, bytes, count, out);
      at += 4 + bytes;
    }
    // A call still holding the ring must not hand these holds out again,
    // nor write ones nobody would give back.
    block->used = 0;
    block->header.closed = 1;
  }
  // A handle may keep a signal past the ring, and its waits must wake.
  m_data->notify();
  m_space->notify();
}

uint32_t SharedRing::used()
{
  WordGuard guard(ring()->header.lock);
  return ring()->used;
}

// --- snapshot ----------------------------------------------------------------

SharedSnapshot::SharedSnapshot(uint32_t capacity)
    : SharedObject(
          Kind::snapshot,
          sizeof(Block),
          capacity,
          (size_t)capacity * kCopies)
{}

uint64_t SharedSnapshot::seq() const { return snapshot()->seq.load(); }

uint32_t SharedSnapshot::valueSize(
    uint32_t index,
    std::vector<uint32_t>* holds,
    bool torn) const
{
  const Block* block = snapshot();
  uint32_t word = block->sizes[index].load(std::memory_order_relaxed);
  uint32_t bytes = std::min(word & ~kRecordHolds, block->header.capacity);
  // Locked reads skip copyTorn, so tsan still checks them.
  auto load = [&](uint32_t at) {
    uint32_t value;
    if (torn)
    {
      copyTorn(&value, copy(index) + at, 4);
    }
    else
    {
      memcpy(&value, copy(index) + at, 4);
    }
    return value;
  };
  uint32_t count = 0;
  if ((word & kRecordHolds) != 0 && bytes >= 4)
  {
    count = std::min(load(bytes - 4), (bytes - 4) / 4);
  }
  uint32_t size = bytes - (uint32_t)holdBytes(count);
  for (uint32_t i = 0; holds != nullptr && i < count; i++)
  {
    holds->push_back(load(size + i * 4));
  }
  return size;
}

bool SharedSnapshot::carriesHolds(uint32_t index) const
{
  return (snapshot()->sizes[index].load(std::memory_order_relaxed) &
          kRecordHolds) != 0;
}

bool SharedSnapshot::publish(
    Span<const uint8_t> value,
    Span<const uint32_t> holds,
    std::vector<uint32_t>* replaced)
{
  Block* block = snapshot();
  uint32_t count = (uint32_t)holds.size();
  if (value.size() + holdBytes(count) > block->header.capacity)
  {
    return false;
  }
  // Publishers take turns; readers never take the lock.
  WordGuard guard(block->header.lock);
  uint32_t latest = block->latest.load(std::memory_order_relaxed);
  if (block->header.closed != 0 ||
      (latest != 0 && replaced == nullptr && carriesHolds(latest - 1)))
  {
    return false;
  }
  if (latest != 0 && replaced != nullptr)
  {
    valueSize(latest - 1, replaced, false);
  }
  uint32_t index = latest % kCopies;
  // A publisher that died mid write left the version odd.
  uint32_t version = block->versions[index].load(std::memory_order_relaxed) | 1;
  block->versions[index].store(version, std::memory_order_relaxed);
  // Orders the odd version before the bytes, as a reader checks them.
  std::atomic_thread_fence(std::memory_order_release);
  uint8_t* to = copy(index);
  memcpy(to, value.data(), value.size());
  if (count != 0)
  {
    memcpy(to + value.size(), holds.data(), count * 4);
    memcpy(to + value.size() + count * 4, &count, 4);
  }
  block->sizes[index].store(
      sizeWord(value.size(), count),
      std::memory_order_relaxed);
  block->versions[index].store(version + 1, std::memory_order_release);
  block->latest.store(index + 1, std::memory_order_release);
  block->seq.bump();
  return true;
}

uint32_t SharedSnapshot::read(
    uint8_t* out,
    uint32_t capacity,
    std::vector<uint32_t>* holds)
{
  Block* block = snapshot();
  size_t kept = holds != nullptr ? holds->size() : 0;
  // Retries only when publishes overtook the copy.
  for (;;)
  {
    uint32_t latest = block->latest.load(std::memory_order_acquire);
    if (latest == 0)
    {
      return 0;
    }
    uint32_t index = latest - 1;
    uint32_t version = block->versions[index].load(std::memory_order_acquire);
    if ((version & 1) != 0)
    {
      continue;
    }
    uint32_t size = valueSize(index, nullptr, true);
    bool refused = holds == nullptr && carriesHolds(index);
    bool fits = !refused && out != nullptr && size <= capacity;
    if (fits && size != 0)
    {
      copyTorn(out, copy(index), size);
    }
    if (fits && holds != nullptr)
    {
      valueSize(index, holds, true);
    }
    std::atomic_thread_fence(std::memory_order_acquire);
    if (block->versions[index].load(std::memory_order_relaxed) == version)
    {
      return refused ? kCarriesHolds : size + 1;
    }
    if (holds != nullptr)
    {
      holds->resize(kept);
    }
  }
}

void SharedSnapshot::takeHolds(std::vector<uint32_t>& out)
{
  Block* block = snapshot();
  WordGuard guard(block->header.lock);
  uint32_t latest = block->latest.load(std::memory_order_relaxed);
  if (latest != 0)
  {
    valueSize(latest - 1, &out, false);
  }
  // A publish still holding the snapshot must not give them back again,
  // nor publish ones nobody would give back.
  block->latest.store(0, std::memory_order_release);
  block->header.closed = 1;
}

// --- blob --------------------------------------------------------------------

SharedBlob::SharedBlob(Span<const uint8_t> bytes)
    : SharedObject(
          Kind::blob,
          sizeof(SharedHeader),
          (uint32_t)bytes.size(),
          bytes.size())
{
  if (made() && !bytes.empty())
  {
    memcpy(data(), bytes.data(), bytes.size());
  }
}

uint32_t SharedBlob::read(uint32_t offset, uint8_t* out, uint32_t capacity)
    const
{
  if (offset >= size())
  {
    return 0;
  }
  uint32_t count = std::min(capacity, size() - offset);
  memcpy(out, data() + offset, count);
  return count;
}

// --- registry ----------------------------------------------------------------

namespace
{
constexpr uint32_t kNoSlot = UINT32_MAX;

SharedTable& sharedTable()
{
  // Never destroyed, so an object released during static teardown finds
  // it, and never allocated, so it cannot fail.
  static SharedTable table;
  static SharedSlot firstChunk[kChunkSlots];
  static bool ready = []() {
    table.freeHead = kNoSlot;
    table.freeTail = kNoSlot;
    table.chunks[0].store(firstChunk);
    return true;
  }();
  (void)ready;
  return table;
}

struct OwnerBudget
{
  uint64_t owner;
  uint64_t charged;
  uint64_t limit;
  uint32_t objects;
  OwnerBudget* next;
};

LaneMutex& budgetMutex()
{
  static LaneMutex mutex;
  return mutex;
}

// One per live VM, few enough to scan; never destroyed, as a release in
// static teardown refunds.
OwnerBudget* s_ownerBudgets = nullptr;

// The link holding owner's budget, null once its VM went; under budgetMutex.
OwnerBudget*& budgetOf(uint64_t owner)
{
  OwnerBudget** at = &s_ownerBudgets;
  while (*at != nullptr && (*at)->owner != owner)
  {
    at = &(*at)->next;
  }
  return *at;
}

// Counts one more object against owner's cap; false when it has them all or
// its VM went, though a lane of it may still run.
bool countObject(uint64_t owner)
{
  std::lock_guard<LaneMutex> lock(budgetMutex());
  OwnerBudget* budget = budgetOf(owner);
  if (budget == nullptr || budget->objects == kSharedMaxObjects)
  {
    return false;
  }
  budget->objects++;
  return true;
}

void giveBack(uint64_t owner, uint64_t bytes, uint32_t objects)
{
  std::lock_guard<LaneMutex> lock(budgetMutex());
  if (OwnerBudget* budget = budgetOf(owner))
  {
    budget->charged -= std::min(budget->charged, bytes);
    budget->objects -= std::min(budget->objects, objects);
  }
}

thread_local uint32_t t_heldRegion = 0;
thread_local SharedRegion* t_heldObject = nullptr;

uint32_t indexOf(uint32_t id) { return (id & ~kIdBit) >> kIdSlotShift; }

uint32_t idOf(uint32_t index, uint32_t state)
{
  return kIdBit | (index << kIdSlotShift) | (state & kIdGenerationMask);
}

bool names(uint32_t state, uint32_t id)
{
  return (state & kSlotLive) != 0 && ((state ^ id) & kIdGenerationMask) == 0;
}

SharedSlot* slotAt(SharedTable& table, uint32_t index)
{
  SharedSlot* chunk = table.chunks[index >> kChunkShift].load(
      std::memory_order_acquire);
  return chunk != nullptr ? chunk + (index & (kChunkSlots - 1)) : nullptr;
}

SharedSlot* slotOf(SharedTable& table, uint32_t id)
{
  return (id & kIdBit) != 0 ? slotAt(table, indexOf(id)) : nullptr;
}

bool pin(SharedSlot& slot, uint32_t id)
{
  uint32_t state = slot.state.load(std::memory_order_relaxed);
  do
  {
    if (!names(state, id))
    {
      return false;
    }
  } while (!compareExchangeWeak(
      slot.state,
      state,
      state + kSlotPin,
      std::memory_order_acquire,
      std::memory_order_relaxed));
  return true;
}

bool held(const SharedTable& table, const SharedSlot& slot)
{
  if (slot.state.load(std::memory_order_acquire) >= kSlotPin)
  {
    return true;
  }
  // Sequentially consistent against the release's retire: a lane either
  // sees the slot retired or is seen here.
  for (const auto& lane : table.lanes)
  {
    if (lane.load(std::memory_order_seq_cst) == (uintptr_t)&slot)
    {
      return true;
    }
  }
  return false;
}

// Waits out the slot's lookups outside the lock, then frees it for reuse,
// appending the holds its object carried for the caller to give back.
void retire(
    SharedTable& table,
    SharedSlot& slot,
    uint32_t index,
    std::vector<uint32_t>& holds)
{
  spinUntil([&]() { return !held(table, slot); });
  SharedObject* object = slot.object.exchange(
      nullptr,
      std::memory_order_relaxed);
  uint64_t owner = slot.owner.load(std::memory_order_relaxed);
  slot.block.store(nullptr, std::memory_order_relaxed);
  slot.owner.store(0, std::memory_order_relaxed);
  {
    // Oldest first, so a stale id only aliases a new object after its
    // slot went round every free slot 4096 times.
    WordGuard guard(table.lock);
    slot.nextFree = kNoSlot;
    if (table.freeTail == kNoSlot)
    {
      table.freeHead = index;
    }
    else
    {
      slotAt(table, table.freeTail)->nextFree = index;
    }
    table.freeTail = index;
  }
  object->takeHolds(holds);
  giveBack(owner, object->charge, 1);
  object->unref();
}

// Gives back a hold, or every hold when forced, answering whether that was
// the last, so the caller retires the slot it now took out of use.
bool dropHold(SharedSlot& slot, uint32_t id, uint64_t owner, bool force)
{
  uint32_t state = slot.state.load(std::memory_order_relaxed);
  if (!names(state, id) || slot.owner.load(std::memory_order_relaxed) != owner)
  {
    return false;
  }
  SharedObject* object = slot.object.load(std::memory_order_relaxed);
  if (!force && --object->holds != 0)
  {
    return false;
  }
  // Lookups still move the pins; only the lock holder moves the rest.
  uint32_t retired;
  do
  {
    retired = (state & ~(kSlotLive | kIdGenerationMask)) |
              ((state + 1) & kIdGenerationMask);
  } while (!compareExchangeWeak(
      slot.state,
      state,
      retired,
      std::memory_order_seq_cst,
      std::memory_order_relaxed));
  return true;
}

// A retired object's holds go in turn rather than by recursion, which a long
// chain of objects holding the next would overflow the stack with.
void releaseSlot(uint32_t id, uint64_t owner, bool force)
{
  SharedTable& table = sharedTable();
  std::vector<uint32_t> holds;
  for (;;)
  {
    SharedSlot* slot = slotOf(table, id);
    bool last = false;
    if (slot != nullptr)
    {
      WordGuard guard(table.lock);
      last = dropHold(*slot, id, owner, force);
    }
    if (last)
    {
      retire(table, *slot, indexOf(id), holds);
    }
    if (holds.empty())
    {
      return;
    }
    id = holds.back();
    holds.pop_back();
    force = false;
  }
}
} // namespace

SharedTable* SharedRegistry::table() { return &sharedTable(); }

#if defined(__EMSCRIPTEN__) && RIVE_PROCESSOR_AUDIO()
// For a lane without librive, which reaches shared objects through it.
extern "C" EMSCRIPTEN_KEEPALIVE uint32_t rive_shared_table()
{
  return (uint32_t)(uintptr_t)SharedRegistry::table();
}
#endif

uint32_t SharedRegistry::add(rcp<SharedObject> object, uint64_t owner)
{
  if (!countObject(owner))
  {
    return kCreateTooMany;
  }
  SharedTable& table = sharedTable();
  uint32_t index;
  for (;;)
  {
    uint32_t next;
    {
      WordGuard guard(table.lock);
      index = table.freeHead;
      if (index != kNoSlot)
      {
        table.freeHead = slotAt(table, index)->nextFree;
        if (table.freeHead == kNoSlot)
        {
          table.freeTail = kNoSlot;
        }
        break;
      }
      index = table.count.load(std::memory_order_relaxed);
      if (index == kChunkCount * kChunkSlots)
      {
        index = kNoSlot;
        break;
      }
      next = index >> kChunkShift;
      if (table.chunks[next].load(std::memory_order_relaxed) != nullptr)
      {
        table.count.store(index + 1, std::memory_order_release);
        break;
      }
    }
    // Made outside the lock before its first slot is taken, so a taken
    // slot always has one; a racing add that made one too frees its own.
    auto* made = (SharedSlot*)calloc(kChunkSlots, sizeof(SharedSlot));
    if (made == nullptr)
    {
      index = kNoSlot;
      break;
    }
    SharedSlot* expected = nullptr;
    if (!compareExchangeStrong(
            table.chunks[next],
            expected,
            made,
            std::memory_order_acq_rel))
    {
      free(made);
    }
  }
  if (index == kNoSlot)
  {
    giveBack(owner, 0, 1);
    return kCreateFull;
  }
  SharedSlot* slot = slotAt(table, index);
  slot->owner.store(owner, std::memory_order_relaxed);
  slot->block.store(object->block(), std::memory_order_relaxed);
  slot->object.store(object.release(), std::memory_order_relaxed);
  // Nothing pins a slot that is not live, so its state is the generation.
  uint32_t state = slot->state.load(std::memory_order_relaxed);
  slot->state.store(state | kSlotLive, std::memory_order_release);
  return idOf(index, state);
}

rcp<SharedObject> SharedRegistry::find(
    uint32_t id,
    SharedObject::Kind kind,
    uint64_t owner)
{
  SharedSlot* slot = slotOf(sharedTable(), id);
  if (slot == nullptr || !pin(*slot, id))
  {
    return nullptr;
  }
  SharedObject* object = slot->object.load(std::memory_order_relaxed);
  rcp<SharedObject> found = object->kind() == kind &&
                                    slot->owner.load(
                                        std::memory_order_relaxed) == owner
                                ? ref_rcp(object)
                                : nullptr;
  slot->state.fetch_sub(kSlotPin, std::memory_order_release);
  return found;
}

bool SharedRegistry::retain(uint32_t id, uint64_t owner)
{
  SharedTable& table = sharedTable();
  SharedSlot* slot = slotOf(table, id);
  if (slot == nullptr)
  {
    return false;
  }
  WordGuard guard(table.lock);
  if (!names(slot->state.load(std::memory_order_relaxed), id) ||
      slot->owner.load(std::memory_order_relaxed) != owner)
  {
    return false;
  }
  slot->object.load(std::memory_order_relaxed)->holds++;
  return true;
}

void SharedRegistry::release(uint32_t id, uint64_t owner)
{
  releaseSlot(id, owner, false);
}

void SharedRegistry::releaseOwner(uint64_t owner)
{
  // First, so a lane still running creates no more.
  {
    std::lock_guard<LaneMutex> lock(budgetMutex());
    OwnerBudget*& link = budgetOf(owner);
    if (OwnerBudget* gone = link)
    {
      link = gone->next;
      delete gone;
    }
  }
  SharedTable& table = sharedTable();
  uint32_t count = table.count.load(std::memory_order_acquire);
  for (uint32_t index = 0; index < count; index++)
  {
    SharedSlot* slot = slotAt(table, index);
    if (slot == nullptr)
    {
      continue;
    }
    uint32_t state = slot->state.load(std::memory_order_acquire);
    if ((state & kSlotLive) != 0 &&
        slot->owner.load(std::memory_order_relaxed) == owner)
    {
      releaseSlot(idOf(index, state), owner, true);
    }
  }
}

uint32_t SharedRegistry::ringOf(uint32_t signal, uint64_t owner, bool& space)
{
  SharedTable& table = sharedTable();
  uint32_t count = table.count.load(std::memory_order_acquire);
  for (uint32_t index = 0; index < count; index++)
  {
    SharedSlot* slot = slotAt(table, index);
    if (slot == nullptr)
    {
      continue;
    }
    uint32_t id = idOf(index, slot->state.load(std::memory_order_acquire));
    rcp<SharedRing> ring = find<SharedRing>(id, owner);
    if (ring != nullptr &&
        (ring->dataSignalId() == signal || ring->spaceSignalId() == signal))
    {
      space = ring->spaceSignalId() == signal;
      return id;
    }
  }
  return 0;
}

bool SharedRegistry::charge(uint64_t owner, uint64_t bytes)
{
  if (bytes == 0)
  {
    return true;
  }
  std::lock_guard<LaneMutex> lock(budgetMutex());
  OwnerBudget* budget = budgetOf(owner);
  if (budget == nullptr || bytes > budget->limit - budget->charged)
  {
    return false;
  }
  budget->charged += bytes;
  return true;
}

void SharedRegistry::refund(uint64_t owner, uint64_t bytes)
{
  giveBack(owner, bytes, 0);
}

uint64_t SharedRegistry::nextOwner(uint64_t byteBudget)
{
  static std::atomic<uint64_t> next{1};
  uint64_t owner = next.fetch_add(1);
  std::lock_guard<LaneMutex> lock(budgetMutex());
  s_ownerBudgets = new OwnerBudget{owner, 0, byteBudget, 0, s_ownerBudgets};
  return owner;
}

uint32_t rive::sharedHeldRegion() { return t_heldRegion; }

SharedRegion* rive::sharedHeld(uint32_t id)
{
  return id != 0 && id == t_heldRegion ? t_heldObject : nullptr;
}

void rive::setSharedHeldRegion(uint32_t id, rcp<SharedRegion> region)
{
  // The held reference is manual so a thread_local needs no destructor.
  safe_unref(t_heldObject);
  t_heldRegion = id;
  t_heldObject = region.release();
}

void rive::releaseTrappedRegion()
{
  SharedRegion* region = t_heldObject;
  if (region == nullptr)
  {
    return;
  }
  // The trap may have stopped mid update, so the bytes are suspect.
  region->poison();
  region->unlock();
  setSharedHeldRegion(0, nullptr);
}

SharedRegion* rive::stashHeldRegion(uint32_t* id)
{
  SharedRegion* region = t_heldObject;
  *id = t_heldRegion;
  t_heldRegion = 0;
  t_heldObject = nullptr;
  return region;
}

void rive::restoreHeldRegion(uint32_t id, SharedRegion* region)
{
  releaseTrappedRegion();
  t_heldRegion = id;
  t_heldObject = region;
}

#endif
