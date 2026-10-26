#ifdef WITH_RIVE_SCRIPTING_WASM
#include "catch.hpp"
#include "rive/wasm/wasm_scripting_vm.hpp"
#include "rive/wasm/wasm_lane_layout_gen.hpp"
#include "rive/wasm/wasm_shared.hpp"
#include "rive/wasm/wasm_signal_wait.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <random>
#include <thread>
#include <vector>

using namespace rive;

// Yields until done holds, false past a deadline only a stuck thread misses.
template <typename Done> static bool waitUntil(Done done)
{
  auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
  while (!done())
  {
    if (std::chrono::steady_clock::now() >= deadline)
    {
      return false;
    }
    std::this_thread::yield();
  }
  return true;
}

TEST_CASE("a region's lock excludes every other thread", "[shared]")
{
  auto region = make_rcp<SharedRegion>(64);
  constexpr int kThreads = 8;
  constexpr int kRounds = 2000;
  std::vector<std::thread> threads;
  for (int t = 0; t < kThreads; t++)
  {
    threads.emplace_back([&]() {
      for (int i = 0; i < kRounds; i++)
      {
        REQUIRE(region->lock(INFINITY));
        uint32_t value;
        memcpy(&value, region->bytes(), 4);
        value++;
        memcpy(region->bytes(), &value, 4);
        region->unlock();
      }
    });
  }
  for (auto& thread : threads)
  {
    thread.join();
  }
  uint32_t total;
  memcpy(&total, region->bytes(), 4);
  CHECK(total == kThreads * kRounds);
}

TEST_CASE("a region's lock times out while another thread holds it", "[shared]")
{
  auto region = make_rcp<SharedRegion>(8);
  REQUIRE(region->lock(INFINITY));
  bool got = true;
  std::thread other([&]() { got = region->lock(0.02); });
  other.join();
  CHECK_FALSE(got);
  region->unlock();
}

TEST_CASE("word atomics count every add", "[shared]")
{
  auto region = make_rcp<SharedRegion>(16);
  CHECK(region->word(2) == nullptr);
  CHECK(region->word(16) == nullptr);
  std::vector<std::thread> threads;
  for (int t = 0; t < 8; t++)
  {
    threads.emplace_back([&]() {
      for (int i = 0; i < 10000; i++)
      {
        region->word(4)->fetch_add(1);
      }
    });
  }
  for (auto& thread : threads)
  {
    thread.join();
  }
  CHECK(region->word(4)->load() == 80000);
}

TEST_CASE("a ring keeps each writer's records in order", "[shared]")
{
  // Small, so records wrap around the buffer constantly.
  auto ring = make_rcp<SharedRing>(100);
  constexpr int kWriters = 4;
  constexpr int kRecords = 5000;
  std::vector<std::thread> writers;
  for (int w = 0; w < kWriters; w++)
  {
    writers.emplace_back([&, w]() {
      for (int i = 0; i < kRecords; i++)
      {
        uint32_t record[2] = {(uint32_t)w, (uint32_t)i};
        while (!ring->write(Span<const uint8_t>((uint8_t*)record, 8)))
        {
          std::this_thread::yield();
        }
      }
    });
  }
  std::vector<int> next(kWriters, 0);
  int read = 0;
  while (read < kWriters * kRecords)
  {
    uint32_t record[2];
    uint32_t size = ring->read((uint8_t*)record, 8);
    if (size == 0)
    {
      std::this_thread::yield();
      continue;
    }
    REQUIRE(size == 9);
    REQUIRE((int)record[1] == next[record[0]]);
    next[record[0]]++;
    read++;
  }
  for (auto& writer : writers)
  {
    writer.join();
  }
  CHECK(ring->used() == 0);
  CHECK(ring->dataSignal()->seq() == kWriters * kRecords);
}

TEST_CASE("a full ring refuses a write and frees space on read", "[shared]")
{
  auto ring = make_rcp<SharedRing>(16);
  uint8_t bytes[12] = {};
  CHECK(ring->write(Span<const uint8_t>(bytes, 12)));
  CHECK_FALSE(ring->write(Span<const uint8_t>(bytes, 12)));
  // A read too small for the head answers its size and leaves it queued.
  CHECK(ring->read(bytes, 4) == 13);
  // A record's length counts against the capacity too.
  CHECK(ring->used() == 16);
  CHECK(ring->read(bytes, 12) == 13);
  CHECK(ring->write(Span<const uint8_t>(bytes, 12)));
}

TEST_CASE("a drop pops only a head record too large for the reader", "[shared]")
{
  auto ring = make_rcp<SharedRing>(64);
  uint8_t bytes[12] = {};
  uint32_t ids[1] = {7};
  CHECK(ring->write(Span<const uint8_t>(bytes, 12)));
  CHECK(
      ring->write(Span<const uint8_t>(bytes, 2), Span<const uint32_t>(ids, 1)));
  CHECK(ring->drop(12) == 0);
  CHECK(ring->drop(8) == 13);
  // A record naming shared objects is left alone without a place for them.
  CHECK(ring->drop(0) == SharedObject::kCarriesHolds);
  std::vector<uint32_t> holds;
  CHECK(ring->drop(0, &holds) == 3);
  CHECK(holds == std::vector<uint32_t>{7});
  CHECK(ring->used() == 0);
  CHECK(ring->drop(0) == 0);
}

