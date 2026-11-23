#include <rive/file.hpp>
#include <rive/artboard.hpp>
#include <rive/artboard_component_list.hpp>
#include <rive/animation/state_machine_instance.hpp>
#include <rive/constraints/scrolling/scroll_constraint.hpp>
#include <rive/viewmodel/viewmodel_instance.hpp>
#include <rive/viewmodel/viewmodel_instance_boolean.hpp>
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <vector>

// A layout hidden with displayValue none collapses everything under it, and
// a list inside measures its rows at no size while it is collapsed. A
// virtualized list keeps the size each row on screen measured, and lays its
// window out from that table, so a collapsed one must not write into it: shown
// again, it would place every row it had on screen at no height.

using namespace rive;

namespace
{
// virtualized_list_display_none.riv's Main: Rows, eight rows 40 tall in a 200
// tall viewport whose scroll virtualizes, inside Panel. Main's machine keys
// Panel's display to none while the view model's shown is false.
struct HiddenList
{
  static constexpr float kRowHeight = 40.0f;
  static constexpr int kRows = 8;

  rcp<File> file;
  std::unique_ptr<ArtboardInstance> artboard;
  std::unique_ptr<StateMachineInstance> machine;
  rcp<ViewModelInstance> viewModelInstance;

  HiddenList()
  {
    file = ReadRiveFile("assets/virtualized_list_display_none.riv");
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

  void show(bool value)
  {
    viewModelInstance->propertyValue("shown")
        ->as<ViewModelInstanceBoolean>()
        ->propertyValue(value);
  }

  ArtboardComponentList* list()
  {
    auto list = artboard->find<ArtboardComponentList>("Rows");
    REQUIRE(list != nullptr);
    return list;
  }

  ScrollConstraint* scroll()
  {
    auto scroll = artboard->find<ScrollConstraint>("Scroll");
    REQUIRE(scroll != nullptr);
    return scroll;
  }

  // The rows the list has realized, as {index, y} pairs.
  std::vector<std::pair<int, float>> realized()
  {
    std::vector<std::pair<int, float>> rows;
    for (int i = 0; i < kRows; i++)
    {
      if (list()->artboardInstance(i) != nullptr)
      {
        rows.push_back({i, list()->itemPosition(i).y});
      }
    }
    return rows;
  }
};
} // namespace

TEST_CASE(
    "A virtualized list hidden with display none keeps its rows' sizes",
    "[component_list]")
{
  HiddenList scene;
  for (int i = 0; i < HiddenList::kRows; i++)
  {
    REQUIRE(scene.list()->itemSize(i).y == HiddenList::kRowHeight);
  }
  scene.show(false);
  scene.advance(10);
  REQUIRE(scene.list()->isCollapsed());
  for (int i = 0; i < HiddenList::kRows; i++)
  {
    INFO("row " << i);
    CHECK(scene.list()->itemSize(i).y == HiddenList::kRowHeight);
  }
}

TEST_CASE(
    "A virtualized list hidden with display none comes back with its rows in "
    "place",
    "[component_list]")
{
  for (float offset : {0.0f, -100.0f})
  {
    INFO("scrolled to " << offset);
    HiddenList scene;
    scene.scroll()->offsetY(offset);
    scene.advance(10);
    auto before = scene.realized();
    // The rows on screen, at least, are realized, each 40 below the last.
    REQUIRE(before.size() >= 5);
    for (auto& row : before)
    {
      INFO("row " << row.first);
      REQUIRE(row.second == row.first * HiddenList::kRowHeight + offset);
    }

    scene.show(false);
    scene.advance(10);
    scene.show(true);
    // From the first frame it shows, and in the frames after.
    scene.advance(1);
    CHECK(scene.realized() == before);
    scene.advance(9);

    CHECK(scene.scroll()->offsetY() == offset);
    CHECK(scene.realized() == before);
  }
}
