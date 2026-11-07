#include <rive/dependent_list.hpp>
#include <catch.hpp>
#include <algorithm>
#include <cstdint>
#include <functional>
#include <random>
#include <vector>

// DependentList keeps each dependent once, in the order added, as the
// LazyVector it replaced did with pushUnique and eraseAll. Past
// kIndexedFrom dependents it finds them through an index instead of a scan,
// and a removal leaves a hole until the list closes its holes.

using namespace rive;

namespace
{
struct Dependent
{
  int id;
};

// What a list of dependents held before: a vector scanned to add each once,
// and erased from.
struct Reference
{
  std::vector<Dependent*> list;
  void add(Dependent* dependent)
  {
    if (std::find(list.begin(), list.end(), dependent) == list.end())
    {
      list.push_back(dependent);
    }
  }
  void remove(Dependent* dependent)
  {
    list.erase(std::remove(list.begin(), list.end(), dependent), list.end());
  }
};

std::vector<Dependent*> visited(const DependentList<Dependent>& list)
{
  std::vector<Dependent*> out;
  list.forEach([&](Dependent* dependent) { out.push_back(dependent); });
  return out;
}
} // namespace

TEST_CASE(
    "A dependent list keeps each dependent once, in order",
    "[dependents]")
{
  std::vector<Dependent> pool(200);
  for (size_t count :
       {size_t(5), DependentList<Dependent>::kIndexedFrom, size_t(200)})
  {
    INFO("count " << count);
    DependentList<Dependent> list;
    Reference reference;
    CHECK(list.empty());
    for (size_t i = 0; i < count; i++)
    {
      list.add(&pool[i]);
      list.add(&pool[i]);
      reference.add(&pool[i]);
    }
    CHECK(list.size() == count);
    CHECK(list.view() == reference.list);
    CHECK(visited(list) == reference.list);

    // Every third goes, the walk skips the holes, and the view has none.
    for (size_t i = 0; i < count; i += 3)
    {
      list.remove(&pool[i]);
      reference.remove(&pool[i]);
    }
    CHECK(list.size() == reference.list.size());
    CHECK(visited(list) == reference.list);
    CHECK(list.view() == reference.list);

    // One added again goes to the end, not back to its old place.
    list.add(&pool[0]);
    reference.add(&pool[0]);
    CHECK(list.view() == reference.list);

    // Removing what isn't there changes nothing.
    list.remove(&pool[1]);
    list.remove(&pool[1]);
    reference.remove(&pool[1]);
    list.remove(nullptr);
    list.add(nullptr);
    CHECK(list.view() == reference.list);
  }
}

TEST_CASE("A dependent list matches a scanned list under churn", "[dependents]")
{
  // Few enough dependents that adds find duplicates and removes find holes,
  // and enough that the list crosses kIndexedFrom both ways.
  std::vector<Dependent> pool(300);
  std::mt19937 random(7);
  for (size_t spread : {size_t(20), size_t(60), size_t(300)})
  {
    INFO("spread " << spread);
    DependentList<Dependent> list;
    Reference reference;
    std::uniform_int_distribution<size_t> pick(0, spread - 1);
    for (int step = 0; step < 20000; step++)
    {
      auto dependent = &pool[pick(random)];
      // Biased to add, then to remove, so the size sweeps up and down.
      bool adding = (step / 2500) % 2 == 0 ? random() % 4 != 0
                                           : random() % 4 == 0;
      if (adding)
      {
        list.add(dependent);
        reference.add(dependent);
      }
      else
      {
        list.remove(dependent);
        reference.remove(dependent);
      }
      REQUIRE(list.size() == reference.list.size());
      if (step % 97 == 0)
      {
        REQUIRE(visited(list) == reference.list);
      }
      if (step % 389 == 0)
      {
        REQUIRE(list.view() == reference.list);
      }
    }
    CHECK(list.view() == reference.list);
  }
}

TEST_CASE("A dependent list copies what it holds, in order", "[dependents]")
{
  std::vector<Dependent> pool(100);
  DependentList<Dependent> list;
  for (auto& dependent : pool)
  {
    list.add(&dependent);
  }
  for (size_t i = 0; i < pool.size(); i += 2)
  {
    list.remove(&pool[i]);
  }
  DependentList<Dependent> copy(list);
  CHECK(copy.view() == list.view());
  // The copy is its own: adding to it leaves the original alone, and
  // finds what it already holds.
  copy.add(&pool[1]);
  copy.add(&pool[0]);
  CHECK(copy.size() == list.size() + 1);
  CHECK(copy.view().back() == &pool[0]);

  DependentList<Dependent> moved(std::move(copy));
  CHECK(moved.size() == list.size() + 1);
  CHECK(copy.empty());
  moved.clear();
  CHECK(moved.empty());
  CHECK(moved.view().empty());
}

// A walk visits every dependent once even when what it calls reads the list
// or adds to it while the list holds holes. Closing them midway would move
// the dependents not yet visited to lower slots, the walk's own among them.
TEST_CASE(
    "A dependent list's walk visits each once while holes close",
    "[dependents]")
{
  const size_t count = 200;
  std::vector<Dependent> pool(count * 4);
  DependentList<Dependent> list;
  Reference reference;
  for (size_t i = 0; i < count; i++)
  {
    list.add(&pool[i]);
    reference.add(&pool[i]);
  }
  // Holes in every third slot, too few for the list to close them itself.
  for (size_t i = 1; i < count; i += 3)
  {
    list.remove(&pool[i]);
    reference.remove(&pool[i]);
  }
  REQUIRE(list.size() == reference.list.size());
  const size_t midway = reference.list.size() / 2;
  std::vector<Dependent*> walked;
  // Runs once, midway, with holes behind the walk.
  auto walk = [&](std::function<void()> midwayCall) {
    list.forEach([&](Dependent* dependent) {
      walked.push_back(dependent);
      if (walked.size() == midway)
      {
        midwayCall();
      }
    });
  };

  SECTION("reading the list")
  {
    walk([&]() { CHECK(list.view().size() == reference.list.size()); });
    CHECK(walked == reference.list);
  }

  SECTION("copying the list")
  {
    walk([&]() {
      DependentList<Dependent> copy(list);
      CHECK(copy.size() == reference.list.size());
    });
    CHECK(walked == reference.list);
  }

  SECTION("adding to the list")
  {
    // Enough to fill the index twice over.
    walk([&]() {
      for (size_t i = count; i < count * 4; i++)
      {
        list.add(&pool[i]);
        reference.add(&pool[i]);
      }
    });
    // The ones added along the way are visited too, after the rest.
    CHECK(walked == reference.list);
  }
}

// Adding and removing tens of thousands of dependents takes no scan per
// call: done the old way, this many would take seconds.
TEST_CASE(
    "A dependent list adds and removes many without scanning",
    "[dependents]")
{
  const size_t count = 100000;
  std::vector<Dependent> pool(count);
  DependentList<Dependent> list;
  for (auto& dependent : pool)
  {
    list.add(&dependent);
  }
  // Each added again is found, not appended.
  for (auto& dependent : pool)
  {
    list.add(&dependent);
  }
  REQUIRE(list.size() == count);
  // Removed in the order added, the worst case for an erase.
  for (size_t i = 0; i + 1 < count; i++)
  {
    list.remove(&pool[i]);
  }
  CHECK(list.size() == 1);
  CHECK(list.view() == std::vector<Dependent*>{&pool[count - 1]});
}
