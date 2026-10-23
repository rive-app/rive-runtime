#ifndef _RIVE_WASM_SHARED_HPP_
#define _RIVE_WASM_SHARED_HPP_

#ifdef WITH_RIVE_SCRIPTING_WASM

#include "rive/async/threading.hpp"
#include "rive/refcnt.hpp"
#include "rive/span.hpp"
#include "rive/wasm/wasm_lane_policy.hpp"
#include "rive/wasm/wasm_shared_layout_gen.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>

namespace rive
{

/// Spins until done, yielding after a while, but never sleeping: an audio
/// worklet may not wait on a futex at all.
template <typename Done> void spinUntil(Done done)
{
  for (uint32_t spins = 0; !done(); spins++)
  {
    if (spins > 64)
    {
      std::this_thread::yield();
    }
  }
}

/// Takes a lock word any lane may hold briefly, the audio thread included,
/// as 1 from librive or as its own token from a lane without it.
void lockWord(std::atomic<int32_t>& word);
void unlockWord(std::atomic<int32_t>& word);

/// Takes a mutex without sleeping in it, for the audio thread.
template <typename Mutex> void lockSpinning(Mutex& mutex)
{
  spinUntil([&mutex]() { return mutex.try_lock(); });
}

/// Guards what every lane's thread reaches, which nothing contends for
/// without threads.
#if RIVE_ONE_THREAD()
struct LaneMutex
{
  void lock() {}
  void unlock() {}
};
#else
using LaneMutex = std::mutex;
#endif

/// When this thread's budgeted call runs out, so a region lock gives up
/// rather than block a thread the watchdog cannot interrupt.
std::chrono::steady_clock::time_point armCallDeadline(
    std::chrono::steady_clock::time_point deadline);
void restoreCallDeadline(std::chrono::steady_clock::time_point outer);
std::chrono::steady_clock::time_point callDeadline();
/// The longest a blocking wait on this thread lasts, whatever its timeout.
std::chrono::milliseconds waitLimit();

/// A word a waiter parks on until it changes, bumped by every signal a
/// worker watches; on the web a futex word lanes without librive bump.
class WakeWord
{
public:
  uint32_t value() const
  {
    return (uint32_t)m_word.load(std::memory_order_acquire);
  }
  /// Never sleeps, so the audio thread may wake a worker.
  void wake();
  /// Returns once the word moved off seen or the deadline passed.
  void wait(uint32_t seen, std::chrono::steady_clock::time_point deadline);

private:
  std::atomic<int32_t> m_word{0};
#ifndef __EMSCRIPTEN__
  std::mutex m_mutex;
  std::condition_variable m_cv;
#endif
};

/// The words a shared object keeps its state in, for lanes reaching it
/// through Atomics; idl/shared_layout.py fixes the offsets on wasm32.
struct SharedHeader
{
  uint32_t kind;
  std::atomic<int32_t> lock;
  /// Bytes after the kind's block.
  uint32_t capacity;
  /// Set under the lock once it retired, so no write lands holds in it.
  uint32_t closed;
};

/// Memory every lane reaches by id: each script instance has its own linear
/// memory, so what lanes share lives here, in one block per object.
class SharedObject : public RefCnt<SharedObject>
{
public:
  enum class Kind : uint8_t
  {
    region,
    ring,
    snapshot,
    signal,
    blob,
  };

  /// What a read answers a reader that takes no holds, for a value that
  /// carries some.
  static constexpr uint32_t kCarriesHolds = 0xffffffff;

