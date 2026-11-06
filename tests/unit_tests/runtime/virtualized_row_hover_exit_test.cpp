#include <rive/file.hpp>
#include <rive/artboard.hpp>
#include <rive/artboard_component_list.hpp>
#include <rive/animation/animation_state.hpp>
#include <rive/animation/linear_animation.hpp>
#include <rive/animation/state_machine_instance.hpp>
#include <rive/constraints/scrolling/scroll_constraint.hpp>
#include <rive/viewmodel/viewmodel_instance.hpp>
#include <rive/viewmodel/viewmodel_instance_boolean.hpp>
#include <rive/viewmodel/viewmodel_instance_list.hpp>
#include <rive/viewmodel/viewmodel_instance_list_item.hpp>
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <string>
#include <vector>

// A list passes pointer events on to the rows it shows. A row it stops
// showing gets no more of them, so it has to get its exit as it goes, or
// whatever its enter listeners wrote stays written. A virtualized list stops
// showing a row once it scrolls out of the window, and any list stops
// showing the row of an item removed from it.

using namespace rive;

namespace
{
// virtualized_row_hover_exit.riv's Main: Virtual Rows (x 0-150) and Plain
// Rows (x 150-300), each 20 rows of 60 in a 300 tall viewport. A row sets its
// item's hover while the pointer is over it, and badgeHover while it is over
// the Badge the row hosts (x 100-140 and y 10-50 in the row). A row's exit
// also fires a global trigger, which takes Main's machine from idle to left,
// and an event its own machine sets leftEvent on; the badge's exit fires one
// the row's machine sets badgeLeftEvent on, and the badge's machine sets
// badgeSawLeft once leftEvent changes. A press on either is claimed by a
// capture listener there.
struct HoverRows
{
  rcp<File> file;
  std::unique_ptr<ArtboardInstance> artboard;
  std::unique_ptr<StateMachineInstance> machine;
  rcp<ViewModelInstance> viewModelInstance;

  HoverRows()
  {
    file = ReadRiveFile("assets/virtualized_row_hover_exit.riv");
    artboard = file->artboardNamed("Main");
    REQUIRE(artboard != nullptr);
    viewModelInstance = file->createViewModelInstance(
        artboard->viewModelId(),
        0);
    machine = artboard->stateMachineAt(0);
    machine->bindViewModelInstance(viewModelInstance);
    advance(10);
  }

  void advance(int frames)
  {
    for (int i = 0; i < frames; i++)
    {
      machine->advanceAndApply(1.0f / 60.0f);
    }
  }

  ArtboardComponentList* list(const std::string& prefix)
  {
    auto list = artboard->find<ArtboardComponentList>(prefix + " Rows");
    REQUIRE(list != nullptr);
    return list;
  }

  // The animation Main's machine is in: idle, then left once a row's exit
  // has fired the trigger.
  std::string mainState()
  {
    auto state = machine->layerState(0);
    if (state == nullptr || !state->is<AnimationState>() ||
        state->as<AnimationState>()->animation() == nullptr)
    {
      return "";
    }
    return state->as<AnimationState>()->animation()->name();
  }

  ScrollConstraint* scroll(const std::string& prefix)
  {
    auto scroll = artboard->find<ScrollConstraint>(prefix + " Scroll");
    REQUIRE(scroll != nullptr);
    return scroll;
  }

  bool flag(const std::string& prefix, int index, const char* name)
  {
    return list(prefix)
        ->listItem(index)
        ->viewModelInstance()
        ->propertyValue(name)
        ->as<ViewModelInstanceBoolean>()
        ->propertyValue();
  }

  // The items whose rows the pointer is over, by hover and by badgeHover.
  std::vector<int> hovered(const std::string& prefix, const char* name)
  {
    std::vector<int> items;
    for (int i = 0; i < (int)list(prefix)->artboardCount(); i++)
    {
      if (flag(prefix, i, name))
      {
        items.push_back(i);
      }
    }
    return items;
  }
};
} // namespace

// The pointer rests on the first row's badge, and the list scrolls 900 down
// under it: the row it was on goes out of the window, and the 16th row (at
// 900-960) comes under the pointer. The row gets its exit as it goes, so its
// item no longer reads as hovered, and nor does the badge it hosts. The next
// move enters the row now under the pointer. A list that doesn't virtualize
// keeps the row, which gets its exit from that move.
TEST_CASE(
    "A hovered row that scrolls out of a virtualized list gets its exit",
    "[component_list]")
{
  for (const char* prefix : {"Virtual", "Plain"})
  {
    INFO(prefix);
    const bool virtualized = std::string(prefix) == "Virtual";
    HoverRows scene;
    const float x = virtualized ? 120.0f : 270.0f;
    scene.machine->pointerMove(Vec2D(x, 30.0f));
    scene.advance(1);
    REQUIRE(scene.hovered(prefix, "hover") == std::vector<int>{0});
    REQUIRE(scene.hovered(prefix, "badgeHover") == std::vector<int>{0});
    REQUIRE(scene.mainState() == "idle");

    scene.scroll(prefix)->offsetY(-900.0f);
    scene.advance(10);
    CHECK((scene.list(prefix)->artboardInstance(0) == nullptr) == virtualized);
    if (virtualized)
    {
      CHECK(scene.hovered(prefix, "hover").empty());
      CHECK(scene.hovered(prefix, "badgeHover").empty());
      CHECK(scene.mainState() == "left");
    }

    scene.machine->pointerMove(Vec2D(x, 31.0f));
    scene.advance(1);
    CHECK(scene.hovered(prefix, "hover") == std::vector<int>{15});
    CHECK(scene.hovered(prefix, "badgeHover") == std::vector<int>{15});
    scene.advance(1);
    CHECK(scene.mainState() == "left");
  }
}

