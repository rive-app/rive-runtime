#include "rive/file.hpp"
#include "rive/backboard.hpp"
#include "rive/nested_artboard.hpp"
#include "rive/node.hpp"
#include "rive/focus_data.hpp"
#include "rive/input/focus_manager.hpp"
#include "rive/nested_artboard_layout.hpp"
#include "rive/layout_component.hpp"
#include "rive/layout/layout_component_style.hpp"
#include "rive/artboard_component_list.hpp"
#include "rive/solo.hpp"
#include "rive/shapes/paint/paint_image.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/viewmodel_instance_list_item.hpp"
#include "rive/viewmodel/viewmodel_instance_trigger.hpp"
#include "rive/animation/nested_bool.hpp"
#include "rive/animation/nested_trigger.hpp"
#include "rive/animation/nested_state_machine.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/animation/state_machine_input_instance.hpp"
#include "rive/shapes/paint/fill.hpp"
#include "rive/shapes/paint/solid_color.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "rive_file_reader.hpp"
#include "utils/svg_factory.hpp"
#include "utils/svg_renderer.hpp"
#include <catch.hpp>
#include <algorithm>
#include <cstdlib>
#include <tuple>

using namespace rive;

constexpr uint8_t kDeferCollapsed = static_cast<uint8_t>(
    BackboardRuntimeFlags::deferCollapsedNestedArtboards);

// RIVE_DEFER_COLLAPSED_NESTED=0 runs every test that imports with the default
// options as if each file's Backboard cleared deferCollapsedNestedArtboards
// (which is on by default); =1 as if each set it.
static bool g_deferCollapsedNestedFromEnvironment = [] {
  const char* value = getenv("RIVE_DEFER_COLLAPSED_NESTED");
  if (value != nullptr && (value[0] == '0' || value[0] == '1'))
  {
    File::testing_defaultImportOptions
        .testing_runtimeFlags = value[0] == '1' ? kDeferCollapsed : 0;
  }
  return true;
}();

namespace
{
// The assets predate 7.6, which deferring needs, so the testing override
// decides it whatever their header and Backboard say.
rcp<File> importWith(const char* path, Factory* factory, bool defer)
{
  std::vector<uint8_t> bytes = ReadFile(path);
  FileImportOptions options;
  options.testing_runtimeFlags = defer ? kDeferCollapsed : 0;
  ImportResult result;
  auto file = File::import(
      bytes,
      factory,
      &result,
      nullptr,
      nullptr,
      false,
      options);
  REQUIRE(result == ImportResult::success);
  REQUIRE(file != nullptr);
  CHECK(file->defersCollapsedNestedArtboards() == defer);
  return file;
}

rcp<File> importDeferring(const char* path)
{
  return importWith(path, &gNoOpFactory, true);
}

std::string drawn(ArtboardInstance* artboard)
{
  SVGRenderer renderer;
  artboard->draw(&renderer);
  return renderer.finalize((int)artboard->width(), (int)artboard->height());
}

SolidColor* rectangleColor(ArtboardInstance* artboard)
{
  auto rectangle = artboard->find<Shape>("Clickable-Rectangle");
  REQUIRE(rectangle != nullptr);
  return rectangle->children()[1]->as<Fill>()->paint()->as<SolidColor>();
}
} // namespace

