#include "rive/animation/state_machine_instance.hpp"
#include "rive/artboard_component_list.hpp"
#include "rive/input/focus_manager.hpp"
#include "rive/input/focus_node.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/viewmodel/viewmodel_instance_list.hpp"
#include "rive/viewmodel/viewmodel_instance_list_item.hpp"
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include <catch.hpp>
#include <algorithm>
#include <unordered_map>
#include <vector>

// A component list keeps one focus row node per item under its focus scope.
// These tests pin what that scope holds after each kind of list change, and
// that keeping it in step costs work in proportion to the list, not to its
// square.

namespace
{
// An item's row, with the item itself: holding the item keeps its address
// from being handed to a new item once it leaves the list, which would make
// the new item look like one that stayed.
struct ItemRow
{
  rive::rcp<rive::ViewModelInstanceListItem> item;
  rive::rcp<rive::FocusNode> row;
};
using RowsByItem = std::
    unordered_map<rive::ViewModelInstanceListItem*, ItemRow>;

struct FocusRowList
{
  rive::rcp<rive::File> file;
  std::unique_ptr<rive::ArtboardInstance> artboard;
  std::unique_ptr<rive::StateMachineInstance> stateMachine;
  rive::rcp<rive::ViewModelInstance> viewModelInstance;
  rive::ArtboardComponentList* list = nullptr;
  rive::ViewModelInstanceList* items = nullptr;
  rive::ViewModel* itemViewModel = nullptr;

  FocusRowList()
  {
    file = ReadRiveFile("assets/component_list_virtualized.riv");
    artboard = file->artboardNamed("Main");
    REQUIRE(artboard != nullptr);
    viewModelInstance = file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    artboard->bindViewModelInstance(viewModelInstance);
    stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);
    stateMachine->bindViewModelInstance(viewModelInstance);
    stateMachine->advanceAndApply(0.016f);

    list = artboard->find<rive::ArtboardComponentList>("List");
    REQUIRE(list != nullptr);
    REQUIRE(list->listScopeFocusNode() != nullptr);
    for (auto& value : viewModelInstance->propertyValues())
    {
      if (value->is<rive::ViewModelInstanceList>())
      {
        items = value->as<rive::ViewModelInstanceList>();
        break;
      }
    }
    REQUIRE(items != nullptr);
    REQUIRE(items->listItems().size() > 0);
    itemViewModel = items->listItems()[0]->viewModelInstance()->viewModel();
  }

  rive::rcp<rive::ViewModelInstanceListItem> makeItem()
  {
    auto item = rive::make_rcp<rive::ViewModelInstanceListItem>();
    item->viewModelInstance(
        file->createDefaultViewModelInstance(itemViewModel));
    return item;
  }

  void advance() { stateMachine->advanceAndApply(0.016f); }

  // Grows or shrinks the list to `count` items and settles it.
  void resize(size_t count)
  {
    while (items->listItems().size() > count)
    {
      items->pop();
    }
    while (items->listItems().size() < count)
    {
      items->addItem(makeItem());
    }
    advance();
    checkRows();
  }

  // The row each item has now. Holds the rows, so one that leaves the list
  // can still be asked where it went, and the items (see ItemRow).
  RowsByItem rowsByItem() const
  {
    RowsByItem rows;
    const auto& rowNodes = list->listRowFocusNodes();
    auto listItems = items->listItems();
    for (size_t i = 0; i < listItems.size() && i < rowNodes.size(); i++)
    {
      rows[listItems[i].get()] = {listItems[i], rowNodes[i]};
    }
    return rows;
  }

  // The scope's children are the rows, one per item, in list order.
  void checkRows() const
  {
    auto scope = list->listScopeFocusNode();
    REQUIRE(scope != nullptr);
    const auto& rows = list->listRowFocusNodes();
    const auto& children = scope->children();
    REQUIRE(rows.size() == items->listItems().size());
    REQUIRE(children.size() == rows.size());
    size_t inOrder = 0;
    size_t attached = 0;
    for (size_t i = 0; i < rows.size(); i++)
    {
      inOrder += children[i] == rows[i] ? 1 : 0;
      attached += rows[i] != nullptr && rows[i]->parent() == scope.get() &&
                          rows[i]->manager() == scope->manager()
                      ? 1
                      : 0;
    }
    CHECK(inOrder == rows.size());
    CHECK(attached == rows.size());
    std::vector<rive::FocusNode*> unique;
    for (const auto& row : rows)
    {
      unique.push_back(row.get());
    }
    std::sort(unique.begin(), unique.end());
    CHECK(std::adjacent_find(unique.begin(), unique.end()) == unique.end());
  }

