#include "rive/animation/state_machine_instance.hpp"
#include "rive/artboard_component_list.hpp"
#include "rive/constraints/scrolling/scroll_constraint.hpp"
#include "rive/focus_data.hpp"
#include "rive/input/focus_manager.hpp"
#include "rive/input/focus_node.hpp"
#include "rive/layout/layout_component_style.hpp"
#include "rive/layout_component.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/viewmodel_instance_boolean.hpp"
#include "rive/viewmodel/viewmodel_instance_list.hpp"
#include "rive/viewmodel/viewmodel_instance_list_item.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <string>
#include <vector>

// Focus in a virtualized list. virtualized_focus_rows.riv shows 20 items of
// 200x50 through a 200x200 viewport, four at a time, in a vertical scroll
// that virtualizes and animates to a position. Items 5 and 6 show an artboard
// with nothing to focus; every other item's row holds one focus stop, and its
// machine writes the item's `focused` on focus and on blur.
//
// The file's Outer artboard nests that: six groups, two showing, each a
// virtualized scroll of six such rows of which two show. Its Grid artboard is
// 24 focusable cells of 100x50 in four columns, in a 250 wide viewport that
// scrolls down only, so the last column is never built.

namespace
{
const float kFrame = 1.0f / 60.0f;
const float kRowHeight = 50.0f;
const float kViewportHeight = 200.0f;

struct FocusRows
{
  rive::rcp<rive::File> file;
  std::unique_ptr<rive::ArtboardInstance> artboard;
  std::unique_ptr<rive::StateMachineInstance> stateMachine;
  rive::rcp<rive::ViewModelInstance> viewModelInstance;
  rive::ArtboardComponentList* list = nullptr;
  rive::ScrollConstraint* scroll = nullptr;
  rive::ViewModelInstanceList* items = nullptr;
  rive::FocusManager* focus = nullptr;

  FocusRows(const char* artboardName = "Main", const char* listName = "Rows")
  {
    file = ReadRiveFile("assets/virtualized_focus_rows.riv");
    artboard = file->artboardNamed(artboardName);
    REQUIRE(artboard != nullptr);
    viewModelInstance = file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    artboard->bindViewModelInstance(viewModelInstance);
    stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);
    stateMachine->bindViewModelInstance(viewModelInstance);
    list = artboard->find<rive::ArtboardComponentList>(listName);
    REQUIRE(list != nullptr);
    scroll = artboard->find<rive::ScrollConstraint>()[0];
    for (auto& value : viewModelInstance->propertyValues())
    {
      if (value->is<rive::ViewModelInstanceList>())
      {
        items = value->as<rive::ViewModelInstanceList>();
      }
    }
    REQUIRE(items != nullptr);
    advance(4);
    focus = stateMachine->focusManager();
    REQUIRE(focus != nullptr);
    REQUIRE(list->virtualizationEnabled());
    REQUIRE(list->listScopeFocusNode() != nullptr);
  }

  void advance(int frames)
  {
    for (int i = 0; i < frames; i++)
    {
      stateMachine->advanceAndApply(kFrame);
    }
  }

  // Long enough for a scroll to a position to come to rest.
  void settle() { advance(180); }

  bool realized(int item) { return list->artboardInstance(item) != nullptr; }

  // The item whose row holds the primary focus, or -1.
  int focusedItem()
  {
    auto node = focus->primaryFocus();
    if (node == nullptr || node->focusable() == nullptr)
    {
      return -1;
    }
    auto* holder = node->focusable()->focusableArtboard();
    for (int i = 0; i < list->artboardCount(); i++)
    {
      if (list->artboardInstance(i) == holder)
      {
        return i;
      }
    }
    return -1;
  }

  rive::rcp<rive::FocusNode> stopOf(int item)
  {
    auto* row = list->artboardInstance(item);
    REQUIRE(row != nullptr);
    auto data = row->find<rive::FocusData>();
    REQUIRE(data.size() == 1);
    return data[0]->focusNode();
  }

