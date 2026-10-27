// Hidden benchmark for capture-phase listeners. Not part of the suite: build
// the tests in release and run ./out/release/unit_tests "[.capture_bench]".
// It compiles without the feature too, so copying it into an older tree
// gives a baseline. Three regimes per scene:
//   A  no machine anywhere has a capture listener
//   B  another file's machine has one (the pass must stay off in this tree)
//   C  this tree has one (its first pointer listener flipped to capture)
#include "rive/animation/nested_state_machine.hpp"
#include "rive/animation/state_machine.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/animation/state_machine_listener.hpp"
#include "rive/artboard_component_list.hpp"
#include "rive/nested_artboard.hpp"
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

#if __has_include("rive/listener_phase.hpp")
#include "rive/listener_phase.hpp"
#define HAS_CAPTURE 1
#else
#define HAS_CAPTURE 0
#endif

using namespace rive;

namespace
{
using Clock = std::chrono::steady_clock;

void forEachMachine(
    StateMachineInstance* smi,
    const std::function<void(StateMachineInstance*)>& visit)
{
  visit(smi);
  for (auto nested : smi->artboard()->nestedArtboards())
  {
    for (auto animation : nested->nestedAnimations())
    {
      if (animation->is<NestedStateMachine>())
      {
        if (auto child = animation->as<NestedStateMachine>()
                             ->stateMachineInstance())
        {
          forEachMachine(child, visit);
        }
      }
    }
  }
  for (auto list : smi->artboard()->artboardComponentLists())
  {
    for (int i : list->orderedListIndices())
    {
      if (auto child = list->stateMachineInstance(i))
      {
        forEachMachine(child, visit);
      }
    }
  }
}

rcp<File> load(const std::string& path)
{
  auto bytes = ReadFile(path.c_str());
  ImportResult result;
  auto file = File::import(bytes, &gNoOpFactory, &result);
  if (result != ImportResult::success || file == nullptr ||
      file->artboard() == nullptr)
  {
    return nullptr;
  }
  return file;
}

struct BenchInstance
{
  std::unique_ptr<ArtboardInstance> artboard;
  std::unique_ptr<StateMachineInstance> machine;

  explicit BenchInstance(File* file, const char* artboardName = nullptr)
  {
    artboard = artboardName != nullptr ? file->artboardNamed(artboardName)
                                       : file->artboardDefault();
    if (artboard == nullptr || artboard->stateMachineCount() == 0)
    {
      artboard = nullptr;
      return;
    }
    machine = artboard->stateMachineAt(0);
    // The first update builds the draw order, the next advance sorts the
    // hit components by it.
    machine->advanceAndApply(0.0f);
    machine->advanceAndApply(0.0f);
  }
};

#if HAS_CAPTURE
// Flips the first pointer listener of the file's artboard's first machine
// (or the named one) to the capture phase. Returns its name.
std::string captureFirstPointerListener(
    File* file,
    const char* artboardName,
    const char* listenerName = nullptr)
{
  Artboard* artboard = artboardName != nullptr ? file->artboard(artboardName)
                                               : file->artboard();
  if (artboard == nullptr || artboard->stateMachineCount() == 0)
  {
    return "";
  }
  auto machine = artboard->stateMachine(0);
  for (size_t i = 0; i < machine->listenerCount(); i++)
  {
    auto listener = machine->listener(i);
    if (!listener->hasPointerListeners())
    {
      continue;
    }
    if (listenerName != nullptr && listener->name() != listenerName)
    {
      continue;
    }
    const_cast<StateMachineListener*>(listener)->phaseValue(
        (uint32_t)ListenerPhase::capture);
    return listener->name().empty() ? "(unnamed)" : listener->name();
  }
  return "";
}
#endif

// Pointer moves over a grid across the artboard, in blocks with an untimed
// advance between them so reported events and hover work don't pile up.
// Returns the best of `runs` runs, in nanoseconds per event.
double timeMoves(StateMachineInstance* machine, int runs = 7)
{
  const float width = machine->artboard()->width();
  const float height = machine->artboard()->height();
  const int grid = 24;
  std::vector<Vec2D> points;
  for (int y = 0; y < grid; y++)
  {
    for (int x = 0; x < grid; x++)
    {
      float px = (x + 0.5f) * width / grid;
      float py = (y + 0.5f) * height / grid;
      // Serpentine, so consecutive moves stay close like a real drag.
      points.push_back(Vec2D(y % 2 == 0 ? px : width - px, py));
    }
  }
  double best = 1e30;
  for (int run = 0; run < runs; run++)
  {
    Clock::duration total{};
    size_t events = 0;
    for (int pass = 0; pass < 8; pass++)
    {
      for (size_t start = 0; start < points.size(); start += 64)
      {
        auto t0 = Clock::now();
        size_t end = std::min(points.size(), start + 64);
        for (size_t i = start; i < end; i++)
        {
          machine->pointerMove(points[i]);
        }
        total += Clock::now() - t0;
        events += end - start;
        machine->advanceAndApply(0.0f);
      }
    }
    double ns = std::chrono::duration<double, std::nano>(total).count() /
                events;
    best = std::min(best, ns);
  }
  return best;
}

// A press and release at the artboard's center, the gesture a claim hangs on.
double timeClicks(StateMachineInstance* machine, int runs = 7)
{
  Vec2D center(
      machine->artboard()->width() * 0.5f,
      machine->artboard()->height() * 0.5f);
  double best = 1e30;
  for (int run = 0; run < runs; run++)
  {
    Clock::duration total{};
    const int clicks = 512;
    for (int i = 0; i < clicks; i++)
    {
      auto t0 = Clock::now();
      machine->pointerDown(center);
      machine->pointerUp(center);
      total += Clock::now() - t0;
      if (i % 32 == 31)
      {
        machine->advanceAndApply(0.0f);
      }
    }
    double ns = std::chrono::duration<double, std::nano>(total).count() /
                (clicks * 2);
    best = std::min(best, ns);
  }
  return best;
}

// Instancing the artboard (which builds every nested machine) plus the root
// machine, then tearing both down.
double timeInit(File* file, const char* artboardName, int runs = 5)
{
  double best = 1e30;
  for (int run = 0; run < runs; run++)
  {
    const int count = 64;
    auto t0 = Clock::now();
    for (int i = 0; i < count; i++)
    {
      auto artboard = artboardName != nullptr
                          ? file->artboardNamed(artboardName)
                          : file->artboardDefault();
      auto machine = artboard->stateMachineAt(0);
    }
    double us = std::chrono::duration<double, std::micro>(Clock::now() - t0)
                    .count() /
                count;
    best = std::min(best, us);
  }
  return best;
}

// One frame of advanceAndApply, without input.
double timeAdvance(StateMachineInstance* machine, int runs = 7)
{
  double best = 1e30;
  for (int run = 0; run < runs; run++)
  {
    const int frames = 256;
    auto t0 = Clock::now();
    for (int i = 0; i < frames; i++)
    {
      machine->advanceAndApply(1.0f / 60.0f);
    }
    double us = std::chrono::duration<double, std::micro>(Clock::now() - t0)
                    .count() /
                frames;
    best = std::min(best, us);
  }
  return best;
}

struct BenchScene
{
  const char* path;
  const char* artboard;
  // For the capture-in-tree run on the feature's own fixture, a listener
  // that only watches moves, so nothing gets claimed mid-run.
  const char* captureListener;
};

const BenchScene kScenes[] = {
    {"assets/capture_listeners.riv", "Host", "Slot B Move"},
    {"assets/layoutstest_8-planets-grid.riv", nullptr, nullptr},
    {"assets/inventory_demo_test_v2.riv", nullptr, nullptr},
    {"assets/magic_alley_db_reduced_export.riv", nullptr, nullptr},
};
} // namespace