TEST_CASE(
    "a ring record carries its holds to its reader or its drop",
    "[shared]")
{
  auto ring = make_rcp<SharedRing>(64);
  uint8_t bytes[4] = {1, 2, 3, 4};
  uint32_t ids[2] = {7, 9};
  // Each hold takes four bytes more, and their count four.
  CHECK(
      ring->write(Span<const uint8_t>(bytes, 4), Span<const uint32_t>(ids, 2)));
  CHECK(ring->used() == 4 + 4 + 12);
  CHECK(
      ring->write(Span<const uint8_t>(bytes, 4), Span<const uint32_t>(ids, 1)));
  std::vector<uint32_t> holds;
  uint8_t out[4] = {};
  CHECK(ring->read(out, 2, &holds) == 5);
  CHECK(holds.empty());
  CHECK(ring->read(out, 4, &holds) == 5);
  CHECK(out[3] == 4);
  CHECK(holds == std::vector<uint32_t>{7, 9});
  // What the ring still queues goes back with its signals.
  std::vector<uint32_t> left;
  ring->takeHolds(left);
  CHECK(left.size() == 3);
  CHECK(left[2] == 7);
  CHECK(ring->used() == 0);
}

TEST_CASE("a snapshot holds what its latest value names", "[shared]")
{
  auto snapshot = make_rcp<SharedSnapshot>(32);
  uint32_t value = 42;
  uint32_t first[1] = {5};
  uint32_t second[2] = {6, 8};
  std::vector<uint32_t> replaced;
  CHECK(snapshot->publish(
      Span<const uint8_t>((uint8_t*)&value, 4),
      Span<const uint32_t>(first, 1),
      &replaced));
  CHECK(replaced.empty());
  CHECK(snapshot->publish(
      Span<const uint8_t>((uint8_t*)&value, 4),
      Span<const uint32_t>(second, 2),
      &replaced));
  CHECK(replaced == std::vector<uint32_t>{5});
  std::vector<uint32_t> holds;
  uint32_t out = 0;
  CHECK(snapshot->read((uint8_t*)&out, 4, &holds) == 5);
  CHECK(out == 42);
  CHECK(holds == std::vector<uint32_t>{6, 8});
  uint32_t many[8] = {};
  CHECK_FALSE(snapshot->publish(
      Span<const uint8_t>((uint8_t*)&value, 4),
      Span<const uint32_t>(many, 8),
      &replaced));
  std::vector<uint32_t> left;
  snapshot->takeHolds(left);
  CHECK(left == std::vector<uint32_t>{6, 8});
  CHECK(snapshot->read(nullptr, 0) == 0);
}

TEST_CASE(
    "snapshot readers see whole values while a writer publishes",
    "[shared]")
{
  auto snapshot = make_rcp<SharedSnapshot>(64);
  CHECK(snapshot->read(nullptr, 0) == 0);
  std::atomic<bool> done{false};
  std::thread writer([&]() {
    for (uint32_t i = 1; i <= 20000; i++)
    {
      uint32_t value[16];
      for (uint32_t& word : value)
      {
        word = i;
      }
      snapshot->publish(Span<const uint8_t>((uint8_t*)value, 64));
    }
    done = true;
  });
  while (!done.load())
  {
    uint32_t value[16];
    if (snapshot->read((uint8_t*)value, 64) == 65)
    {
      for (uint32_t word : value)
      {
        REQUIRE(word == value[0]);
      }
    }
  }
  writer.join();
  CHECK(snapshot->seq() == 20000);
}

TEST_CASE("a signal counts every notify and wakes its watchers", "[shared]")
{
  auto signal = make_rcp<SharedSignal>();
  WakeWord watcher;
  signal->addWatcher(&watcher);
  std::vector<std::thread> notifiers;
  for (int t = 0; t < 4; t++)
  {
    notifiers.emplace_back([&]() {
      for (int i = 0; i < 5000; i++)
      {
        signal->notify();
      }
    });
  }
  for (auto& notifier : notifiers)
  {
    notifier.join();
  }
  CHECK(signal->seq() == 20000);
  CHECK(watcher.value() == 20000);
  signal->removeWatcher(&watcher);
  signal->notify();
  CHECK(watcher.value() == 20000);
}

TEST_CASE("a wake word ends a wait from another thread", "[shared]")
{
  WakeWord word;
  for (int round = 0; round < 200; round++)
  {
    uint32_t seen = word.value();
    std::thread waker([&]() { word.wake(); });
    word.wait(seen, std::chrono::steady_clock::time_point::max());
    CHECK(word.value() != seen);
    waker.join();
  }
  uint32_t seen = word.value();
  auto began = std::chrono::steady_clock::now();
  word.wait(seen, began + std::chrono::milliseconds(10));
  CHECK(word.value() == seen);
  CHECK(
      std::chrono::steady_clock::now() - began >=
      std::chrono::milliseconds(10));
}