  // Every item that was in the list before still has the row it had, and
  // the rows of the items that left are out of the focus tree.
  void checkKept(const RowsByItem& before) const
  {
    checkRows();
    const auto& rows = list->listRowFocusNodes();
    auto listItems = items->listItems();
    RowsByItem remaining = before;
    size_t kept = 0;
    size_t expected = 0;
    for (size_t i = 0; i < listItems.size(); i++)
    {
      auto itr = remaining.find(listItems[i].get());
      if (itr == remaining.end())
      {
        continue;
      }
      expected++;
      kept += itr->second.row == rows[i] ? 1 : 0;
      remaining.erase(itr);
    }
    CHECK(kept == expected);
    for (const auto& gone : remaining)
    {
      CHECK(gone.second.row->parent() == nullptr);
      CHECK(gone.second.row->manager() == nullptr);
    }
  }
};

// Work done to keep the focus rows in step with one list change.
struct RowSyncWork
{
  uint64_t childEdits;
  uint64_t compares;
};

template <typename Change> RowSyncWork measure(size_t count, Change change)
{
  FocusRowList rows;
  rows.resize(count);
  auto before = rows.rowsByItem();
  change(rows);
  rive::FocusNode::sm_childEdits = 0;
  rive::ArtboardComponentList::sm_focusRowCompares = 0;
  rows.advance();
  RowSyncWork work = {
      rive::FocusNode::sm_childEdits,
      rive::ArtboardComponentList::sm_focusRowCompares};
  rows.checkKept(before);
  return work;
}
} // namespace

TEST_CASE(
    "Component List focus rows follow appended items",
    "[component_list][focus_rows]")
{
  FocusRowList rows;
  auto before = rows.rowsByItem();
  const size_t count = rows.items->listItems().size();
  for (int i = 0; i < 5; i++)
  {
    rows.items->addItem(rows.makeItem());
  }
  rows.advance();
  REQUIRE(rows.items->listItems().size() == count + 5);
  rows.checkKept(before);
}

TEST_CASE(
    "Component List focus rows follow prepended items",
    "[component_list][focus_rows]")
{
  FocusRowList rows;
  auto before = rows.rowsByItem();
  for (int i = 0; i < 5; i++)
  {
    rows.items->addItemAt(rows.makeItem(), 0);
  }
  rows.advance();
  rows.checkKept(before);
  CHECK(before.count(rows.items->listItems()[0].get()) == 0);
  CHECK(before.count(rows.items->listItems()[5].get()) == 1);
}

TEST_CASE(
    "Component List focus rows follow items inserted in the middle",
    "[component_list][focus_rows]")
{
  FocusRowList rows;
  rows.resize(20);
  auto before = rows.rowsByItem();
  rows.items->addItemAt(rows.makeItem(), 7);
  rows.advance();
  rows.checkKept(before);

  // Several at once, in two places.
  before = rows.rowsByItem();
  rows.items->addItemAt(rows.makeItem(), 3);
  rows.items->addItemAt(rows.makeItem(), 3);
  rows.items->addItemAt(rows.makeItem(), 15);
  rows.advance();
  REQUIRE(rows.items->listItems().size() == 24);
  rows.checkKept(before);
}

TEST_CASE(
    "Component List focus rows follow removed items",
    "[component_list][focus_rows]")
{
  FocusRowList rows;
  rows.resize(20);
  auto before = rows.rowsByItem();
  rows.items->removeItem(7);
  rows.advance();
  REQUIRE(rows.items->listItems().size() == 19);
  rows.checkKept(before);

  // The first, the last, and two apart in the middle, at once.
  before = rows.rowsByItem();
  rows.items->shift();
  rows.items->pop();
  rows.items->removeItem(4);
  rows.items->removeItem(9);
  rows.advance();
  REQUIRE(rows.items->listItems().size() == 15);
  rows.checkKept(before);
}