// assets/pointer_events_nested_artboards_in_solos.riv: a Solo whose timeline
// swaps "Nested-Artboard-Active" and "Nested-Artboard-Inactive", each a
// rectangle a pointer turns from green to red (see "hit test on nested
// artboards in solos").
TEST_CASE(
    "a collapsed nested artboard is instanced the first time it is shown",
    "[deferredNested]")
{
  auto file = importDeferring(
      "assets/pointer_events_nested_artboards_in_solos.riv");
  auto artboard = file->artboard()->instance();
  auto active = artboard->find<NestedArtboard>("Nested-Artboard-Active");
  auto inactive = artboard->find<NestedArtboard>("Nested-Artboard-Inactive");
  REQUIRE(active != nullptr);
  REQUIRE(inactive != nullptr);
  CHECK(active->artboardInstance() != nullptr);
  CHECK_FALSE(active->isInstanceDeferred());
  CHECK(inactive->artboardInstance() == nullptr);
  CHECK(inactive->isInstanceDeferred());

  auto stateMachine = artboard->stateMachineAt(0);
  REQUIRE(stateMachine != nullptr);
  stateMachine->advance(0.0f);
  artboard->advance(0.0f);
  CHECK(inactive->isCollapsed());
  CHECK(inactive->artboardInstance() == nullptr);

  // The timeline shows it: it mounts, with its state machine.
  stateMachine->advance(0.1f);
  artboard->advance(0.1f);
  REQUIRE_FALSE(inactive->isCollapsed());
  auto mounted = inactive->artboardInstance();
  REQUIRE(mounted != nullptr);
  CHECK_FALSE(inactive->isInstanceDeferred());
  REQUIRE(inactive->nestedAnimations().size() == 1);
  CHECK(
      inactive->nestedAnimations()[0]
          ->as<NestedStateMachine>()
          ->stateMachineInstance() != nullptr);
  auto green = 0xFF00B511;
  auto red = 0xFFC80000;
  CHECK(rectangleColor(mounted)->colorValue() == green);

  // Hidden again, it keeps what it mounted.
  stateMachine->advance(0.1f);
  artboard->advance(0.1f);
  CHECK(inactive->isCollapsed());
  CHECK(inactive->artboardInstance() == mounted);

  // The pointer reaches the shown one only, as when both were instanced up
  // front.
  stateMachine->pointerUp(Vec2D(200.0f, 200.0f));
  stateMachine->advance(0.0f);
  artboard->advance(0.0f);
  stateMachine->advance(0.1f);
  artboard->advance(0.1f);
  CHECK(rectangleColor(active->artboardInstance())->colorValue() == red);
  CHECK(rectangleColor(mounted)->colorValue() == green);
}

// assets/collapsed_nested_databind.riv: a Solo bound to `soloIndex` whose
// option [1] holds bodyB (see "a collapsed nested artboard does not write to
// its source").
TEST_CASE(
    "a deferred nested artboard mounts when a bound solo shows it",
    "[deferredNested]")
{
  auto file = importDeferring("assets/collapsed_nested_databind.riv");
  auto artboard = file->artboardDefault();
  auto stateMachine = artboard->stateMachineAt(0);
  auto vmi = file->createDefaultViewModelInstance(artboard.get());
  stateMachine->bindViewModelInstance(vmi);
  auto hosts = artboard->find<NestedArtboard>();
  REQUIRE(hosts.size() == 1);
  auto host = hosts[0];
  CHECK(host->isInstanceDeferred());

  auto number = [&](const char* name) {
    return vmi->propertyValue(name)
        ->as<ViewModelInstanceNumber>()
        ->propertyValue();
  };
  auto setNumber = [&](const char* name, float value) {
    vmi->propertyValue(name)->as<ViewModelInstanceNumber>()->propertyValue(
        value);
  };
  setNumber("feed", 55.0f);
  for (int i = 0; i < 10; i++)
  {
    stateMachine->advanceAndApply(0.016f);
  }
  CHECK(host->artboardInstance() == nullptr);
  CHECK(number("neckY") == Approx(1234.0f));
  CHECK(number("deepY") == Approx(4321.0f));
  CHECK(number("echo") == Approx(777.0f));

  // Shown, it mounts bound to the context it was handed while it had
  // nothing to pass it to.
  setNumber("soloIndex", 1.0f);
  for (int i = 0; i < 10; i++)
  {
    stateMachine->advanceAndApply(0.016f);
  }
  CHECK(host->artboardInstance() != nullptr);
  CHECK(number("neckY") == Approx(-70.0f));
  CHECK(number("deepY") == Approx(-42.0f));
  CHECK(number("echo") == Approx(55.0f));

  setNumber("soloIndex", 0.0f);
  for (int i = 0; i < 10; i++)
  {
    stateMachine->advanceAndApply(0.016f);
  }
  CHECK(host->artboardInstance() != nullptr);
  CHECK(number("neckY") == Approx(-70.0f));
  CHECK(number("deepY") == Approx(-42.0f));
  CHECK(number("echo") == Approx(55.0f));
}