TEST_CASE("a snapshot refuses a value past its capacity", "[shared]")
{
  auto snapshot = make_rcp<SharedSnapshot>(8);
  uint8_t bytes[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
  CHECK(snapshot->publish(Span<const uint8_t>(bytes, 8)));
  CHECK_FALSE(snapshot->publish(Span<const uint8_t>(bytes, 12)));
  CHECK(snapshot->seq() == 1);
  uint8_t out[8] = {};
  CHECK(snapshot->read(out, 8) == 9);
  CHECK(out[7] == 8);
}

TEST_CASE("a blob keeps a copy of its bytes and reads any slice", "[shared]")
{
  using namespace shared_layout;
  std::vector<uint8_t> bytes = {1, 2, 3, 4, 5, 6, 7};
  auto blob = make_rcp<SharedBlob>(Span<const uint8_t>(bytes));
  bytes[0] = 99;
  CHECK(blob->size() == 7);
  CHECK(blob->block()->kind == kKindBlob);
  const uint8_t* data = (const uint8_t*)blob->block() + kBlobData;
  CHECK(blob->bytes() == data);
  CHECK(data[0] == 1);
  CHECK(data[6] == 7);
  uint8_t out[4] = {};
  CHECK(blob->read(0, out, 4) == 4);
  CHECK(out[3] == 4);
  // A read near the end copies only what is left.
  CHECK(blob->read(5, out, 4) == 2);
  CHECK(out[0] == 6);
  CHECK(out[1] == 7);
  CHECK(blob->read(7, out, 4) == 0);
  CHECK(blob->read(UINT32_MAX, out, 4) == 0);
  auto empty = make_rcp<SharedBlob>(Span<const uint8_t>());
  CHECK(empty->size() == 0);
  CHECK(empty->read(0, out, 4) == 0);
}

TEST_CASE("blob readers on many threads see the same bytes", "[shared]")
{
  std::vector<uint8_t> bytes(64 * 1024);
  for (size_t i = 0; i < bytes.size(); i++)
  {
    bytes[i] = (uint8_t)(i * 7);
  }
  uint64_t owner = SharedRegistry::nextOwner();
  uint32_t id = SharedRegistry::add(
      make_rcp<SharedBlob>(Span<const uint8_t>(bytes)),
      owner);
  std::vector<std::thread> threads;
  // Not vector<bool>, whose packed bits threads cannot write apart.
  std::vector<char> same(8);
  for (int t = 0; t < 8; t++)
  {
    threads.emplace_back([&, t]() {
      std::vector<uint8_t> out(bytes.size());
      bool ok = true;
      for (int round = 0; round < 50; round++)
      {
        auto blob = SharedRegistry::find<SharedBlob>(id, owner);
        ok = ok && blob != nullptr &&
             blob->read(0, out.data(), (uint32_t)out.size()) == out.size() &&
             out == bytes;
      }
      same[t] = ok;
    });
  }
  for (auto& thread : threads)
  {
    thread.join();
  }
  CHECK(std::all_of(same.begin(), same.end(), [](char ok) { return ok; }));
  CHECK(SharedRegistry::find<SharedRegion>(id, owner) == nullptr);
  SharedRegistry::release(id, owner);
  CHECK(SharedRegistry::find<SharedBlob>(id, owner) == nullptr);
}

TEST_CASE("the registry table leads from an id to its block", "[shared]")
{
  using namespace shared_layout;
  uint64_t owner = SharedRegistry::nextOwner();
  auto ring = make_rcp<SharedRing>(64);
  uint32_t id = SharedRegistry::add(ring, owner);
  const SharedTable* table = SharedRegistry::table();
  uint32_t index = (id & ~kIdBit) >> kIdSlotShift;
  REQUIRE(index < table->count.load());
  const SharedSlot* chunk = table->chunks[index >> kChunkShift].load();
  REQUIRE(chunk != nullptr);
  const SharedSlot& slot = chunk[index & (kChunkSlots - 1)];
  CHECK(slot.block.load() == ring->block());
  CHECK((slot.state.load() & kSlotLive) != 0);
  CHECK((slot.state.load() & kIdGenerationMask) == (id & kIdGenerationMask));
  CHECK(ring->block()->kind == kKindRing);
  CHECK(ring->block()->capacity == 64);
  SharedRegistry::release(id, owner);
  CHECK((slot.state.load() & kSlotLive) == 0);
  CHECK(slot.block.load() == nullptr);
}

TEST_CASE("registry ids check kind, owner and generation", "[shared]")
{
  uint64_t owner = SharedRegistry::nextOwner();
  uint64_t other = SharedRegistry::nextOwner();
  uint32_t id = SharedRegistry::add(make_rcp<SharedRegion>(8), owner);
  CHECK((id & 0x80000000u) != 0);
  CHECK(SharedRegistry::find<SharedRegion>(id, owner) != nullptr);
  CHECK(SharedRegistry::find<SharedRing>(id, owner) == nullptr);
  CHECK(SharedRegistry::find<SharedRegion>(id, other) == nullptr);
  // Another owner's release does nothing.
  SharedRegistry::release(id, other);
  CHECK(SharedRegistry::find<SharedRegion>(id, owner) != nullptr);
  // A lookup keeps the object alive past its release.
  auto held = SharedRegistry::find<SharedRegion>(id, owner);
  SharedRegistry::release(id, owner);
  CHECK(SharedRegistry::find<SharedRegion>(id, owner) == nullptr);
  CHECK(held->kind() == SharedObject::Kind::region);
  // A reused slot gets a new generation, so the old id stays dead.
  uint32_t reused = SharedRegistry::add(make_rcp<SharedSignal>(), owner);
  CHECK(reused != id);
  CHECK(SharedRegistry::find<SharedSignal>(id, owner) == nullptr);
  SharedRegistry::releaseOwner(owner);
  CHECK(SharedRegistry::find<SharedSignal>(reused, owner) == nullptr);
}

TEST_CASE("recent regions answer only what the registry would", "[shared]")
{
  uint64_t owner = SharedRegistry::nextOwner();
  uint64_t other = SharedRegistry::nextOwner();
  auto region = make_rcp<SharedRegion>(8);
  int32_t refs = region->debugging_refcnt();
  uint32_t id = SharedRegistry::add(region, owner);
  // The first lookup keeps the region, the second takes it from the cache.
  CHECK(sharedRecentRegion(id, owner) == region.get());
  CHECK(sharedRecentRegion(id, owner) == region.get());
  CHECK(sharedRecentRegion(id, other) == nullptr);
  // A released id misses although the cache still names it.
  SharedRegistry::release(id, owner);
  CHECK(sharedRecentRegion(id, owner) == nullptr);
  CHECK(region->debugging_refcnt() == refs);
  // A reused slot answers only its new id.
  uint32_t reused = SharedRegistry::add(make_rcp<SharedRegion>(8), owner);
  CHECK(sharedRecentRegion(reused, owner) != nullptr);
  CHECK(sharedRecentRegion(id, owner) == nullptr);
  auto kept = make_rcp<SharedRegion>(8);
  int32_t keptRefs = kept->debugging_refcnt();
  uint32_t keptId = SharedRegistry::add(kept, owner);
  CHECK(sharedRecentRegion(keptId, owner) == kept.get());
  SharedRegistry::release(keptId, owner);
  forgetRecentRegions();
  CHECK(kept->debugging_refcnt() == keptRefs);
  // A slot reused until its id comes round again answers with its new region.
  auto old = make_rcp<SharedRegion>(8);
  uint32_t oldId = SharedRegistry::add(old, owner);
  CHECK(sharedRecentRegion(oldId, owner) == old.get());
  SharedRegistry::release(oldId, owner);
  rcp<SharedRegion> fresh;
  for (int i = 0; i < 20000 && fresh == nullptr; i++)
  {
    auto next = make_rcp<SharedRegion>(8);
    uint32_t nextId = SharedRegistry::add(next, owner);
    if (nextId == oldId)
    {
      fresh = next;
    }
    else
    {
      SharedRegistry::release(nextId, owner);
    }
  }
  REQUIRE(fresh != nullptr);
  CHECK(sharedRecentRegion(oldId, owner) == fresh.get());
  forgetRecentRegions();
  SharedRegistry::releaseOwner(owner);
}

TEST_CASE("each hold keeps a registry object until the last goes", "[shared]")
{
  uint64_t owner = SharedRegistry::nextOwner();
  uint64_t other = SharedRegistry::nextOwner();
  uint32_t id = SharedRegistry::add(make_rcp<SharedRegion>(8), owner);
  CHECK_FALSE(SharedRegistry::retain(id, other));
  CHECK(SharedRegistry::retain(id, owner));
  SharedRegistry::release(id, owner);
  CHECK(SharedRegistry::find<SharedRegion>(id, owner) != nullptr);
  SharedRegistry::release(id, owner);
  CHECK(SharedRegistry::find<SharedRegion>(id, owner) == nullptr);
  CHECK_FALSE(SharedRegistry::retain(id, owner));
  // Teardown frees what is still held.
  uint32_t held = SharedRegistry::add(make_rcp<SharedRegion>(8), owner);
  CHECK(SharedRegistry::retain(held, owner));
  SharedRegistry::releaseOwner(owner);
  CHECK(SharedRegistry::find<SharedRegion>(held, owner) == nullptr);
}

TEST_CASE("a ring's signals go with it", "[shared]")
{
  uint64_t owner = SharedRegistry::nextOwner();
  auto ring = make_rcp<SharedRing>(64);
  uint32_t data = SharedRegistry::add(ring->dataSignal(), owner);
  uint32_t space = SharedRegistry::add(ring->spaceSignal(), owner);
  ring->setSignalIds(data, space);
  uint32_t id = SharedRegistry::add(ring, owner);
  CHECK(SharedRegistry::find<SharedSignal>(data, owner) != nullptr);
  SharedRegistry::release(id, owner);
  CHECK(SharedRegistry::find<SharedRing>(id, owner) == nullptr);
  CHECK(SharedRegistry::find<SharedSignal>(data, owner) == nullptr);
  CHECK(SharedRegistry::find<SharedSignal>(space, owner) == nullptr);
}

TEST_CASE("a ring going wakes the waits on a signal kept past it", "[shared]")
{
  uint64_t owner = SharedRegistry::nextOwner();
  auto ring = make_rcp<SharedRing>(64);
  uint32_t data = SharedRegistry::add(ring->dataSignal(), owner);
  uint32_t space = SharedRegistry::add(ring->spaceSignal(), owner);
  ring->setSignalIds(data, space);
  uint32_t id = SharedRegistry::add(ring, owner);
  REQUIRE(SharedRegistry::retain(data, owner));
  WakeWord word;
  ring->dataSignal()->addWatcher(&word);
  uint32_t seen = word.value();
  uint64_t seq = ring->dataSignal()->seq();
  ring = nullptr;
  SharedRegistry::release(id, owner);
  auto kept = SharedRegistry::find<SharedSignal>(data, owner);
  REQUIRE(kept != nullptr);
  CHECK(kept->seq() > seq);
  CHECK(word.value() != seen);
  kept->removeWatcher(&word);
  SharedRegistry::release(data, owner);
  CHECK(SharedRegistry::find<SharedSignal>(data, owner) == nullptr);
}

TEST_CASE("a trapped holder's region is poisoned and unlocked", "[shared]")
{
  auto region = make_rcp<SharedRegion>(8);
  REQUIRE(region->lock(INFINITY));
  setSharedHeldRegion(0x80000100u, region);
  CHECK(sharedHeld(0x80000100u) == region.get());
  releaseTrappedRegion();
  CHECK(region->poisoned());
  CHECK(sharedHeldRegion() == 0);
  CHECK(region->lock(0));
  region->unlock();
}

TEST_CASE("a nested call frees only the region it took", "[shared]")
{
  auto outer = make_rcp<SharedRegion>(8);
  auto inner = make_rcp<SharedRegion>(8);
  REQUIRE(outer->lock(INFINITY));
  setSharedHeldRegion(0x80000100u, outer);
  uint32_t stashedId = 0;
  SharedRegion* stashed = stashHeldRegion(&stashedId);
  CHECK(sharedHeld(0x80000100u) == nullptr);
  REQUIRE(inner->lock(INFINITY));
  setSharedHeldRegion(0x80000200u, inner);
  restoreHeldRegion(stashedId, stashed);
  CHECK(inner->poisoned());
  CHECK(!outer->poisoned());
  CHECK(sharedHeld(0x80000100u) == outer.get());
  setSharedHeldRegion(0, nullptr);
  outer->unlock();
}

TEST_CASE("a released id stays dead while its slot is reused", "[shared]")
{
  uint64_t owner = SharedRegistry::nextOwner();
  uint32_t stale = SharedRegistry::add(make_rcp<SharedSignal>(), owner);
  SharedRegistry::release(stale, owner);
  // Far past the 256 reuses an 8 bit generation would have survived.
  for (int i = 0; i < 1000; i++)
  {
    uint32_t id = SharedRegistry::add(make_rcp<SharedSignal>(), owner);
    CHECK(id != stale);
    CHECK(SharedRegistry::find<SharedSignal>(stale, owner) == nullptr);
    SharedRegistry::release(id, owner);
  }
}

TEST_CASE("lookups race releases and slot reuse", "[shared]")
{
  // Each region's bytes hold the id it was added under, so a lookup that
  // reached a reused slot's new object, or freed memory, would see another.
  constexpr int kIds = 64;
  uint64_t owner = SharedRegistry::nextOwner();
  std::atomic<uint32_t> ids[kIds];
  auto addTagged = [owner]() {
    auto region = make_rcp<SharedRegion>(8);
    uint32_t id = SharedRegistry::add(region, owner);
    region->word(0)->store(id);
    return id;
  };
  for (auto& id : ids)
  {
    id.store(addTagged());
  }
  std::atomic<bool> done{false};
  std::atomic<uint32_t> mismatches{0};
  std::atomic<uint64_t> hits{0};
  std::vector<std::thread> finders;
  for (int t = 0; t < 6; t++)
  {
    finders.emplace_back([&, t]() {
      std::minstd_rand random(t);
      while (!done.load())
      {
        uint32_t id = ids[random() % kIds].load();
        auto region = SharedRegistry::find<SharedRegion>(id, owner);
        if (region == nullptr)
        {
          continue;
        }
        hits++;
        // The tag is stored right after the add publishes the id.
        uint32_t tag = region->word(0)->load();
        if (tag != id && tag != 0)
        {
          mismatches++;
        }
      }
    });
  }
  std::vector<std::thread> churners;
  for (int t = 0; t < 2; t++)
  {
    churners.emplace_back([&, t]() {
      std::minstd_rand random(100 + t);
      for (int i = 0; i < 20000; i++)
      {
        uint32_t id = ids[random() % kIds].exchange(0);
        if (id == 0)
        {
          continue;
        }
        SharedRegistry::release(id, owner);
        if (SharedRegistry::find<SharedRegion>(id, owner) != nullptr)
        {
          mismatches++;
        }
        uint32_t displaced = ids[random() % kIds].exchange(addTagged());
        if (displaced != 0)
        {
          SharedRegistry::release(displaced, owner);
        }
      }
    });
  }
  for (auto& churner : churners)
  {
    churner.join();
  }
  done = true;
  for (auto& finder : finders)
  {
    finder.join();
  }
  CHECK(mismatches.load() == 0);
  CHECK(hits.load() > 0);
  SharedRegistry::releaseOwner(owner);
  for (auto& id : ids)
  {
    CHECK(SharedRegistry::find<SharedRegion>(id.load(), owner) == nullptr);
  }
}

TEST_CASE("a release waits out a lane's pin", "[shared]")
{
  // A lane without librive pins as rive_shared_lane.mjs does: its lane
  // word names the slot, then it checks the slot is still live.
  using namespace shared_layout;
  SharedTable* table = SharedRegistry::table();
  std::atomic<uintptr_t>* lane = nullptr;
  for (auto& word : table->lanes)
  {
    uintptr_t free = 0;
    if (word.compare_exchange_strong(free, kLaneIdle))
    {
      lane = &word;
      break;
    }
  }
  REQUIRE(lane != nullptr);
  uint64_t owner = SharedRegistry::nextOwner();
  uint32_t id = SharedRegistry::add(make_rcp<SharedSignal>(), owner);
  uint32_t index = (id & ~kIdBit) >> kIdSlotShift;
  SharedSlot* slot = table->chunks[index >> kChunkShift].load() +
                     (index & (kChunkSlots - 1));
  lane->store((uintptr_t)slot);
  REQUIRE((slot->state.load() & kSlotLive) != 0);
  std::atomic<bool> released{false};
  std::thread releaser([&]() {
    SharedRegistry::release(id, owner);
    released = true;
  });
  // Retired, so no new lookup takes it, but not freed while pinned.
  REQUIRE(waitUntil([&]() { return (slot->state.load() & kSlotLive) == 0; }));
  CHECK(SharedRegistry::find<SharedSignal>(id, owner) == nullptr);
  CHECK_FALSE(released.load());
  CHECK(slot->block.load()->kind == kKindSignal);
  lane->store(kLaneIdle);
  releaser.join();
  CHECK(released.load());
  CHECK(slot->block.load() == nullptr);
  lane->store(0);
}

TEST_CASE(
    "snapshot readers never see a torn value while publishers race",
    "[shared]")
{
  // Each value is a run of one word, its length set by that word, so a
  // torn read shows as mixed words or a size that does not match them.
  constexpr uint32_t kWords = 64;
  auto snapshot = make_rcp<SharedSnapshot>(kWords * 4);
  std::atomic<bool> done{false};
  std::atomic<uint32_t> torn{0};
  std::atomic<uint64_t> reads{0};
  // Publishers start once every reader runs, so reads race publishes.
  std::atomic<int> readersReady{0};
  std::vector<std::thread> threads;
  for (uint32_t p = 0; p < 3; p++)
  {
    threads.emplace_back([&, p]() {
      waitUntil([&]() { return readersReady.load() == 3; });
      uint32_t value[kWords];
      for (uint32_t i = 1; i <= 20000; i++)
      {
        uint32_t word = i * 3 + p;
        uint32_t count = 1 + word % kWords;
        for (uint32_t k = 0; k < count; k++)
        {
          value[k] = word;
        }
        snapshot->publish(Span<const uint8_t>((uint8_t*)value, count * 4));
      }
    });
  }
  std::vector<std::thread> readers;
  for (int r = 0; r < 3; r++)
  {
    readers.emplace_back([&]() {
      uint32_t value[kWords];
      readersReady++;
      // Past the last publish a read always finds a value.
      uint64_t mine = 0;
      while (!done.load() || mine == 0)
      {
        uint32_t size = snapshot->read((uint8_t*)value, kWords * 4);
        if (size == 0)
        {
          continue;
        }
        mine++;
        reads++;
        uint32_t count = (size - 1) / 4;
        bool whole = count == 1 + value[0] % kWords;
        for (uint32_t k = 0; k < count; k++)
        {
          whole = whole && value[k] == value[0];
        }
        if (!whole)
        {
          torn++;
        }
      }
    });
  }
  for (auto& thread : threads)
  {
    thread.join();
  }
  done = true;
  for (auto& reader : readers)
  {
    reader.join();
  }
  CHECK(torn.load() == 0);
  CHECK(reads.load() > 0);
  CHECK(snapshot->seq() == 60000);
}

TEST_CASE("a publisher stopped mid write never blocks a reader", "[shared]")
{
  using namespace shared_layout;
  auto snapshot = make_rcp<SharedSnapshot>(8);
  uint8_t first[4] = {1, 2, 3, 4};
  REQUIRE(snapshot->publish(Span<const uint8_t>(first, 4)));
  // As a lane ended inside a publish leaves it: the lock held by its token
  // and the next copy's version odd.
  auto* block = (SharedSnapshot::Block*)snapshot->block();
  block->header.lock.store(kLaneFirstToken);
  uint32_t next = block->latest.load() % SharedSnapshot::kCopies;
  block->versions[next].fetch_or(1);
  uint8_t out[8] = {};
  CHECK(snapshot->read(out, 8) == 5);
  CHECK(out[3] == 4);
  // Once the lock is freed, the next publish repairs the odd version.
  block->header.lock.store(0);
  uint8_t second[2] = {9, 9};
  REQUIRE(snapshot->publish(Span<const uint8_t>(second, 2)));
  CHECK((block->versions[next].load() & 1) == 0);
  CHECK(snapshot->read(out, 8) == 3);
  CHECK(out[0] == 9);
}

TEST_CASE("a region lock gives up at the call deadline", "[shared]")
{
  using Clock = std::chrono::steady_clock;
  auto region = make_rcp<SharedRegion>(8);
  std::atomic<bool> held{false};
  std::atomic<bool> letGo{false};
  std::thread holder([&]() {
    held = region->lock(INFINITY);
    while (!letGo.load())
    {
      std::this_thread::yield();
    }
    region->unlock();
  });
  while (!held.load())
  {
    std::this_thread::yield();
  }
  auto waitedMs = [&]() {
    auto began = Clock::now();
    CHECK_FALSE(region->lock(INFINITY));
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               Clock::now() - began)
        .count();
  };
  // Without the deadline the wait would last the two second lane limit.
  Clock::time_point outer = armCallDeadline(
      Clock::now() + std::chrono::milliseconds(60));
  {
    // A later deadline nested inside keeps the earlier one.
    Clock::time_point nested = armCallDeadline(
        Clock::now() + std::chrono::seconds(20));
    CHECK(waitedMs() < 1000);
    restoreCallDeadline(nested);
  }
  CHECK(waitedMs() < 1000);
  restoreCallDeadline(outer);
  CHECK(callDeadline() == Clock::time_point::max());
  letGo = true;
  holder.join();
}