TEST_CASE(
    "Component List focus rows follow swapped items",
    "[component_list][focus_rows]")
{
  FocusRowList rows;
  rows.resize(20);
  auto before = rows.rowsByItem();
  rows.items->swap(4, 5);
  rows.advance();
  rows.checkKept(before);

  before = rows.rowsByItem();
  rows.items->swap(0, 19);
  rows.advance();
  rows.checkKept(before);
}

TEST_CASE(
    "Component List focus rows follow a reordered long list",
    "[component_list][focus_rows]")
{
  // More rows change places than the list matches without a sorted index.
  FocusRowList rows;
  rows.resize(300);
  auto before = rows.rowsByItem();
  for (uint32_t i = 0; i < 150; i++)
  {
    rows.items->swap(i, 299 - i);
  }
  rows.advance();
  rows.checkKept(before);

  // Reordered, with some removed and some added, in one change.
  before = rows.rowsByItem();
  for (uint32_t i = 0; i < 100; i++)
  {
    rows.items->swap(i, 200 - i);
  }
  for (int i = 0; i < 40; i++)
  {
    rows.items->removeItem(10 + i * 3);
  }
  for (int i = 0; i < 25; i++)
  {
    rows.items->addItemAt(rows.makeItem(), 5 + i * 7);
  }
  rows.advance();
  REQUIRE(rows.items->listItems().size() == 285);
  rows.checkKept(before);
}

TEST_CASE(
    "Component List focus rows follow a cleared and refilled list",
    "[component_list][focus_rows]")
{
  FocusRowList rows;
  rows.resize(20);

  // Emptied, then refilled a frame later.
  auto before = rows.rowsByItem();
  rows.items->removeAllItems();
  rows.advance();
  CHECK(rows.list->listScopeFocusNode()->children().empty());
  rows.checkKept(before);
  for (int i = 0; i < 6; i++)
  {
    rows.items->addItem(rows.makeItem());
  }
  rows.advance();
  REQUIRE(rows.items->listItems().size() == 6);
  rows.checkKept(before);

  // Every item replaced within one frame, by fewer and then by more.
  before = rows.rowsByItem();
  rows.items->removeAllItems();
  for (int i = 0; i < 3; i++)
  {
    rows.items->addItem(rows.makeItem());
  }
  rows.advance();
  REQUIRE(rows.items->listItems().size() == 3);
  rows.checkKept(before);

  before = rows.rowsByItem();
  rows.items->removeAllItems();
  for (int i = 0; i < 100; i++)
  {
    rows.items->addItem(rows.makeItem());
  }
  rows.advance();
  REQUIRE(rows.items->listItems().size() == 100);
  rows.checkKept(before);

  // A long list replaced in one frame.
  before = rows.rowsByItem();
  rows.items->removeAllItems();
  for (int i = 0; i < 90; i++)
  {
    rows.items->addItem(rows.makeItem());
  }
  rows.advance();
  REQUIRE(rows.items->listItems().size() == 90);
  rows.checkKept(before);
}