  // Where the item's row is drawn down the viewport.
  float drawnTop(int item)
  {
    auto transform = list->worldTransformForArtboard(
        list->artboardInstance(item));
    return transform[5];
  }

  bool inView(int item)
  {
    float top = drawnTop(item);
    return top > -0.5f && top + kRowHeight < kViewportHeight + 0.5f;
  }

  bool readsFocused(int item)
  {
    auto instance = items->listItems()[item]->viewModelInstance();
    auto* value = instance->propertyValue("focused");
    REQUIRE(value != nullptr);
    return value->as<rive::ViewModelInstanceBoolean>()->propertyValue();
  }

  // Tab or Shift+Tab, then the frames it takes to land.
  void tab(bool forward = true)
  {
    if (forward)
    {
      focus->focusNext();
    }
    else
    {
      focus->focusPrevious();
    }
    settle();
  }
};

bool holdsFocusNodes(rive::FocusNode* node)
{
  for (const auto& child : node->children())
  {
    if (child->focusable() != nullptr || holdsFocusNodes(child.get()))
    {
      return true;
    }
  }
  return false;
}
} // namespace

TEST_CASE(
    "Tab walks a virtualized list's items in order",
    "[component_list][focus][virtual_focus]")
{
  FocusRows rows;
  REQUIRE(rows.items->listItems().size() == 20);
  // Four rows show, and not all twenty are built.
  CHECK(rows.realized(0));
  CHECK_FALSE(rows.realized(12));

  // Every item with something to focus, in list order: 5 and 6 have none.
  std::vector<int> expected;
  for (int i = 0; i < 20; i++)
  {
    if (i != 5 && i != 6)
    {
      expected.push_back(i);
    }
  }
  for (int item : expected)
  {
    rows.tab();
    INFO("expecting item " << item);
    REQUIRE(rows.focusedItem() == item);
    CHECK(rows.inView(item));
  }
  // The scroll went with it, so the first rows were recycled.
  CHECK_FALSE(rows.realized(0));

  // Off the end of the list, as off the end of any scope.
  rows.tab();
  CHECK(rows.focusedItem() == -1);
}

TEST_CASE(
    "Shift+Tab walks a virtualized list's items back",
    "[component_list][focus][virtual_focus]")
{
  FocusRows rows;
  // Into the list from its far end: item 19 isn't built yet.
  CHECK_FALSE(rows.realized(19));
  for (int item = 19; item >= 0; item--)
  {
    if (item == 5 || item == 6)
    {
      continue;
    }
    rows.tab(false);
    INFO("expecting item " << item);
    REQUIRE(rows.focusedItem() == item);
    CHECK(rows.inView(item));
  }
}

TEST_CASE(
    "Tab pressed again before a row arrives goes one item further",
    "[component_list][focus][virtual_focus]")
{
  FocusRows rows;
  rows.focus->setFocus(rows.stopOf(3));
  rows.advance(2);
  REQUIRE(rows.focusedItem() == 3);
  // 4 isn't in the viewport and 5 and 6 can't take focus: 4, then 7.
  rows.focus->focusNext();
  rows.focus->focusNext();
  rows.settle();
  CHECK(rows.focusedItem() == 7);
  CHECK(rows.inView(7));
}

TEST_CASE(
    "A row recycled while it held focus reports its blur",
    "[component_list][focus][virtual_focus]")
{
  FocusRows rows;
  for (int i = 0; i < 12; i++)
  {
    rows.tab();
  }
  // 0..4, then 7..13.
  REQUIRE(rows.focusedItem() == 13);
  CHECK_FALSE(rows.realized(0));
  for (int item = 0; item < 20; item++)
  {
    INFO("item " << item);
    if (item == 5 || item == 6)
    {
      continue;
    }
    CHECK(rows.readsFocused(item) == (item == 13));
  }

  // The focused row scrolled out of the window gives focus up, and says so.
  rows.scroll->offsetY(0.0f);
  rows.settle();
  REQUIRE_FALSE(rows.realized(13));
  CHECK(rows.focusedItem() == -1);
  CHECK_FALSE(rows.readsFocused(13));
}