  virtual ~SharedObject();
  /// False when its memory could not be had; nothing else may be called.
  virtual bool made() const { return m_block != nullptr; }
  Kind kind() const { return (Kind)m_block->kind; }
  SharedHeader* block() const { return m_block; }
  /// True only the first time, so a misuse warns once per object.
  bool firstWarning() { return !m_warned.exchange(true); }
  /// Appends the ids it holds through what it carries, given back when it
  /// goes, and forgets them.
  virtual void takeHolds(std::vector<uint32_t>& out) {}
  /// Who holds its registry slot: its creator, main lane handles, lanes
  /// started with it. Changed only under the registry's lock.
  uint32_t holds = 1;
  /// What it costs its owner's shared memory budget, refunded when it goes.
  uint64_t charge = 0;

protected:
  /// dataSize bytes follow the kind's block of blockSize.
  SharedObject(Kind kind, size_t blockSize, uint32_t capacity, size_t dataSize);
  uint8_t* data() const { return (uint8_t*)m_block + m_blockSize; }

private:
  SharedHeader* m_block;
  size_t m_blockSize;
  std::atomic<bool> m_warned{false};
};

/// A count in a low and a high word, which a lane without librive bumps
/// through 32 bit Atomics, as a 64 bit one allocates in JS.
struct SharedSeq
{
  std::atomic<uint32_t> low;
  std::atomic<uint32_t> high;

  void bump()
  {
    if (low.fetch_add(1, std::memory_order_acq_rel) == UINT32_MAX)
    {
      high.fetch_add(1, std::memory_order_acq_rel);
    }
  }
  /// High first, so a read during a carry answers less, never more.
  uint64_t load() const
  {
    uint64_t top = high.load(std::memory_order_acquire);
    return top << 32 | low.load(std::memory_order_acquire);
  }
};

/// An eventcount: a waiter that read the sequence before checking its
/// condition never misses the notify, which also bumps each watcher's word.
class SharedSignal : public SharedObject
{
public:
  struct Block
  {
    SharedHeader header;
    uint32_t watcherCount;
    uint32_t watcherCapacity;
    SharedSeq seq;
    WakeWord** watchers;
  };

  static constexpr Kind kKind = Kind::signal;
  SharedSignal();
  ~SharedSignal() override;
  void notify();
  /// Watchers come once however many waits share them, so a notify wakes
  /// each word once.
  void addWatcher(WakeWord* watcher);
  void removeWatcher(WakeWord* watcher);
  uint64_t seq() const;
  /// Wakes its watchers, whose waits then find it gone.
  void takeHolds(std::vector<uint32_t>& out) override;

private:
  Block* signal() const { return (Block*)block(); }
  // How many waits share each watcher, beside the block lanes read.
  uint32_t* m_watchCounts = nullptr;
};

/// Bytes behind one lock. A lane holds at most one region at a time, so lock
/// order can never cycle. Its words also serve lock free atomics.
class SharedRegion : public SharedObject
{
public:
  struct Block
  {
    SharedHeader header;
    std::atomic<uint32_t> poisoned;
    uint32_t reserved[3];
  };

  static constexpr Kind kKind = Kind::region;
  explicit SharedRegion(uint32_t size);
  uint32_t size() const { return block()->capacity; }
  /// False once the timeout, this thread's call deadline or the wait limit
  /// passed; the caller checks nesting first.
  bool lock(double timeoutSeconds);
  void unlock();
#if defined(WITH_RIVE_TOOLS) || defined(TESTING)
  /// Tests stand in for a coarse timer: every lock that sleeps wakes this
  /// much past its deadline.
  static void delayWakes(uint32_t ms);
#endif
  uint8_t* bytes() const { return data(); }
  /// The aligned word at offset, for lock free slot operations.
  std::atomic<uint32_t>* word(uint32_t offset);
  bool poisoned() const;
  void poison();

private:
#if RIVE_ONE_THREAD()
  bool m_locked = false;
#else
  std::timed_mutex m_lock;
#endif
};

/// Records in arrival order, read and written without sleeping or allocating.
/// A record carries holds on what its value names, which pass to its reader.
class SharedRing : public SharedObject
{
public:
  struct Block
  {
    SharedHeader header;
    uint32_t head;
    uint32_t used;
    /// Registry ids of the two signals, for lanes without librive.
    uint32_t dataSignal;
    uint32_t spaceSignal;
  };