TEST_CASE("a retired ring or snapshot takes no more holds", "[shared]")
{
  uint64_t owner = SharedRegistry::nextOwner();
  uint32_t carried = SharedRegistry::add(make_rcp<SharedSignal>(), owner);
  const uint8_t value[] = {1, 2};
  auto ring = make_rcp<SharedRing>(64);
  std::vector<uint32_t> holds;
  ring->takeHolds(holds);
  CHECK_FALSE(ring->write(
      Span<const uint8_t>(value, 2),
      Span<const uint32_t>(&carried, 1)));
  CHECK(ring->used() == 0);
  auto snapshot = make_rcp<SharedSnapshot>(64);
  snapshot->takeHolds(holds);
  std::vector<uint32_t> replaced;
  CHECK_FALSE(snapshot->publish(
      Span<const uint8_t>(value, 2),
      Span<const uint32_t>(&carried, 1),
      &replaced));
  CHECK(snapshot->read(nullptr, 0, &holds) == 0);
  SharedRegistry::release(carried, owner);
}

TEST_CASE("a signal wakes a word many waits share once", "[shared]")
{
  auto signal = make_rcp<SharedSignal>();
  WakeWord word;
  signal->addWatcher(&word);
  signal->addWatcher(&word);
  signal->notify();
  CHECK(word.value() == 1);
  signal->removeWatcher(&word);
  signal->notify();
  CHECK(word.value() == 2);
  signal->removeWatcher(&word);
  signal->notify();
  CHECK(word.value() == 2);
}