// A drag scrolling the list turns the pointer off for its rows, and a
// listener that is off misses its exit. A row that goes out of the window
// mid drag can't have it turned back on later, so it gets its exit all the
// same.
TEST_CASE(
    "A row that scrolls out of a virtualized list mid drag gets its exit",
    "[component_list]")
{
  HoverRows scene;
  scene.machine->pointerMove(Vec2D(120.0f, 30.0f));
  scene.advance(1);
  REQUIRE(scene.hovered("Virtual", "hover") == std::vector<int>{0});
  // As a drag the host's scroll started turns it off for the row.
  scene.list("Virtual")->stateMachineInstance(0)->disablePointerEvents(0);

  scene.scroll("Virtual")->offsetY(-900.0f);
  scene.advance(10);
  REQUIRE(scene.list("Virtual")->artboardInstance(0) == nullptr);
  CHECK(scene.hovered("Virtual", "hover").empty());
  CHECK(scene.hovered("Virtual", "badgeHover").empty());
  CHECK(scene.mainState() == "left");
}

// The pointer rests on the first row's badge, and the row's item is removed:
// the row goes, in either list, and the second item's row moves up under the
// pointer. The row gets its exit as it goes, so the removed item no longer
// reads as hovered, and its exit's trigger reaches Main's machine. The next
// move enters the row now under the pointer.
TEST_CASE(
    "A hovered row whose item is removed from the list gets its exit",
    "[component_list]")
{
  for (const char* prefix : {"Virtual", "Plain"})
  {
    INFO(prefix);
    HoverRows scene;
    const float x = std::string(prefix) == "Virtual" ? 120.0f : 270.0f;
    scene.machine->pointerMove(Vec2D(x, 30.0f));
    scene.advance(1);
    REQUIRE(scene.hovered(prefix, "hover") == std::vector<int>{0});
    REQUIRE(scene.mainState() == "idle");
    auto removed = scene.list(prefix)->listItem(0)->viewModelInstance();

    scene.viewModelInstance
        ->propertyValue(
            std::string(prefix) == "Virtual" ? "virtualItems" : "plainItems")
        ->as<ViewModelInstanceList>()
        ->removeItem(0);
    scene.advance(10);
    REQUIRE(scene.list(prefix)->artboardCount() == 19);
    CHECK_FALSE(removed->propertyValue("hover")
                    ->as<ViewModelInstanceBoolean>()
                    ->propertyValue());
    CHECK_FALSE(removed->propertyValue("badgeHover")
                    ->as<ViewModelInstanceBoolean>()
                    ->propertyValue());
    CHECK(scene.hovered(prefix, "hover").empty());
    CHECK(scene.mainState() == "left");

    scene.machine->pointerMove(Vec2D(x, 31.0f));
    scene.advance(1);
    CHECK(scene.hovered(prefix, "hover") == std::vector<int>{0});
    CHECK(scene.hovered(prefix, "badgeHover") == std::vector<int>{0});
  }
}

// The events a row's exits fire reach its machine's listeners at its next
// advance, which a row that leaves the window never gets bound to its item:
// destroyed, it gets none, and reused, it gets one bound to another item. So
// they are delivered as the row goes, the badge's first, until delivering
// queues no more.
TEST_CASE(
    "A row that scrolls out of a virtualized list delivers its exit's events",
    "[component_list]")
{
  HoverRows scene;
  scene.machine->pointerMove(Vec2D(120.0f, 30.0f));
  scene.advance(1);
  REQUIRE(scene.hovered("Virtual", "hover") == std::vector<int>{0});
  auto departing = scene.list("Virtual")->listItem(0)->viewModelInstance();
  auto flag = [&](const char* name) {
    return departing->propertyValue(name)
        ->as<ViewModelInstanceBoolean>()
        ->propertyValue();
  };

  scene.scroll("Virtual")->offsetY(-900.0f);
  scene.advance(10);
  REQUIRE(scene.list("Virtual")->artboardInstance(0) == nullptr);
  CHECK(flag("leftEvent"));
  CHECK(flag("badgeLeftEvent"));
  // The row's listener set leftEvent after the badge's reports were
  // delivered, which queued another in the badge.
  CHECK(flag("badgeSawLeft"));
  CHECK(scene.hovered("Virtual", "leftEvent") == std::vector<int>{0});
  CHECK(scene.hovered("Virtual", "badgeLeftEvent") == std::vector<int>{0});
}

// A press on the row's badge is claimed twice: by the row's capture listener
// on Hit, which the badge isn't under, and by the badge's on Dot. A claim
// keeps the normal listeners under it off until the press's release, which a
// row that leaves the window mid press never gets, so both let go as it goes
// and their exits still run.
TEST_CASE(
    "A row that scrolls out of a virtualized list mid press gets its exit",
    "[component_list]")
{
  HoverRows scene;
  scene.machine->pointerMove(Vec2D(120.0f, 30.0f));
  scene.advance(1);
  scene.machine->pointerDown(Vec2D(120.0f, 30.0f));
  scene.advance(1);
  REQUIRE(scene.hovered("Virtual", "pressed") == std::vector<int>{0});
  REQUIRE(scene.hovered("Virtual", "hover") == std::vector<int>{0});
  REQUIRE(scene.hovered("Virtual", "badgeHover") == std::vector<int>{0});

  scene.scroll("Virtual")->offsetY(-900.0f);
  scene.advance(10);
  REQUIRE(scene.list("Virtual")->artboardInstance(0) == nullptr);
  CHECK(scene.hovered("Virtual", "hover").empty());
  CHECK(scene.hovered("Virtual", "badgeHover").empty());
  CHECK(scene.mainState() == "left");
}