TEST_CASE(
    "Rows outside a virtualized list's window hold no focus nodes",
    "[component_list][focus][virtual_focus]")
{
  FocusRows rows;
  // Scroll the first rows out to where two of the four showing are Labels.
  // Fewer Rows are needed there, so some of the pooled ones stay in the
  // pool rather than being handed to another item.
  rows.scroll->offsetY(-5.0f * kRowHeight);
  rows.settle();
  REQUIRE_FALSE(rows.realized(0));
  REQUIRE(rows.realized(5));
  REQUIRE(rows.realized(8));
  // One row per item, in order, under the list's scope.
  const auto& rowNodes = rows.list->listScopeFocusNode()->children();
  REQUIRE(rowNodes.size() == 20);
  for (int item = 0; item < 20; item++)
  {
    INFO("item " << item);
    bool expectNodes = rows.realized(item) && item != 5 && item != 6;
    CHECK(holdsFocusNodes(rowNodes[item].get()) == expectNodes);
  }
}

TEST_CASE(
    "Focus taken during a scroll is shown where the scroll ends",
    "[focus][virtual_focus]")
{
  FocusRows rows;
  // Start an animated scroll two rows down and catch it in flight.
  rows.scroll->scrollToPosition(0.0f, -2.0f * kRowHeight);
  rows.advance(2);
  REQUIRE(rows.scroll->offsetY() > -2.0f * kRowHeight + 1.0f);
  REQUIRE(rows.scroll->effectiveScrollOffsetY() == Approx(-2.0f * kRowHeight));

  // Item 1 is on screen now and would be above the viewport once the scroll
  // arrives. Showing it means stopping a row short, with item 1 at the top.
  rows.focus->setFocus(rows.stopOf(1));
  rows.settle();
  CHECK(rows.focusedItem() == 1);
  CHECK(rows.scroll->offsetY() == Approx(-kRowHeight).margin(0.5f));
  CHECK(rows.inView(1));
}

TEST_CASE(
    "Tab passes a virtualized list that isn't shown",
    "[component_list][focus][virtual_focus]")
{
  FocusRows rows;
  // Fully transparent: the rows it has built are no longer focus stops, and
  // the ones it hasn't must not be waited for either.
  rows.artboard->find<rive::LayoutComponent>("Viewport")->opacity(0.0f);
  rows.advance(4);
  for (int press = 0; press < 3; press++)
  {
    rows.focus->focusNext();
    CHECK_FALSE(rows.focus->hasPendingFocusRequests(rows.artboard.get()));
    rows.settle();
    CHECK(rows.focusedItem() == -1);
    CHECK(rows.scroll->offsetY() == Approx(0.0f));
  }
  for (int press = 0; press < 3; press++)
  {
    rows.focus->focusPrevious();
    CHECK_FALSE(rows.focus->hasPendingFocusRequests(rows.artboard.get()));
    rows.settle();
    CHECK(rows.focusedItem() == -1);
    CHECK(rows.scroll->offsetY() == Approx(0.0f));
  }
}