TEST_CASE("a signal going wakes its watchers", "[shared]")
{
  uint64_t owner = SharedRegistry::nextOwner();
  auto signal = make_rcp<SharedSignal>();
  WakeWord word;
  signal->addWatcher(&word);
  uint32_t id = SharedRegistry::add(signal, owner);
  signal = nullptr;
  SharedRegistry::release(id, owner);
  CHECK(word.value() == 1);
  SharedRegistry::releaseOwner(owner);
}

TEST_CASE(
    "a blocking wait ends at a notify, its timeout or a signal gone",
    "[shared]")
{
  using Clock = std::chrono::steady_clock;
  uint64_t owner = SharedRegistry::nextOwner();
  auto signal = make_rcp<SharedSignal>();
  uint32_t id = SharedRegistry::add(signal, owner);
  auto wait = [&](double after, double seconds) {
    return SignalWait(0, owner, id, signal.get(), after, seconds).block();
  };
  auto began = Clock::now();
  CHECK(wait(0, 0.02) == lane_layout::kWaitTimedOut);
  CHECK(Clock::now() - began >= std::chrono::milliseconds(20));
  signal->notify();
  CHECK(wait(0, 30) == 1);
  std::thread notifier([&]() {
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    signal->notify();
  });
  CHECK(wait(1, 30) == 2);
  notifier.join();
  // The wait leaves no watcher behind for the next notify to wake.
  WakeWord word;
  signal->addWatcher(&word);
  signal->notify();
  CHECK(word.value() == 1);
  signal->removeWatcher(&word);
  CHECK(
      SignalWait(0, owner, 0, nullptr, 0, 30).block() ==
      lane_layout::kWaitGone);
  signal = nullptr;
  SharedRegistry::release(id, owner);
  CHECK(
      SignalWait(0, owner, id, nullptr, 0, 30).block() ==
      lane_layout::kWaitGone);
  SharedRegistry::releaseOwner(owner);
}