  static constexpr Kind kKind = Kind::ring;
  explicit SharedRing(uint32_t capacity);
  bool made() const override;
  /// The ring bytes a record and the holds it carries take.
  static uint64_t recordBytes(size_t size, size_t holdCount);
  /// False when the record and its holds do not fit, or it retired.
  bool write(Span<const uint8_t> record, Span<const uint32_t> holds = {});
  /// Size of the head record plus one, 0 when empty; pops it and its holds
  /// when it fits out, unless it carries holds and holds is null.
  uint32_t read(
      uint8_t* out,
      uint32_t capacity,
      std::vector<uint32_t>* holds = nullptr);
  /// Pops the head record and its holds when it is larger than capacity,
  /// as a reader drops one too large for it; its size plus one, 0 when
  /// there was none to drop.
  uint32_t drop(uint32_t capacity, std::vector<uint32_t>* holds = nullptr);
  uint32_t used();
  rcp<SharedSignal> dataSignal() const { return m_data; }
  rcp<SharedSignal> spaceSignal() const { return m_space; }
  /// Records the signals' registry ids once they are minted.
  void setSignalIds(uint32_t data, uint32_t space);
  uint32_t dataSignalId() const { return ring()->dataSignal; }
  uint32_t spaceSignalId() const { return ring()->spaceSignal; }
  /// Its signals and what its records still carry; wakes their waits.
  void takeHolds(std::vector<uint32_t>& out) override;

private:
  Block* ring() const { return (Block*)block(); }
  /// What read and drop share: the head record's size plus one, 0 when
  /// empty; pops it when take answers true for its size.
  template <typename Take>
  uint32_t takeHead(std::vector<uint32_t>* holds, bool& popped, Take&& take);
  /// Pops the head record, appending its holds when given.
  void pop(uint32_t bytes, uint32_t count, std::vector<uint32_t>* holds);
  void copyIn(const uint8_t* from, uint32_t size);
  void copyOut(uint32_t at, uint8_t* to, uint32_t size) const;
  /// The record whose size word is at at: the bytes after the word, and
  /// how many holds end them.
  void peekRecord(uint32_t at, uint32_t& bytes, uint32_t& count) const;
  void copyHolds(
      uint32_t at,
      uint32_t bytes,
      uint32_t count,
      std::vector<uint32_t>& out) const;

  rcp<SharedSignal> m_data;
  rcp<SharedSignal> m_space;
};

/// The latest value, within a capacity fixed at creation. A publish writes the
/// copy readers are not on, so no reader waits or sees a mix of two values.
class SharedSnapshot : public SharedObject
{
public:
  static constexpr uint32_t kCopies = shared_layout::kSnapshotCopies;
  struct Block
  {
    SharedHeader header;
    std::atomic<uint32_t> latest;
    uint32_t reserved;
    SharedSeq seq;
    std::atomic<uint32_t> versions[kCopies];
    std::atomic<uint32_t> sizes[kCopies];
  };