// assets/nested_event_test.riv: a pointer on the nested artboard fires an
// event that a listener on the main artboard turns into "Boolean 1" (see
// "events from a nested artboard propagate to a listener on a parent").
TEST_CASE(
    "a deferred nested artboard's events reach the machine above it",
    "[deferredNested]")
{
  auto file = importDeferring("assets/nested_event_test.riv");
  // Collapsed in the source, the host defers in every instance.
  auto sourceHosts = file->artboard()->find<NestedArtboard>();
  REQUIRE(sourceHosts.size() == 1);
  sourceHosts[0]->collapse(true);

  auto artboard = file->artboard()->instance();
  auto host = artboard->find<NestedArtboard>()[0];
  REQUIRE(host->isInstanceDeferred());
  // Created while its host has nothing to listen to.
  auto stateMachine = artboard->stateMachineAt(0);
  auto input = stateMachine->getBool("Boolean 1");
  REQUIRE(input != nullptr);
  REQUIRE(input->value() == false);

  // Nothing collapses the instance's host, so its first advance mounts it.
  artboard->advance(0.0f);
  stateMachine->advance(0.0f);
  REQUIRE(host->artboardInstance() != nullptr);
  auto nestedStateMachine = host->nestedAnimations()[0]
                                ->as<NestedStateMachine>()
                                ->stateMachineInstance();
  REQUIRE(nestedStateMachine != nullptr);

  stateMachine->pointerDown(Vec2D(250.0f, 100.0f));
  REQUIRE(nestedStateMachine->reportedEventCount() == 1);
  artboard->advance(0.0f);
  CHECK(input->value() == true);
}

// assets/runtime_nested_inputs.riv: "CircleOuter" hosts a machine with the
// boolean "CircleOuterState" (see "validate nested boolean get/set").
TEST_CASE(
    "a nested input set before its host mounts applies when it does",
    "[deferredNested]")
{
  auto file = importDeferring("assets/runtime_nested_inputs.riv");
  file->artboard("MainArtboard")
      ->find<NestedArtboard>("CircleOuter")
      ->collapse(true);

  auto artboard = file->artboard("MainArtboard")->instance();
  // find, unlike nestedArtboard(name), doesn't mount it.
  auto host = artboard->find<NestedArtboard>("CircleOuter");
  REQUIRE(host != nullptr);
  REQUIRE(host->isInstanceDeferred());
  // A nested input's name comes from its machine, which a deferred host
  // hasn't made, so it is found by type here (as a keyframe reaches it).
  NestedBool* nestedInput = nullptr;
  for (auto animation : host->nestedAnimations())
  {
    auto machine = animation->as<NestedStateMachine>();
    for (size_t i = 0; nestedInput == nullptr && machine->input(i) != nullptr;
         i++)
    {
      if (machine->input(i)->is<NestedBool>())
      {
        nestedInput = machine->input(i)->as<NestedBool>();
      }
    }
  }
  REQUIRE(nestedInput != nullptr);
  REQUIRE(host->isInstanceDeferred());
  CHECK(nestedInput->nestedValue() == false);
  nestedInput->nestedValue(true);
  CHECK(nestedInput->nestedValue() == true);

  // Looked up by path, the host mounts, and its machine starts with the
  // value it was set to.
  auto boolInput = artboard->getBool("CircleOuterState", "CircleOuter");
  REQUIRE(boolInput != nullptr);
  CHECK_FALSE(host->isInstanceDeferred());
  CHECK(nestedInput->name() == "CircleOuterState");
  CHECK(boolInput->value() == true);
  CHECK(nestedInput->nestedValue() == true);
}

// A collapsed host's machine holds a trigger until it next advances; a
// deferred host holds it until it creates the machine.
TEST_CASE(
    "a nested trigger fired before its host mounts fires when it does",
    "[deferredNested]")
{
  auto file = importDeferring("assets/runtime_nested_inputs.riv");
  file->artboard("MainArtboard")
      ->find<NestedArtboard>("CircleOuter")
      ->collapse(true);

  auto artboard = file->artboard("MainArtboard")->instance();
  auto host = artboard->find<NestedArtboard>("CircleOuter");
  REQUIRE(host->isInstanceDeferred());
  NestedTrigger* nestedTrigger = nullptr;
  for (auto animation : host->nestedAnimations())
  {
    auto machine = animation->as<NestedStateMachine>();
    for (size_t i = 0; nestedTrigger == nullptr && machine->input(i) != nullptr;
         i++)
    {
      if (machine->input(i)->is<NestedTrigger>())
      {
        nestedTrigger = machine->input(i)->as<NestedTrigger>();
      }
    }
  }
  REQUIRE(nestedTrigger != nullptr);
  nestedTrigger->applyValue();
  REQUIRE(host->isInstanceDeferred());

  auto trigger = artboard->getTrigger("CircleOuterTrigger", "CircleOuter");
  REQUIRE(trigger != nullptr);
  CHECK(trigger->didFire());
}