TEST_CASE(
    "Component List focus rows follow a list that repeats an item",
    "[component_list][focus_rows]")
{
  FocusRowList rows;
  rows.resize(10);
  auto repeated = rows.items->listItems()[3];

  // The item shows on rows 2 and 4: one row node each.
  rows.items->addItemAt(repeated, 2);
  rows.advance();
  REQUIRE(rows.items->listItems().size() == 11);
  rows.checkRows();
  auto firstRow = rows.list->listRowFocusNodes()[2];
  auto secondRow = rows.list->listRowFocusNodes()[4];
  CHECK(firstRow != secondRow);

  // Changes elsewhere leave both of its rows where they are.
  rows.items->addItem(rows.makeItem());
  rows.advance();
  rows.checkRows();
  CHECK(rows.list->listRowFocusNodes()[2] == firstRow);
  CHECK(rows.list->listRowFocusNodes()[4] == secondRow);

  rows.items->addItemAt(rows.makeItem(), 0);
  rows.advance();
  rows.checkRows();
  CHECK(rows.list->listRowFocusNodes()[3] == firstRow);
  CHECK(rows.list->listRowFocusNodes()[5] == secondRow);

  // Moving one of the two keeps both rows, each used once.
  rows.items->swap(3, 8);
  rows.advance();
  rows.checkRows();
  {
    auto a = rows.list->listRowFocusNodes()[5];
    auto b = rows.list->listRowFocusNodes()[8];
    CHECK(
        ((a == firstRow && b == secondRow) ||
         (a == secondRow && b == firstRow)));
  }

  // A third showing gets a row of its own.
  rows.items->addItemAt(repeated, 1);
  rows.advance();
  REQUIRE(rows.items->listItems().size() == 14);
  rows.checkRows();

  // Taking showings away one at a time takes one row each, and the rows
  // that go leave the focus tree.
  auto scope = rows.list->listScopeFocusNode();
  std::vector<rive::rcp<rive::FocusNode>> shown;
  for (size_t i = 0; i < rows.items->listItems().size(); i++)
  {
    if (rows.items->listItems()[i] == repeated)
    {
      shown.push_back(rows.list->listRowFocusNodes()[i]);
    }
  }
  REQUIRE(shown.size() == 3);
  for (size_t left = 3; left > 0; left--)
  {
    // By index: removing by item takes every showing at once.
    auto listItems = rows.items->listItems();
    auto showingAt = std::find(listItems.begin(), listItems.end(), repeated);
    REQUIRE(showingAt != listItems.end());
    rows.items->removeItem((int)(showingAt - listItems.begin()));
    rows.advance();
    rows.checkRows();
    size_t attached = 0;
    for (const auto& row : shown)
    {
      if (row->parent() == scope.get())
      {
        attached++;
      }
      else
      {
        CHECK(row->parent() == nullptr);
        CHECK(row->manager() == nullptr);
      }
    }
    size_t showing = 0;
    for (auto& item : rows.items->listItems())
    {
      showing += item == repeated ? 1 : 0;
    }
    CHECK(attached == showing);
  }
}

TEST_CASE(
    "Component List focus row sync is linear in the list's length",
    "[component_list][focus_rows]")
{
  const size_t small = 1000;
  const size_t large = 4000;

  // Changes that touch a fixed number of rows: the rows touched under the
  // scope must not depend on how long the list is, and the item compares
  // that find them may only grow in step with it.
  auto checkFixedChange = [&](const char* name,
                              void (*change)(FocusRowList&),
                              uint64_t rowsTouched) {
    INFO(name);
    RowSyncWork atSmall = measure(small, change);
    RowSyncWork atLarge = measure(large, change);
    CHECK(atSmall.childEdits == rowsTouched);
    CHECK(atLarge.childEdits == rowsTouched);
    CHECK(atSmall.compares <= 2 * small + 200);
    CHECK(atLarge.compares <= 2 * large + 200);
  };

  checkFixedChange(
      "append",
      [](FocusRowList& rows) {
        for (int i = 0; i < 10; i++)
        {
          rows.items->addItem(rows.makeItem());
        }
      },
      10);
  checkFixedChange(
      "prepend",
      [](FocusRowList& rows) { rows.items->addItemAt(rows.makeItem(), 0); },
      1);
  checkFixedChange(
      "insert",
      [](FocusRowList& rows) {
        rows.items->addItemAt(
            rows.makeItem(),
            (int)(rows.items->listItems().size() / 2));
      },
      1);
  checkFixedChange(
      "remove",
      [](FocusRowList& rows) {
        rows.items->removeItem((int)(rows.items->listItems().size() / 2));
      },
      1);
  checkFixedChange(
      "swap",
      [](FocusRowList& rows) {
        uint32_t middle = (uint32_t)(rows.items->listItems().size() / 2);
        rows.items->swap(middle, middle + 1);
      },
      2);

  // Emptying the list takes every row out, once each.
  auto clear = [](FocusRowList& rows) { rows.items->removeAllItems(); };
  RowSyncWork clearSmall = measure(small, clear);
  RowSyncWork clearLarge = measure(large, clear);
  CHECK(clearSmall.childEdits == small);
  CHECK(clearLarge.childEdits == large);
  CHECK(clearSmall.compares <= 2 * small + 200);
  CHECK(clearLarge.compares <= 2 * large + 200);
}