TEST_CASE("capture listener benchmark", "[.capture_bench]")
{
  printf("feature compiled in: %s\n", HAS_CAPTURE ? "yes" : "no");
  for (const auto& benchScene : kScenes)
  {
    auto file = load(benchScene.path);
    if (file == nullptr)
    {
      printf("%s: could not load\n", benchScene.path);
      continue;
    }
    size_t machines = 0, hits = 0;
    {
      BenchInstance scene(file.get(), benchScene.artboard);
      REQUIRE(scene.machine != nullptr);
      forEachMachine(scene.machine.get(), [&](StateMachineInstance* smi) {
        machines++;
        hits += smi->hitComponentsCount();
      });
    }
    printf(
        "\n%s (%zu machines, %zu hit components)\n",
        benchScene.path,
        machines,
        hits);
    printf("  init      %9.2f us\n", timeInit(file.get(), benchScene.artboard));
    {
      BenchInstance scene(file.get(), benchScene.artboard);
      printf("  advance   %9.2f us/frame\n", timeAdvance(scene.machine.get()));
      printf(
          "  A no capture anywhere   move %8.1f ns  click %8.1f ns\n",
          timeMoves(scene.machine.get()),
          timeClicks(scene.machine.get()));
    }
#if HAS_CAPTURE
    {
      // Another file's machine with a capture listener stays alive, which
      // turns the capture phase on process-wide.
      auto other = load("assets/capture_listeners.riv");
      REQUIRE(other != nullptr);
      captureFirstPointerListener(other.get(), "Host", "Slot B Move");
      BenchInstance elsewhere(other.get(), "Host");
      REQUIRE(StateMachineInstance::liveCaptureMachines() == 1);
      BenchInstance scene(file.get(), benchScene.artboard);
      printf(
          "  B capture elsewhere     move %8.1f ns  click %8.1f ns\n",
          timeMoves(scene.machine.get()),
          timeClicks(scene.machine.get()));
    }
    {
      auto captured = load(benchScene.path);
      auto name = captureFirstPointerListener(
          captured.get(),
          benchScene.artboard,
          benchScene.captureListener);
      if (name.empty())
      {
        printf("  C capture in tree       (no pointer listener)\n");
      }
      else
      {
        printf(
            "  init with capture %9.2f us\n",
            timeInit(captured.get(), benchScene.artboard));
        BenchInstance scene(captured.get(), benchScene.artboard);
        printf(
            "  C capture in tree       move %8.1f ns  click %8.1f ns"
            "  (%s)\n",
            timeMoves(scene.machine.get()),
            timeClicks(scene.machine.get()),
            name.c_str());
      }
    }
#endif
  }
}