TEST_CASE(
    "Tab skips the columns a grid scrolled down only can't show",
    "[component_list][focus][virtual_focus]")
{
  FocusRows grid("Grid", "Cells");
  REQUIRE(grid.items->listItems().size() == 24);
  REQUIRE(grid.scroll->virtualizesGridColumns());
  REQUIRE_FALSE(grid.scroll->constrainsHorizontal());
  // Columns 0..2 reach the 250 wide viewport; column 3 starts at 300 and no
  // scroll brings it in.
  CHECK(grid.realized(2));
  CHECK_FALSE(grid.realized(3));

  // Every cell of the first three columns, row after row: the rows below the
  // viewport are scrolled to, the last column is passed without a wait.
  for (int item = 0; item < 24; item++)
  {
    if (item % 4 == 3)
    {
      continue;
    }
    grid.focus->focusNext();
    if (item % 4 != 0)
    {
      // Along a row there is nothing to wait for.
      CHECK_FALSE(grid.focus->hasPendingFocusRequests(grid.artboard.get()));
    }
    grid.settle();
    INFO("expecting cell " << item);
    REQUIRE(grid.focusedItem() == item);
    CHECK(grid.inView(item));
  }
  grid.focus->focusNext();
  CHECK_FALSE(grid.focus->hasPendingFocusRequests(grid.artboard.get()));
  grid.settle();
  CHECK(grid.focusedItem() == -1);

  // And back from the end.
  for (int item = 23; item >= 0; item--)
  {
    if (item % 4 == 3)
    {
      continue;
    }
    grid.tab(false);
    INFO("expecting cell " << item);
    REQUIRE(grid.focusedItem() == item);
  }
}

TEST_CASE(
    "Focus along a row the scroll can't show whole asks for it once",
    "[component_list][focus][virtual_focus]")
{
  FocusRows grid("Grid", "Cells");
  // Centered down the viewport too, the 300 tall grid starts 50 above it,
  // further than the scroll's end lets it come down.
  grid.scroll->viewport()->style()->layoutAlignmentType(4);
  grid.settle();
  REQUIRE(grid.scroll->content()->layoutY() == Approx(-50.0f));

  // The first cell asks for the 50 it is short of. The scroll is drawn
  // clamped, and the next cells of the row are measured from where it was
  // drawn: they ask for the same 50, not for 50 more each.
  grid.tab();
  REQUIRE(grid.focusedItem() == 0);
  const float asked = grid.scroll->offsetY();
  for (int item = 1; item < 4; item++)
  {
    grid.tab();
    REQUIRE(grid.focusedItem() == item);
    CHECK(grid.scroll->offsetY() == Approx(asked));
  }
}

TEST_CASE(
    "Tab reaches every column of a grid centered in its viewport",
    "[component_list][focus][virtual_focus]")
{
  FocusRows grid("Grid", "Cells");
  // The 400 wide grid centered across the 250 wide viewport hangs 75 past
  // either side, so the viewport shows 75..325 of it: some of every column, the
  // last one included though it starts past the viewport's width.
  grid.scroll->viewport()->style()->layoutAlignmentType(1);
  grid.settle();
  REQUIRE(grid.scroll->virtualizesGridColumns());
  REQUIRE_FALSE(grid.scroll->constrainsHorizontal());
  REQUIRE(grid.scroll->content()->layoutX() == Approx(-75.0f));
  REQUIRE(grid.realized(3));

  for (int item = 0; item < 24; item++)
  {
    grid.tab();
    INFO("expecting cell " << item);
    REQUIRE(grid.focusedItem() == item);
  }
  grid.tab();
  CHECK(grid.focusedItem() == -1);

  // Back from the end, each row above the window is entered at its last
  // cell.
  for (int item = 23; item >= 0; item--)
  {
    if (item == 7 || item == 3)
    {
      // Its row is above the window: waited for, not passed.
      REQUIRE_FALSE(grid.realized(item));
    }
    grid.tab(false);
    INFO("expecting cell " << item);
    REQUIRE(grid.focusedItem() == item);
  }
}

namespace
{
// The Outer artboard: groups of rows, each group a list of its own.
struct NestedRows : FocusRows
{
  NestedRows() : FocusRows("Outer", "Groups")
  {
    REQUIRE(items->listItems().size() == 6);
  }