TEST_CASE(
    "a nested artboard or input looked up by name mounts its host",
    "[deferredNested]")
{
  auto file = importDeferring("assets/runtime_nested_inputs.riv");
  file->artboard("MainArtboard")
      ->find<NestedArtboard>("CircleOuter")
      ->collapse(true);

  auto artboard = file->artboard("MainArtboard")->instance();
  // find, unlike nestedArtboard(name), doesn't mount it.
  auto host = artboard->find<NestedArtboard>("CircleOuter");
  REQUIRE(host->isInstanceDeferred());
  auto nestedInput = host->input("CircleOuterState");
  REQUIRE(nestedInput != nullptr);
  CHECK_FALSE(host->isInstanceDeferred());
  CHECK(nestedInput->input() != nullptr);

  auto other = file->artboard("MainArtboard")->instance();
  auto otherHost = other->find<NestedArtboard>("CircleOuter");
  REQUIRE(otherHost->isInstanceDeferred());
  CHECK(other->nestedArtboard("CircleOuter") == otherHost);
  CHECK_FALSE(otherHost->isInstanceDeferred());
  CHECK(otherHost->artboardInstance() != nullptr);
}

// The asset's header says 7.0. Its minor version (the byte after the major,
// both one byte varuints after "RIVE") is set to File::minorVersion to read it
// as a file the current editor exported.
TEST_CASE(
    "files from 7.6 defer by default, unless their Backboard clears it; "
    "older files instance every nested artboard up front",
    "[deferredNested]")
{
  auto load = [](bool current, bool clearFlag) {
    std::vector<uint8_t> bytes = ReadFile(
        "assets/pointer_events_nested_artboards_in_solos.riv");
    REQUIRE(bytes.size() > 6);
    // Copied: the in-class constants have no definition to bind to.
    const int major = File::majorVersion;
    const int minor = File::minorVersion;
    REQUIRE(bytes[4] == major);
    REQUIRE(bytes[5] < 6);
    if (current)
    {
      bytes[5] = static_cast<uint8_t>(minor);
    }
    // Not the testing defaults, which RIVE_DEFER_COLLAPSED_NESTED sets.
    FileImportOptions options;
    auto file = File::import(
        bytes,
        &gNoOpFactory,
        nullptr,
        nullptr,
        nullptr,
        false,
        options);
    REQUIRE(file != nullptr);
    REQUIRE(file->backboard() != nullptr);
    // The asset predates the property, so it reads the default.
    CHECK(file->backboard()->runtimeFlags() == kDeferCollapsed);
    if (clearFlag)
    {
      file->backboard()->runtimeFlags(0);
    }
    auto artboard = file->artboard()->instance();
    auto hidden = artboard->find<NestedArtboard>("Nested-Artboard-Inactive");
    REQUIRE(hidden != nullptr);
    return hidden->isInstanceDeferred();
  };
  CHECK_FALSE(load(false, false));
  CHECK(load(true, false));
  CHECK_FALSE(load(true, true));
}

// The scenes of the silvers "Uncollapse and focus element on the same action"
// (focus_test.cpp) and "Multiple layout collapsing and soloing in hierarchy."
// (layout_test.cpp), drawn with and without deferring. Their silvers record
// the factory's calls in order, which mounting later reorders, so the
// drawings are compared instead.
TEST_CASE("deferring draws what instancing up front draws", "[deferredNested]")
{
  auto uncollapseAndFocus = [](bool defer) {
    SVGFactory factory;
    auto file = importWith("assets/gamepad_inputs_test.riv", &factory, defer);
    auto artboard = file->artboardNamed("UncollapsedParent");
    auto stateMachine = artboard->stateMachineAt(0);
    auto vmi = file->createDefaultViewModelInstance(artboard.get());
    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.0f);
    std::vector<std::string> frames{drawn(artboard.get())};
    for (int i = 0; i < 2; i++)
    {
      stateMachine->pointerDown(Vec2D(475.0f, 475.0f));
      stateMachine->pointerUp(Vec2D(475.0f, 475.0f));
      stateMachine->advanceAndApply(0.016f);
      frames.push_back(drawn(artboard.get()));
    }
    return frames;
  };
  auto eager = uncollapseAndFocus(false);
  auto deferred = uncollapseAndFocus(true);
  REQUIRE(eager.size() == deferred.size());
  for (size_t i = 0; i < eager.size(); i++)
  {
    INFO("gamepad_inputs_test frame " << i);
    CHECK(eager[i] == deferred[i]);
  }

  auto collapsing = [](bool defer) {
    SVGFactory factory;
    auto file = importWith("assets/collapsing_elements.riv", &factory, defer);
    auto artboard = file->artboardDefault();
    auto stateMachine = artboard->stateMachineAt(0);
    stateMachine->advanceAndApply(0.1f);
    std::vector<std::string> frames{drawn(artboard.get())};
    for (int i = 0; i < (int)(4.0f / 0.016f); i++)
    {
      stateMachine->advanceAndApply(0.016f);
      frames.push_back(drawn(artboard.get()));
    }
    return frames;
  };
  auto eagerLayout = collapsing(false);
  auto deferredLayout = collapsing(true);
  REQUIRE(eagerLayout.size() == deferredLayout.size());
  size_t differing = 0;
  for (size_t i = 0; i < eagerLayout.size(); i++)
  {
    differing += eagerLayout[i] != deferredLayout[i] ? 1 : 0;
  }
  CHECK(differing == 0);
}

