#include "catch.hpp"
#include "rive/animation/state_machine_input_instance.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/async/work_pool.hpp"
#include "rive_file_reader.hpp"
#include <chrono>
#include <cstdio>
#include <thread>

// A settled state machine whose scripts have async work out keeps reporting
// "keep going", so hosts keep polling it in (see async_work_host_test), but
// its advance has nothing to do until the work lands or something changes.
// These cover the frames advanceAndApply skips, and every way out of them.
// With StateMachineInstance::sm_verifyAsyncSkips on (the default), each
// skipped advance runs anyway and aborts if it did work, so a change that
// fails to wake the machine stops the run rather than passing. Only state
// machines skip: a finished LinearAnimationInstance advances every frame.

#if defined(WITH_RIVE_SCRIPTING_LUAU) && defined(WITH_RIVE_SCRIPTNET)
#include "rive/artboard_component_list.hpp"
#include "rive/custom_property_number.hpp"
#include "rive/lua/rive_lua_libs.hpp"
#include "rive/lua/scripting_vm.hpp"
#include "rive/nested_artboard.hpp"
#include "rive/node.hpp"
#include "rive/scriptnet/net.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "utils/no_op_renderer.hpp"

using namespace rive;

namespace
{
// Holds every request in flight until a test settles it by hand.
class HoldingProvider : public scriptnet::Provider
{
public:
  std::vector<scriptnet::RequestId> starts;
  void start(scriptnet::RequestId id, const scriptnet::HttpRequest&) override
  {
    starts.push_back(id);
  }
};

// Declared before the file, so the file's VM closes (cancelling its
// requests) while the provider is still installed.
struct HeldNetwork
{
  rcp<HoldingProvider> provider = make_rcp<HoldingProvider>();
  HeldNetwork()
  {
    scriptnet::setProvider(provider);
    scriptnet::setLimits(scriptnet::NetLimits());
  }
  ~HeldNetwork()
  {
    scriptnet::setProvider(nullptr);
    scriptnet::setLimits(scriptnet::NetLimits());
  }
};

// Stands in for a script's fetch: onResponse runs where a promise's
// continuation would, inside the poll at the top of advanceAndApply.
class TestListener : public scriptnet::FetchListener
{
public:
  bool responded = false;
  std::function<void()> onResponded;
  void onResponse(scriptnet::HttpResponse&&) override
  {
    responded = true;
    if (onResponded)
    {
      onResponded();
    }
  }
  void onError(const scriptnet::NetError&) override {}
};

void drainGlobalAsyncWork()
{
  for (int i = 0; i < 100 && rive_hasPendingAsyncWork(); i++)
  {
    rive_pollAsyncWork();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  REQUIRE_FALSE(rive_hasPendingAsyncWork());
}

uint64_t addScriptingVM(File* file)
{
  file->setScriptingVM(make_rcp<ScriptingVM>(
      std::make_unique<CPPRuntimeScriptingContext>(&gNoOpFactory)));
  return file->scriptingVM()->context()->acquireOwnerId();
}

rcp<TestListener> startFetch(uint64_t owner)
{
  auto listener = make_rcp<TestListener>();
  scriptnet::HttpRequest request;
  request.url = "https://example.com/data";
  scriptnet::fetch(owner, std::move(request), listener);
  return listener;
}

// Answers the request from another thread, as a transport would.
void completeFromTransport(scriptnet::RequestId id)
{
  std::thread([id] {
    scriptnet::HttpResponse response;
    response.status = 200;
    response.url = "https://example.com/data";
    scriptnet::complete(id, std::move(response));
  }).join();
}

void settle(StateMachineInstance* scene)
{
  bool settled = false;
  for (int frame = 0; frame < 200 && !settled; frame++)
  {
    settled = !scene->advanceAndApply(0.1f);
  }
  REQUIRE(settled);
}

// One frame, and whether advanceAndApply skipped its advance.
bool frameSkipped(StateMachineInstance* scene)
{
  auto skips = StateMachineInstance::sm_asyncSkips;
  // Pending work always reads as "keep going", skipped or not.
  REQUIRE(scene->advanceAndApply(0.1f));
  return StateMachineInstance::sm_asyncSkips != skips;
}

// After a wake, frames until the scene settles again, with the work still
// out, and skips.
void skipAgain(StateMachineInstance* scene)
{
  bool skipped = false;
  for (int frame = 0; frame < 200 && !skipped; frame++)
  {
    skipped = frameSkipped(scene);
  }
  REQUIRE(skipped);
}

// The frame after the fetch starts settles again, now with work out, and
// arms; the frames after that skip.
void awaitAsync(StateMachineInstance* scene)
{
  CHECK_FALSE(frameSkipped(scene));
  for (int frame = 0; frame < 3; frame++)
  {
    CHECK(frameSkipped(scene));
  }
}
} // namespace

TEST_CASE(
    "a settled machine skips its advance while only a fetch is in flight",
    "[async_skip]")
{
  drainGlobalAsyncWork();
  HeldNetwork network;
  auto file = ReadRiveFile("assets/ball_test.riv");
  uint64_t owner = addScriptingVM(file.get());
  auto artboard = file->artboardNamed("Artboard 2");
  auto stateMachine = artboard->stateMachineAt(0);
  settle(stateMachine.get());

  auto listener = startFetch(owner);
  REQUIRE(network.provider->starts.size() == 1);
  awaitAsync(stateMachine.get());
  CHECK_FALSE(listener->responded);

  completeFromTransport(network.provider->starts[0]);
  // The poll delivers, nothing is out any more, and the advance runs and
  // settles: the host may stop.
  auto skips = StateMachineInstance::sm_asyncSkips;
  CHECK_FALSE(stateMachine->advanceAndApply(0.1f));
  CHECK(listener->responded);
  CHECK(StateMachineInstance::sm_asyncSkips == skips);
}

TEST_CASE(
    "a fetch's outcome wakes the machine to apply what it changed",
    "[async_skip]")
{
  drainGlobalAsyncWork();
  HeldNetwork network;
  auto file = ReadRiveFile("assets/data_binding_test_2.riv");
  uint64_t owner = addScriptingVM(file.get());
  auto artboard = file->artboardNamed("artboard-2");
  auto vmi = file->createDefaultViewModelInstance(artboard.get());
  auto stateMachine = artboard->defaultStateMachine();
  stateMachine->bindViewModelInstance(vmi);
  settle(stateMachine.get());
  auto mapped = artboard->find<CustomPropertyNumber>("mapped-range-1");
  REQUIRE(mapped != nullptr);
  REQUIRE(mapped->propertyValue() == 6.0f);

  auto listener = startFetch(owner);
  // As a continuation writing what it fetched into the view model.
  listener->onResponded = [&] {
    vmi->propertyValue("map-range-num")
        ->as<ViewModelInstanceNumber>()
        ->propertyValue(-1.0f);
  };
  awaitAsync(stateMachine.get());

  completeFromTransport(network.provider->starts[0]);
  stateMachine->advanceAndApply(0.1f);
  CHECK(listener->responded);
  CHECK(mapped->propertyValue() == 1.0f);
}

TEST_CASE(
    "a view model write from the host wakes a machine awaiting async work",
    "[async_skip]")
{
  drainGlobalAsyncWork();
  HeldNetwork network;
  auto file = ReadRiveFile("assets/data_binding_test_2.riv");
  uint64_t owner = addScriptingVM(file.get());
  auto artboard = file->artboardNamed("artboard-2");
  auto vmi = file->createDefaultViewModelInstance(artboard.get());
  auto stateMachine = artboard->defaultStateMachine();
  stateMachine->bindViewModelInstance(vmi);
  settle(stateMachine.get());
  auto mapped = artboard->find<CustomPropertyNumber>("mapped-range-1");
  REQUIRE(mapped != nullptr);

  startFetch(owner);
  awaitAsync(stateMachine.get());

  vmi->propertyValue("map-range-num")
      ->as<ViewModelInstanceNumber>()
      ->propertyValue(-1.0f);
  CHECK_FALSE(frameSkipped(stateMachine.get()));
  CHECK(mapped->propertyValue() == 1.0f);
  // Settled on the new value, it goes back to skipping.
  awaitAsync(stateMachine.get());
}

TEST_CASE(
    "binding another view model instance wakes a machine awaiting async work",
    "[async_skip]")
{
  drainGlobalAsyncWork();
  HeldNetwork network;
  auto file = ReadRiveFile("assets/data_binding_test_2.riv");
  uint64_t owner = addScriptingVM(file.get());
  auto artboard = file->artboardNamed("artboard-2");
  auto stateMachine = artboard->defaultStateMachine();
  stateMachine->bindViewModelInstance(
      file->createDefaultViewModelInstance(artboard.get()));
  settle(stateMachine.get());
  auto mapped = artboard->find<CustomPropertyNumber>("mapped-range-1");
  REQUIRE(mapped != nullptr);

  startFetch(owner);
  awaitAsync(stateMachine.get());

  auto other = file->createDefaultViewModelInstance(artboard.get());
  other->propertyValue("map-range-num")
      ->as<ViewModelInstanceNumber>()
      ->propertyValue(-1.0f);
  stateMachine->bindViewModelInstance(other);
  CHECK_FALSE(frameSkipped(stateMachine.get()));
  CHECK(mapped->propertyValue() == 1.0f);
}

TEST_CASE(
    "inputs and pointer events wake a machine awaiting async work",
    "[async_skip]")
{
  drainGlobalAsyncWork();
  HeldNetwork network;
  auto file = ReadRiveFile("assets/opaque_hit_test.riv");
  uint64_t owner = addScriptingVM(file.get());
  auto artboard = file->artboardNamed("main");
  auto stateMachine = artboard->stateMachineNamed("main-state-machine");
  REQUIRE(stateMachine != nullptr);
  settle(stateMachine.get());
  auto toGreen = stateMachine->getBool("toGreen");
  REQUIRE(toGreen != nullptr);
  REQUIRE_FALSE(toGreen->value());

  startFetch(owner);
  awaitAsync(stateMachine.get());

  // A listener on the green target sets toGreen (see artboard_transform_test).
  stateMachine->pointerDown(Vec2D(100.0f, 250.0f));
  REQUIRE(toGreen->value());
  CHECK_FALSE(frameSkipped(stateMachine.get()));

  skipAgain(stateMachine.get());
  toGreen->value(false);
  CHECK_FALSE(frameSkipped(stateMachine.get()));
  skipAgain(stateMachine.get());
}

TEST_CASE(
    "a change inside a nested artboard wakes the root awaiting async work",
    "[async_skip]")
{
  drainGlobalAsyncWork();
  HeldNetwork network;
  auto file = ReadRiveFile("assets/ball_test.riv");
  uint64_t owner = addScriptingVM(file.get());
  auto artboard = file->artboardNamed("Artboard 2");
  auto stateMachine = artboard->stateMachineAt(0);
  settle(stateMachine.get());
  REQUIRE(!artboard->nestedArtboards().empty());
  auto nested = artboard->nestedArtboards()[0]->artboardInstance();
  REQUIRE(nested != nullptr);
  Node* node = nullptr;
  for (auto object : nested->objects())
  {
    if (object != nullptr && object != nested && object->is<Node>())
    {
      node = object->as<Node>();
      break;
    }
  }
  REQUIRE(node != nullptr);

  startFetch(owner);
  awaitAsync(stateMachine.get());

  node->x(node->x() + 10.0f);
  CHECK_FALSE(frameSkipped(stateMachine.get()));
}

TEST_CASE(
    "resizing the artboard wakes a machine awaiting async work",
    "[async_skip]")
{
  drainGlobalAsyncWork();
  HeldNetwork network;
  auto file = ReadRiveFile("assets/ball_test.riv");
  uint64_t owner = addScriptingVM(file.get());
  auto artboard = file->artboardNamed("Artboard 2");
  auto stateMachine = artboard->stateMachineAt(0);
  settle(stateMachine.get());

  startFetch(owner);
  awaitAsync(stateMachine.get());

  artboard->width(artboard->width() + 10.0f);
  CHECK_FALSE(frameSkipped(stateMachine.get()));
}

TEST_CASE(
    "another root animating on the same thread doesn't wake one awaiting "
    "async work",
    "[async_skip]")
{
  drainGlobalAsyncWork();
  HeldNetwork network;
  auto file = ReadRiveFile("assets/ball_test.riv");
  uint64_t owner = addScriptingVM(file.get());
  auto waiting = file->artboardNamed("Artboard 2");
  auto waitingMachine = waiting->stateMachineAt(0);
  settle(waitingMachine.get());
  // Its nested animation loops, so it never settles (see nested_artboard_test).
  auto busy = file->artboardNamed("Artboard");
  auto busyMachine = busy->stateMachineAt(0);

  startFetch(owner);
  awaitAsync(waitingMachine.get());
  for (int frame = 0; frame < 5; frame++)
  {
    CHECK(busyMachine->advanceAndApply(0.1f));
    CHECK(frameSkipped(waitingMachine.get()));
  }
}

// A scripted condition reads script state that changes without dirt, so its
// layers search every frame, and a skipped frame would miss it turning true.
TEST_CASE(
    "a machine with scripted transition conditions never skips",
    "[async_skip]")
{
  drainGlobalAsyncWork();
  HeldNetwork network;
  auto file = ReadRiveFile("assets/scripted_transition_condition.riv");
  REQUIRE(file->scriptingVM() != nullptr);
  uint64_t owner = file->scriptingVM()->context()->acquireOwnerId();
  auto artboard = file->artboardDefault();
  auto stateMachine = artboard->stateMachineAt(0);
  stateMachine->bindViewModelInstance(
      file->createViewModelInstance(artboard.get()));
  REQUIRE(stateMachine->ownsScriptedConditions());
  settle(stateMachine.get());

  startFetch(owner);
  for (int frame = 0; frame < 4; frame++)
  {
    CHECK_FALSE(frameSkipped(stateMachine.get()));
  }
}

// The quiet mark is the artboard's: a second machine on it settles again
// after a change and sets the mark back before the first has seen it.
TEST_CASE(
    "another machine on the artboard can't hide a change from one awaiting "
    "async work",
    "[async_skip]")
{
  drainGlobalAsyncWork();
  HeldNetwork network;
  auto file = ReadRiveFile("assets/ball_test.riv");
  uint64_t owner = addScriptingVM(file.get());
  auto artboard = file->artboardNamed("Artboard 2");
  auto first = artboard->stateMachineAt(0);
  auto second = artboard->stateMachineAt(0);
  settle(first.get());
  settle(second.get());
  Node* node = nullptr;
  for (auto object : artboard->objects())
  {
    if (object != nullptr && object != artboard.get() && object->is<Node>())
    {
      node = object->as<Node>();
      break;
    }
  }
  REQUIRE(node != nullptr);

  startFetch(owner);
  awaitAsync(first.get());
  awaitAsync(second.get());

  node->x(node->x() + 10.0f);
  skipAgain(second.get());
  CHECK(artboard->quietAwaitingAsync());
  CHECK_FALSE(frameSkipped(first.get()));
}

TEST_CASE("a machine that settles with nothing out doesn't arm", "[async_skip]")
{
  drainGlobalAsyncWork();
  auto file = ReadRiveFile("assets/ball_test.riv");
  addScriptingVM(file.get());
  auto artboard = file->artboardNamed("Artboard 2");
  auto stateMachine = artboard->stateMachineAt(0);
  settle(stateMachine.get());
  auto skips = StateMachineInstance::sm_asyncSkips;
  for (int frame = 0; frame < 3; frame++)
  {
    CHECK_FALSE(stateMachine->advanceAndApply(0.1f));
  }
  CHECK(StateMachineInstance::sm_asyncSkips == skips);
}

// What a frame costs a settled machine while a fetch is out, skipping or not.
// With the verifier on, a skipped frame still runs the whole advance, which
// is what every such frame cost before the skip.
TEST_CASE(
    "bench: a settled machine's frames while a fetch is out",
    "[.][async_skip_bench]")
{
  struct Saved
  {
    bool verifyAsync = StateMachineInstance::sm_verifyAsyncSkips;
    bool verifyRows = ArtboardComponentList::sm_verifyQuietRows;
    ~Saved()
    {
      StateMachineInstance::sm_verifyAsyncSkips = verifyAsync;
      ArtboardComponentList::sm_verifyQuietRows = verifyRows;
    }
  } saved;
  ArtboardComponentList::sm_verifyQuietRows = false;
  constexpr int kFrames = 2000;
  for (const char* asset :
       {"assets/ball_test.riv",
        "assets/data_binding_test_2.riv",
        "assets/car_widgets_v01.riv",
        "assets/echo_show_demo.riv",
        "assets/game_menu_ad_police_files.riv",
        "assets/zombie_skins.riv"})
  {
    drainGlobalAsyncWork();
    HeldNetwork network;
    auto file = ReadRiveFile(asset);
    uint64_t owner = addScriptingVM(file.get());
    auto artboard = file->artboardDefault();
    auto stateMachine = artboard->defaultStateMachine();
    if (stateMachine == nullptr)
    {
      continue;
    }
    bool settled = false;
    for (int frame = 0; frame < 600 && !settled; frame++)
    {
      settled = !stateMachine->advanceAndApply(1.0f / 60.0f);
    }
    if (!settled)
    {
      printf("%-40s never settles\n", asset);
      continue;
    }
    startFetch(owner);
    stateMachine->advanceAndApply(1.0f / 60.0f);
    auto timeFrames = [&](bool verify) {
      StateMachineInstance::sm_verifyAsyncSkips = verify;
      auto skips = StateMachineInstance::sm_asyncSkips;
      auto start = std::chrono::steady_clock::now();
      for (int frame = 0; frame < kFrames; frame++)
      {
        stateMachine->advanceAndApply(1.0f / 60.0f);
      }
      double us = std::chrono::duration<double, std::micro>(
                      std::chrono::steady_clock::now() - start)
                      .count() /
                  kFrames;
      CHECK(StateMachineInstance::sm_asyncSkips - skips == kFrames);
      return us;
    };
    double before = timeFrames(true);
    double after = timeFrames(false);
    // The draw a host still records each frame unless it checks for change,
    // without any GPU work.
    NoOpRenderer renderer;
    auto start = std::chrono::steady_clock::now();
    for (int frame = 0; frame < kFrames; frame++)
    {
      artboard->draw(&renderer);
    }
    double draw = std::chrono::duration<double, std::micro>(
                      std::chrono::steady_clock::now() - start)
                      .count() /
                  kFrames;
    printf(
        "%-40s full advance %8.2f us/frame, skipped %6.2f us/frame, "
        "draw (no-op renderer) %8.2f us/frame\n",
        asset,
        before,
        after,
        draw);
  }
}
#endif
