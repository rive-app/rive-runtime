/*
 * Copyright 2026 Rive
 */

// Instances of one file made on several threads at once. Android's worker
// thread instances list rows and data bound artboards from a file's source
// artboards while it advances, and an app can instance the same file's
// artboards on its main thread meanwhile.

#include "rive/artboard.hpp"
#include "rive/assets/file_asset.hpp"
#include "rive/file.hpp"
#include "rive/nested_artboard.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <atomic>
#include <thread>
#include <vector>

using namespace rive;

namespace
{
constexpr int kThreads = 4;

rcp<File> importBytes(const std::vector<uint8_t>& bytes)
{
  ImportResult result;
  rcp<File> file = File::import(bytes, &gNoOpFactory, &result);
  REQUIRE(result == ImportResult::success);
  REQUIRE(file != nullptr);
  return file;
}

// Runs [fn] on kThreads threads released together, so they meet on whatever
// the first instance of a source builds.
template <typename Fn> void runTogether(Fn fn)
{
  std::atomic<int> ready{0};
  std::vector<std::thread> threads;
  for (int t = 0; t < kThreads; t++)
  {
    threads.emplace_back([&, t] {
      ready.fetch_add(1);
      while (ready.load() < kThreads)
      {
      }
      fn(t);
    });
  }
  for (auto& thread : threads)
  {
    thread.join();
  }
}

// The core types of the components in the order [artboard] and the
// artboards nested in it update in.
void collectOrder(const Artboard* artboard, std::vector<uint16_t>& types)
{
  for (const Component* component : artboard->dependencyOrder())
  {
    types.push_back(component->coreType());
  }
  for (NestedArtboard* nested : artboard->nestedArtboards())
  {
    if (const Artboard* instance = nested->artboardInstance())
    {
      collectOrder(instance, types);
    }
  }
}

std::vector<uint16_t> updateOrder(const Artboard* artboard)
{
  std::vector<uint16_t> types;
  collectOrder(artboard, types);
  return types;
}

size_t referencerCount(const File* file)
{
  size_t count = 0;
  for (const rcp<FileAsset>& asset : file->assets())
  {
    count += asset->fileAssetReferencers().size();
  }
  return count;
}
} // namespace

TEST_CASE(
    "instances of one source made on several threads update alike",
    "[concurrent_instancing]")
{
  // Nested artboards, and no assets or scripts, so only the sources' order
  // recipes are shared between the threads.
  std::vector<uint8_t> bytes = ReadFile("assets/death_knight.riv");
  std::vector<uint16_t> expected = updateOrder(
      importBytes(bytes)->artboardDefault().get());
  REQUIRE(expected.size() > 100);

  int mismatches = 0;
  for (int i = 0; i < 50; i++)
  {
    // A fresh file each time, so no source has built its recipe yet.
    rcp<File> file = importBytes(bytes);
    REQUIRE(file->assets().size() == 0);
    std::vector<std::vector<uint16_t>> orders(kThreads);
    runTogether(
        [&](int t) { orders[t] = updateOrder(file->artboardDefault().get()); });
    for (const auto& order : orders)
    {
      mismatches += order != expected ? 1 : 0;
    }
  }
  CHECK(mismatches == 0);
}

TEST_CASE(
    "instances of one file made and destroyed on several threads keep its "
    "assets' referencers",
    "[concurrent_instancing]")
{
  // Images and a font, so each instance adds referencers to the file's
  // assets as it clones and removes them as it goes.
  std::vector<uint8_t> bytes = ReadFile("assets/superbowl.riv");
  {
    rcp<File> file = importBytes(bytes);
    size_t sourceOnly = referencerCount(file.get());
    auto instance = file->artboardDefault();
    REQUIRE(referencerCount(file.get()) > sourceOnly);
  }

  int mismatches = 0;
  for (int i = 0; i < 15; i++)
  {
    rcp<File> file = importBytes(bytes);
    size_t before = referencerCount(file.get());
    runTogether([&](int) {
      for (int n = 0; n < 4; n++)
      {
        auto instance = file->artboardDefault();
      }
    });
    mismatches += referencerCount(file.get()) != before ? 1 : 0;
  }
  CHECK(mismatches == 0);
}