// assets/quiet_inactive_hosts.riv's "Paint Rows": a row's Solo "Switch" starts
// on an option without "Flasher", which flashes on the view model trigger
// `flash` (see "Quiet rows with collapsed artboard paints don't replay old
// changes"). A flash fired while it is hidden has had its frame, and doesn't
// play once it shows; deferred, the Flasher mounts only when it shows, and
// has to agree.
TEST_CASE(
    "a trigger fired before its host mounts plays as it would have",
    "[deferredNested]")
{
  auto play = [](bool defer) {
    auto file = importWith(
        "assets/quiet_inactive_hosts.riv",
        &gNoOpFactory,
        defer);
    auto artboard = file->artboardNamed("Main");
    auto viewModelInstance = file->createViewModelInstance(
        artboard->viewModelId(),
        0);
    auto machine = artboard->stateMachineAt(0);
    machine->bindViewModelInstance(viewModelInstance);
    auto advance = [&](int frames) {
      for (int i = 0; i < frames; i++)
      {
        machine->advanceAndApply(1.0f / 60.0f);
      }
    };
    advance(90);
    auto list = artboard->find<ArtboardComponentList>("Paint Rows");
    REQUIRE(list != nullptr);
    REQUIRE(list->artboardCount() == 1);
    auto row = list->artboardInstance(0);
    auto host = row->find<NestedArtboard>("Flasher");
    REQUIRE(host != nullptr);
    bool deferredWhileHidden = host->isInstanceDeferred();
    auto flash = [&]() {
      list->listItem(0)
          ->viewModelInstance()
          ->propertyValue("flash")
          ->as<ViewModelInstanceTrigger>()
          ->trigger();
    };
    flash();
    advance(30);
    row->find<Solo>("Switch")->updateByName("On");
    advance(30);
    REQUIRE(host->artboardInstance() != nullptr);
    auto source = host->artboardInstance()
                      ->find<PaintImage>("Flash Paint")
                      ->artboardInstance();
    REQUIRE(source != nullptr);
    auto drawn = [&]() {
      return (uint32_t)source->find<SolidColor>("Color")->colorValue();
    };
    auto shown = drawn();
    flash();
    advance(30);
    return std::make_tuple(deferredWhileHidden, shown, drawn());
  };
  auto eager = play(false);
  auto deferred = play(true);
  CHECK_FALSE(std::get<0>(eager));
  CHECK(std::get<0>(deferred));
  // Still, then played: grey, then gold.
  CHECK(std::get<1>(eager) == 0xFF808080);
  CHECK(std::get<2>(eager) == 0xFFE0A557);
  CHECK(std::get<1>(deferred) == std::get<1>(eager));
  CHECK(std::get<2>(deferred) == std::get<2>(eager));
}