  rive::ViewModelInstanceList* itemsOf(int group)
  {
    auto instance = items->listItems()[group]->viewModelInstance();
    auto* value = instance->propertyValue("items");
    REQUIRE(value != nullptr);
    return value->as<rive::ViewModelInstanceList>();
  }

  // group * 6 + item of the row holding the primary focus, or -1.
  int focusedCell()
  {
    auto node = focus->primaryFocus();
    if (node == nullptr || node->focusable() == nullptr)
    {
      return -1;
    }
    auto* holder = node->focusable()->focusableArtboard();
    for (int group = 0; group < 6; group++)
    {
      auto* groupRow = list->artboardInstance(group);
      if (groupRow == nullptr)
      {
        continue;
      }
      auto* inner = groupRow->find<rive::ArtboardComponentList>("Inner");
      for (int item = 0; item < inner->artboardCount(); item++)
      {
        if (inner->artboardInstance(item) == holder)
        {
          return group * 6 + item;
        }
      }
    }
    return -1;
  }

  // The cells whose data reads focused, as "group*6+item " each.
  std::string readingFocused()
  {
    std::string out;
    for (int group = 0; group < 6; group++)
    {
      auto cells = itemsOf(group)->listItems();
      for (int item = 0; item < (int)cells.size(); item++)
      {
        auto* value = cells[item]->viewModelInstance()->propertyValue(
            "focused");
        if (value->as<rive::ViewModelInstanceBoolean>()->propertyValue())
        {
          out += std::to_string(group * 6 + item) + " ";
        }
      }
    }
    return out;
  }
};
} // namespace

TEST_CASE(
    "Tab walks virtualized lists nested in a virtualized list",
    "[component_list][focus][virtual_focus]")
{
  NestedRows rows;
  // Two groups show, and two rows of each.
  CHECK(rows.realized(1));
  CHECK_FALSE(rows.realized(3));

  // Every row of every group in order. A group recycled while one of its
  // rows held focus hands on a scroll left at its end, so the next group to
  // use it arrives with its first rows unbuilt.
  for (int cell = 0; cell < 36; cell++)
  {
    rows.tab();
    INFO("expecting cell " << cell);
    REQUIRE(rows.focusedCell() == cell);
    // The blur of the row before reached its data, in whichever machine.
    CHECK(rows.readingFocused() == std::to_string(cell) + " ");
  }
  rows.tab();
  CHECK(rows.focusedCell() == -1);
  CHECK(rows.readingFocused() == "");

  // Back: each group is entered at its last row, which isn't built.
  for (int cell = 35; cell >= 0; cell--)
  {
    rows.tab(false);
    INFO("expecting cell " << cell);
    REQUIRE(rows.focusedCell() == cell);
    CHECK(rows.readingFocused() == std::to_string(cell) + " ");
  }
}

TEST_CASE(
    "Shift+Tab into nested virtualized lists lands on the very last row",
    "[component_list][focus][virtual_focus]")
{
  NestedRows rows;
  // Nothing is focused, so this enters from the end: the last group isn't
  // built, and when it arrives neither is its last row. Both are waited
  // for, rather than settling for the last row the group happens to show.
  REQUIRE_FALSE(rows.realized(5));
  rows.tab(false);
  CHECK(rows.focusedCell() == 35);
  rows.tab(false);
  CHECK(rows.focusedCell() == 34);
}

TEST_CASE(
    "A group recycled while a row in it held focus reports that row's blur",
    "[component_list][focus][virtual_focus]")
{
  NestedRows rows;
  rows.tab();
  rows.tab();
  REQUIRE(rows.focusedCell() == 1);
  REQUIRE(rows.readingFocused() == "1 ");

  // The blur is queued on the machine of a row the group hosts, not on the
  // group's own, and the group gets no advance once it is pooled.
  rows.scroll->offsetY(-400.0f);
  rows.settle();
  REQUIRE_FALSE(rows.realized(0));
  CHECK(rows.focusedCell() == -1);
  CHECK(rows.readingFocused() == "");
}