  static constexpr Kind kKind = Kind::snapshot;
  explicit SharedSnapshot(uint32_t capacity);
  /// False when the value and its holds exceed the capacity, or it retired.
  /// Appends the holds it replaces to replaced; without it, replaces none.
  bool publish(
      Span<const uint8_t> value,
      Span<const uint32_t> holds = {},
      std::vector<uint32_t>* replaced = nullptr);
  /// Size of the latest value plus one, 0 before any publish; copies it and
  /// its holds when it fits out, unless it carries holds and holds is null.
  uint32_t read(
      uint8_t* out,
      uint32_t capacity,
      std::vector<uint32_t>* holds = nullptr);
  uint64_t seq() const;
  /// What the latest value carries.
  void takeHolds(std::vector<uint32_t>& out) override;

private:
  Block* snapshot() const { return (Block*)block(); }
  uint8_t* copy(uint32_t index) const
  {
    return data() + (size_t)index * block()->capacity;
  }
  /// The size of copy index's value, appending its holds when given. A torn
  /// read stays inside the copy, and the version check discards it.
  uint32_t valueSize(uint32_t index, std::vector<uint32_t>* holds, bool torn)
      const;
  bool carriesHolds(uint32_t index) const;
};

/// Bytes fixed at creation, so every lane, the audio thread included, reads
/// them without a lock.
class SharedBlob : public SharedObject
{
public:
  static constexpr Kind kKind = Kind::blob;
  explicit SharedBlob(Span<const uint8_t> bytes);
  uint32_t size() const { return block()->capacity; }
  const uint8_t* bytes() const { return data(); }
  /// Copies what lies past offset into out, up to capacity; answers the
  /// bytes copied.
  uint32_t read(uint32_t offset, uint8_t* out, uint32_t capacity) const;
};

/// A registry slot. Lookups pin it rather than the object, so a release
/// frees nothing a lookup in flight still reads.
struct SharedSlot
{
  std::atomic<uint32_t> state;
  std::atomic<SharedHeader*> block;
  std::atomic<SharedObject*> object;
  uint32_t nextFree;
  std::atomic<uint64_t> owner;
};

/// Where every lane looks ids up, librive's lanes and those without it.
struct SharedTable
{
  std::atomic<int32_t> lock;
  std::atomic<uint32_t> count;
  uint32_t freeHead;
  uint32_t freeTail;
  /// A lane without librive pins by storing the slot in its word here.
  std::atomic<uintptr_t> lanes[shared_layout::kLaneCount];
  /// The lock tokens lanes without librive took past their lane words.
  std::atomic<int32_t> tokens;
  std::atomic<SharedSlot*> chunks[shared_layout::kChunkCount];
};

/// Ids every lane names shared objects by. A lookup takes no lock and holds
/// the object through one native call, so no release frees it in use.
class SharedRegistry
{
public:
  /// An id with the top bit, never a VM handle, the object starting with its
  /// creator's hold; else the Create code why not.
  static uint32_t add(rcp<SharedObject> object, uint64_t owner);
  /// The T id names when owner's VM made it, so no script reaches
  /// another's objects.
  template <typename T> static rcp<T> find(uint32_t id, uint64_t owner)
  {
    return static_rcp_cast<T>(find(id, T::kKind, owner));
  }
  /// Another hold on what owner's VM made; false once it is gone.
  static bool retain(uint32_t id, uint64_t owner);
  /// Gives back one hold on what owner's VM made; the last frees the slot
  /// and a ring's signals, once no lookup still holds it.
  static void release(uint32_t id, uint64_t owner);
  /// Frees everything owner's VM made, however many holds are left.
  static void releaseOwner(uint64_t owner);
  /// The id of owner's ring that signal belongs to, else 0; space is set
  /// when it is the ring's space signal.
  static uint32_t ringOf(uint32_t signal, uint64_t owner, bool& space);
  /// A new owner, whose shared memory may come to byteBudget at once.
  static uint64_t nextOwner(
      uint64_t byteBudget = ScriptingLimits().sharedBytesPerFile);
  static SharedTable* table();
  /// Charges bytes to owner's budget; false, charging nothing, when they do
  /// not fit.
  static bool charge(uint64_t owner, uint64_t bytes);
  static void refund(uint64_t owner, uint64_t bytes);

private:
  static rcp<SharedObject> find(
      uint32_t id,
      SharedObject::Kind kind,
      uint64_t owner);
};

/// The region this thread holds, for nesting, view checks and freeing what a
/// trap left locked; held alive even past its owner's release.
uint32_t sharedHeldRegion();
/// The held region when id names it, else null.
SharedRegion* sharedHeld(uint32_t id);
void setSharedHeldRegion(uint32_t id, rcp<SharedRegion> region);
/// After a call trapped: unlock and poison a region the trap left held.
void releaseTrappedRegion();
/// Sets this thread's held region aside, so a call into another VM can
/// neither use nor free it.
SharedRegion* stashHeldRegion(uint32_t* id);
/// Frees what the call left held after a trap, then puts the stash back.
void restoreHeldRegion(uint32_t id, SharedRegion* region);

} // namespace rive

#endif
#endif