// assets/layout/nested_artboard_rotated.riv's NestedRotatedHost: a flex row
// holding a 60x60 fixed sibling and a NestedArtboardLayout. Collapsed in the
// source, the host defers in instances, where nothing collapses it: its first
// advance mounts it, and the row re-collects it before that frame's layout, so
// it lands in the slot it does when instanced up front.
TEST_CASE(
    "a shown deferred nested artboard layout mounts on its first advance",
    "[deferredNested]")
{
  auto slot = [](bool defer) {
    auto file = importWith(
        "assets/layout/nested_artboard_rotated.riv",
        &gNoOpFactory,
        defer);
    auto sourceHosts = file->artboard("NestedRotatedHost")
                           ->find<NestedArtboardLayout>();
    REQUIRE(sourceHosts.size() == 1);
    sourceHosts[0]->collapse(true);

    auto artboard = file->artboardNamed("NestedRotatedHost");
    auto hosts = artboard->find<NestedArtboardLayout>();
    REQUIRE(hosts.size() == 1);
    auto host = hosts[0];
    CHECK_FALSE(host->isCollapsed());
    CHECK(host->isInstanceDeferred() == defer);
    artboard->advance(0.0f);
    CHECK_FALSE(host->isInstanceDeferred());
    REQUIRE(host->artboardInstance() != nullptr);
    return Vec2D(
        host->artboardInstance()->layoutX(),
        host->artboardInstance()->layoutY());
  };
  auto eager = slot(false);
  auto deferred = slot(true);
  CHECK(eager.x != 0.0f);
  CHECK(deferred.x == Approx(eager.x));
  CHECK(deferred.y == Approx(eager.y));
}

namespace
{
// runtime_nested_inputs.riv's MainArtboard instanced with "CircleOuter"
// deferred: collapsed in the source, so every instance defers it.
struct DeferredCircle
{
  rcp<File> file = importDeferring("assets/runtime_nested_inputs.riv");
  std::unique_ptr<ArtboardInstance> artboard;
  NestedArtboard* host = nullptr;

  DeferredCircle()
  {
    file->artboard("MainArtboard")
        ->find<NestedArtboard>("CircleOuter")
        ->collapse(true);
    artboard = file->artboard("MainArtboard")->instance();
    host = artboard->find<NestedArtboard>("CircleOuter");
    REQUIRE(host != nullptr);
    REQUIRE(host->isInstanceDeferred());
  }

  // The first nested input of type T, found without mounting the host.
  template <typename T> T* input()
  {
    for (auto animation : host->nestedAnimations())
    {
      auto machine = animation->as<NestedStateMachine>();
      for (size_t i = 0; machine->input(i) != nullptr; i++)
      {
        if (machine->input(i)->is<T>())
        {
          return machine->input(i)->as<T>();
        }
      }
    }
    FAIL("no nested input of that type");
    return nullptr;
  }
};
} // namespace

// A list hands a row it reuses to another item after remounting its hosts.
// A remount starts the machines afresh from the source's values, so what a
// deferred host held for the previous item can't reach the next one.
TEST_CASE(
    "remounting a deferred host drops what its nested inputs held",
    "[deferredNested]")
{
  DeferredCircle scene;
  auto nestedBool = scene.input<NestedBool>();
  auto nestedTrigger = scene.input<NestedTrigger>();
  nestedBool->nestedValue(true);
  nestedTrigger->applyValue();

  scene.host->remount();
  REQUIRE(scene.host->isInstanceDeferred());
  CHECK(nestedBool->nestedValue() == false);

  auto boolInput = scene.artboard->getBool("CircleOuterState", "CircleOuter");
  REQUIRE(boolInput != nullptr);
  CHECK_FALSE(scene.host->isInstanceDeferred());
  CHECK(boolInput->value() == false);
  auto trigger = scene.artboard->getTrigger(
      "CircleOuterTrigger",
      "CircleOuter");
  REQUIRE(trigger != nullptr);
  CHECK_FALSE(trigger->didFire());
}

// A value set while deferred is held by the nested input until the host mounts.
// Mounted, the row can still be reused: the remount's machines take their
// nested inputs' values, so what was held for the previous item must not be
// handed to the next one either.
TEST_CASE(
    "remounting a mounted host drops what was held while it was deferred",
    "[deferredNested]")
{
  DeferredCircle scene;
  auto nestedBool = scene.input<NestedBool>();
  nestedBool->nestedValue(true);

  scene.host->mountDeferred();
  REQUIRE_FALSE(scene.host->isInstanceDeferred());
  auto mounted = scene.artboard->getBool("CircleOuterState", "CircleOuter");
  REQUIRE(mounted != nullptr);
  CHECK(mounted->value() == true);

  scene.host->remount();
  CHECK(nestedBool->nestedValue() == false);
  auto remounted = scene.artboard->getBool("CircleOuterState", "CircleOuter");
  REQUIRE(remounted != nullptr);
  CHECK(remounted->value() == false);
}