TEST_CASE("an owner's shared memory stays within its budget", "[shared]")
{
  uint64_t budget = 4096;
  uint64_t owner = SharedRegistry::nextOwner(budget);
  uint64_t other = SharedRegistry::nextOwner(budget);
  CHECK(SharedRegistry::charge(owner, budget - 8));
  CHECK_FALSE(SharedRegistry::charge(owner, 16));
  CHECK(SharedRegistry::charge(other, 16));
  auto region = make_rcp<SharedRegion>(8);
  REQUIRE(region->made());
  CHECK(SharedRegistry::charge(owner, 8));
  region->charge = 8;
  uint32_t id = SharedRegistry::add(region, owner);
  region = nullptr;
  CHECK_FALSE(SharedRegistry::charge(owner, 1));
  // Its charge comes back when it goes.
  SharedRegistry::release(id, owner);
  CHECK(SharedRegistry::charge(owner, 8));
  SharedRegistry::refund(owner, budget);
  SharedRegistry::refund(other, 16);
  CHECK(SharedRegistry::charge(owner, budget));
  // A released owner charges nothing more, nor makes objects that cost none.
  SharedRegistry::releaseOwner(owner);
  SharedRegistry::releaseOwner(other);
  CHECK_FALSE(SharedRegistry::charge(owner, 1));
  uint32_t late = SharedRegistry::add(make_rcp<SharedSignal>(), owner);
  CHECK((late & shared_layout::kIdBit) == 0);
}