TEST_CASE(
    "Component List focus stays on a row when the list changes around it",
    "[component_list][focus_rows]")
{
  auto file = ReadRiveFile("assets/semantic/data_binding_lists.riv");
  auto artboard = file->artboardDefault();
  REQUIRE(artboard != nullptr);
  auto stateMachine = artboard->stateMachineAt(0);
  REQUIRE(stateMachine != nullptr);
  auto viewModelInstance = file->createDefaultViewModelInstance(artboard.get());
  REQUIRE(viewModelInstance != nullptr);
  artboard->bindViewModelInstance(viewModelInstance);
  stateMachine->bindViewModelInstance(viewModelInstance);
  // Let the menu finish opening, so its rows can take focus.
  for (int frame = 0; frame < 10; frame++)
  {
    stateMachine->advanceAndApply(0.1f);
  }
  auto* focusManager = stateMachine->focusManager();
  REQUIRE(focusManager != nullptr);

  auto items = viewModelInstance->propertyValue("menu")
                   ->as<rive::ViewModelInstanceList>();
  REQUIRE(items->listItems().size() == 4);
  rive::ArtboardComponentList* list = nullptr;
  for (auto* candidate : artboard->artboardComponentLists())
  {
    if (candidate->artboardCount() > 0 &&
        candidate->listItem(0) == items->listItems()[0])
    {
      list = candidate;
    }
  }
  REQUIRE(list != nullptr);
  auto itemViewModel = items->listItems()[0]->viewModelInstance()->viewModel();
  auto makeItem = [&]() {
    auto item = rive::make_rcp<rive::ViewModelInstanceListItem>();
    item->viewModelInstance(
        file->createDefaultViewModelInstance(itemViewModel));
    return item;
  };
  auto settle = [&]() {
    for (int frame = 0; frame < 3; frame++)
    {
      stateMachine->advanceAndApply(0.016f);
    }
  };

  auto scope = list->listScopeFocusNode();
  REQUIRE(scope != nullptr);
  REQUIRE(scope->children().size() == 4);
  auto focusedItem = items->listItems()[1];
  auto row = scope->children()[1];
  REQUIRE(!row->children().empty());
  auto target = row->children()[0];
  focusManager->setFocus(target);
  settle();
  REQUIRE(focusManager->primaryFocus() == target);
  REQUIRE(row->hasFocus());

  auto checkStillFocused = [&](size_t index) {
    REQUIRE(items->listItems()[index] == focusedItem);
    REQUIRE(scope->children().size() == items->listItems().size());
    CHECK(scope->children()[index] == row);
    CHECK(focusManager->primaryFocus() == target);
    CHECK(row->hasFocus());
    CHECK(scope->hasFocus());
    CHECK(target->parent() == row.get());
  };

  // Appended to.
  items->addItem(makeItem());
  settle();
  checkStillFocused(1);

  // Pushed down by a new first item.
  items->addItemAt(makeItem(), 0);
  settle();
  checkStillFocused(2);

  // Rows removed on either side of it.
  items->removeItem(0);
  items->pop();
  settle();
  checkStillFocused(1);

  // Moved to another place in the list.
  items->swap(1, 3);
  settle();
  checkStillFocused(3);

  // Removed: the focus goes with it.
  items->removeItem(3);
  settle();
  CHECK(scope->children().size() == 3);
  CHECK(row->parent() == nullptr);
  CHECK(focusManager->primaryFocus() != target);
  CHECK_FALSE(scope->hasFocus());
}

TEST_CASE(
    "FocusManager::replaceChildren registers an incoming node's subtree",
    "[focus_rows][FocusManager]")
{
  rive::FocusManager manager;
  auto parent = rive::make_rcp<rive::FocusNode>();
  auto incoming = rive::make_rcp<rive::FocusNode>();
  manager.addChild(nullptr, parent);
  manager.addChild(nullptr, incoming);
  REQUIRE(incoming->manager() == &manager);

  // Attached through the node, which hands out no manager: the incoming
  // node is in the manager while its descendant is not.
  auto descendant = rive::make_rcp<rive::FocusNode>();
  incoming->addChild(descendant);
  REQUIRE(descendant->manager() == nullptr);

  manager.replaceChildren(parent, 0, 0, &incoming, 1);

  REQUIRE(parent->children().size() == 1);
  CHECK(parent->children()[0] == incoming);
  CHECK(incoming->parent() == parent.get());
  CHECK(incoming->manager() == &manager);
  CHECK(descendant->manager() == &manager);
}