// A list remounts the rows it reuses as it scrolls, and putting a host's
// nested inputs back looks up each one's source. Only a host that had no
// machine can hold a value, so one that never deferred skips it, and one
// that did does it once after it mounts.
TEST_CASE(
    "only a host that was deferred resets its nested inputs on remount",
    "[deferredNested]")
{
  SECTION("never deferred")
  {
    auto file = importWith(
        "assets/runtime_nested_inputs.riv",
        &gNoOpFactory,
        false);
    auto artboard = file->artboard("MainArtboard")->instance();
    auto host = artboard->find<NestedArtboard>("CircleOuter");
    REQUIRE(host != nullptr);
    REQUIRE(host->artboardInstance() != nullptr);
    int before = NestedArtboard::testing_heldInputResets;
    host->remount();
    host->remount();
    CHECK(NestedArtboard::testing_heldInputResets == before);
  }
  SECTION("deferred, then mounted")
  {
    DeferredCircle scene;
    scene.host->mountDeferred();
    REQUIRE_FALSE(scene.host->isInstanceDeferred());
    int before = NestedArtboard::testing_heldInputResets;
    scene.host->remount();
    CHECK(NestedArtboard::testing_heldInputResets == before + 1);
    scene.host->remount();
    CHECK(NestedArtboard::testing_heldInputResets == before + 1);
  }
  SECTION("still deferred")
  {
    // It can be handed values for each item it hosts, so each remount puts
    // them back.
    DeferredCircle scene;
    int before = NestedArtboard::testing_heldInputResets;
    scene.host->remount();
    scene.host->remount();
    CHECK(scene.host->isInstanceDeferred());
    CHECK(NestedArtboard::testing_heldInputResets == before + 2);
  }
}

// Artboard::volume reaches only the instances that exist when it is set.
TEST_CASE("a deferred host mounts at its artboard's volume", "[deferredNested]")
{
  DeferredCircle scene;
  scene.artboard->volume(0.25f);
  scene.host->mountDeferred();
  REQUIRE(scene.host->artboardInstance() != nullptr);
  CHECK(scene.host->artboardInstance()->volume() == Approx(0.25f));
}

// assets/collapsed_nested_databind.riv's bodyB host starts collapsed. Bound
// and then unbound before it mounts, it has to mount unbound, as an instanced
// host would be, rather than bind the context it was handed before.
TEST_CASE(
    "a deferred host unbound before it mounts mounts unbound",
    "[deferredNested]")
{
  auto mountedContext = [](bool unbind) {
    auto file = importDeferring("assets/collapsed_nested_databind.riv");
    auto artboard = file->artboardDefault();
    auto vmi = file->createDefaultViewModelInstance(artboard.get());
    artboard->bindViewModelInstance(vmi);
    auto hosts = artboard->find<NestedArtboard>();
    REQUIRE(hosts.size() == 1);
    auto host = hosts[0];
    REQUIRE(host->isInstanceDeferred());
    if (unbind)
    {
      artboard->unbind();
    }
    host->mountDeferred();
    REQUIRE(host->artboardInstance() != nullptr);
    return host->artboardInstance()->dataContext() != nullptr;
  };
  CHECK(mountedContext(false));
  CHECK_FALSE(mountedContext(true));
}

// assets/layout/nested_artboard_rotated.riv's NestedRotatedHost: a flex row
// holding a NestedArtboardLayout. The row turns right to left while the host
// is deferred; the row cascades that only when its style changes, so the
// host has to be handed it when it mounts.
TEST_CASE(
    "a deferred nested artboard layout mounts with its owner's "
    "direction",
    "[deferredNested]")
{
  auto direction = [](bool defer) {
    auto file = importWith(
        "assets/layout/nested_artboard_rotated.riv",
        &gNoOpFactory,
        defer);
    file->artboard("NestedRotatedHost")
        ->find<NestedArtboardLayout>()[0]
        ->collapse(true);
    auto artboard = file->artboardNamed("NestedRotatedHost");
    auto host = artboard->find<NestedArtboardLayout>()[0];
    CHECK(host->isInstanceDeferred() == defer);
    REQUIRE(host->parent()->is<LayoutComponent>());
    auto owner = host->parent()->as<LayoutComponent>();
    REQUIRE(owner->style() != nullptr);
    owner->style()->directionValue(static_cast<uint8_t>(YGDirectionRTL));
    // The row cascades now, while the host is still deferred (an advance
    // would mount the shown host first).
    artboard->updateComponents();
    CHECK(host->isInstanceDeferred() == defer);
    host->mountDeferred();
    artboard->advance(0.0f);
    REQUIRE(host->artboardInstance() != nullptr);
    return host->artboardInstance()->actualDirection();
  };
  CHECK(direction(false) == LayoutDirection::rtl);
  CHECK(direction(true) == LayoutDirection::rtl);
}