TEST_CASE("a long chain of holds goes without deep recursion", "[shared]")
{
  // Each snapshot's value holds the next, so the first release retires
  // every one, which recursion would overflow a pool thread's stack with.
  constexpr int kLinks = 60000;
  uint64_t owner = SharedRegistry::nextOwner();
  std::vector<uint32_t> ids;
  for (int i = 0; i < kLinks; i++)
  {
    ids.push_back(SharedRegistry::add(make_rcp<SharedSnapshot>(8), owner));
  }
  for (int i = 0; i + 1 < kLinks; i++)
  {
    uint32_t next = ids[i + 1];
    REQUIRE(SharedRegistry::retain(next, owner));
    auto snapshot = SharedRegistry::find<SharedSnapshot>(ids[i], owner);
    std::vector<uint32_t> replaced;
    REQUIRE(snapshot->publish({}, Span<const uint32_t>(&next, 1), &replaced));
    SharedRegistry::release(next, owner);
  }
  // A thread's default stack, smaller than the main thread's.
  std::thread releaser([&]() { SharedRegistry::release(ids[0], owner); });
  releaser.join();
  CHECK(SharedRegistry::find<SharedSnapshot>(ids.back(), owner) == nullptr);
  SharedRegistry::releaseOwner(owner);
}

TEST_CASE("an owner keeps at most kSharedMaxObjects at once", "[shared]")
{
  using namespace shared_layout;
  uint64_t owner = SharedRegistry::nextOwner();
  uint64_t other = SharedRegistry::nextOwner();
  std::vector<uint32_t> ids;
  for (uint32_t i = 0; i < kSharedMaxObjects; i++)
  {
    ids.push_back(SharedRegistry::add(make_rcp<SharedSignal>(), owner));
  }
  CHECK(std::all_of(ids.begin(), ids.end(), [](uint32_t id) {
    return (id & kIdBit) != 0;
  }));
  CHECK(SharedRegistry::add(make_rcp<SharedSignal>(), owner) == kCreateTooMany);
  // The cap is each owner's own.
  uint32_t others = SharedRegistry::add(make_rcp<SharedSignal>(), other);
  CHECK((others & kIdBit) != 0);
  SharedRegistry::release(ids.back(), owner);
  CHECK((SharedRegistry::add(make_rcp<SharedSignal>(), owner) & kIdBit) != 0);
  SharedRegistry::releaseOwner(owner);
  SharedRegistry::releaseOwner(other);
}

TEST_CASE("host handles never look like shared ids", "[shared]")
{
  // The std's owner table tells the two apart by bit 31 alone.
  WasmScriptingVM::HandleTable handles;
  int dummy = 0;
  for (int i = 0; i < 300; i++)
  {
    uint32_t handle = handles.mint(
        WasmScriptingVM::HandleTable::Tag::path,
        &dummy);
    CHECK((handle & shared_layout::kIdBit) == 0);
    handles.release(handle, WasmScriptingVM::HandleTable::Tag::path);
  }
}
#endif