namespace
{
// Every stop Tab lands on, from nothing focused to the end of the order.
std::vector<FocusNode*> tabOrder(FocusManager* manager)
{
  manager->clearFocus();
  std::vector<FocusNode*> order;
  for (size_t step = 0; step < 64; step++)
  {
    if (!manager->focusNext() || manager->primaryFocusPtr() == nullptr)
    {
      break;
    }
    order.push_back(manager->primaryFocusPtr());
  }
  return order;
}
} // namespace

// assets/list_focus_order.riv's first artboard hosts nested artboards with
// focusables and state machines of their own. Collapsed in the source, the
// first host defers; mounted under the root's focus manager, its focus nodes
// have to join Tab order where instancing would have put them.
TEST_CASE(
    "a deferred host's focusables join Tab order when it mounts",
    "[deferredNested]")
{
  auto stops = [](bool defer) {
    auto file = importWith("assets/list_focus_order.riv", &gNoOpFactory, defer);
    auto sourceHosts = file->artboard()->find<NestedArtboard>();
    REQUIRE(!sourceHosts.empty());
    sourceHosts[0]->collapse(true);

    auto artboard = file->artboardDefault();
    auto host = artboard->nestedArtboards()[0];
    CHECK(host->isInstanceDeferred() == defer);
    auto stateMachine = artboard->stateMachineAt(0);
    auto vmi = file->createDefaultViewModelInstance(artboard.get());
    stateMachine->bindViewModelInstance(vmi);
    // Shown (nothing collapses it in the instance), so the advance mounts it
    // under the machine's focus manager.
    stateMachine->advanceAndApply(0.016f);
    REQUIRE(stateMachine->focusManager() != nullptr);
    REQUIRE(host->hasNestedStateMachines());
    REQUIRE(host->artboardInstance() != nullptr);

    auto order = tabOrder(stateMachine->focusManager());
    auto nested = host->artboardInstance()->find<FocusData>();
    REQUIRE(!nested.empty());
    for (auto* focusData : nested)
    {
      CHECK(
          std::find(order.begin(), order.end(), focusData->focusNode().get()) !=
          order.end());
    }
    return order.size();
  };
  size_t eager = stops(false);
  CHECK(eager > 0);
  CHECK(stops(true) == eager);
}

// Cloning an instance clones what its hosts mounted. A host that mounted and
// was collapsed again keeps whatever changed in what it hosts, so its clone
// has to be cloned from that, not deferred to mount the source later.
TEST_CASE(
    "cloning an instance clones a collapsed host's mounted artboard",
    "[deferredNested]")
{
  DeferredCircle scene;
  scene.host->mountDeferred();
  auto mounted = scene.host->artboardInstance();
  REQUIRE(mounted != nullptr);
  Node* moved = nullptr;
  size_t movedIndex = 0;
  for (size_t i = 1; i < mounted->objects().size(); i++)
  {
    auto object = mounted->objects()[i];
    if (object != nullptr && object->is<Node>())
    {
      moved = object->as<Node>();
      movedIndex = i;
      break;
    }
  }
  REQUIRE(moved != nullptr);
  moved->x(moved->x() + 123.0f);
  const float x = moved->x();
  scene.host->collapse(true);

  auto clone = scene.artboard->instance();
  auto cloneHost = clone->find<NestedArtboard>("CircleOuter");
  REQUIRE(cloneHost != nullptr);
  CHECK_FALSE(cloneHost->isInstanceDeferred());
  REQUIRE(cloneHost->artboardInstance() != nullptr);
  auto cloned = cloneHost->artboardInstance()->objects()[movedIndex];
  REQUIRE(cloned != nullptr);
  REQUIRE(cloned->is<Node>());
  CHECK(cloned->as<Node>()->x() == Approx(x));
}
