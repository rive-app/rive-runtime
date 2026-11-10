// A list row that hosts a list leading back to rows of itself used to build
// rows until the stack overflowed (Sentry RIVE_NATIVE-18W and its siblings,
// all EXCEPTION_STACK_OVERFLOW in ArtboardComponentList::bindArtboard). The
// fixture's markup is assets/rml/recursive_lists/scene.rml.

#include "rive/animation/state_machine_instance.hpp"
#include "rive/artboard_component_list.hpp"
#include "rive/file.hpp"
#include "rive/nested_artboard.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <string>

namespace
{
struct RecursiveListScene
{
  rive::rcp<rive::File> file;
  std::unique_ptr<rive::ArtboardInstance> artboard;
  std::unique_ptr<rive::StateMachineInstance> machine;
  rive::rcp<rive::ViewModelInstance> viewModelInstance;
  rive::ArtboardComponentList* list = nullptr;

  explicit RecursiveListScene(const char* artboardName)
  {
    file = ReadRiveFile("assets/recursive_lists.riv");
    artboard = file->artboardNamed(artboardName);
    REQUIRE(artboard != nullptr);
    viewModelInstance = file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    artboard->bindViewModelInstance(viewModelInstance);
    machine = artboard->stateMachineAt(0);
    REQUIRE(machine != nullptr);
    machine->bindViewModelInstance(viewModelInstance);
    machine->advanceAndApply(0.0f);
    list = artboard->find<rive::ArtboardComponentList>("Rows");
    REQUIRE(list != nullptr);
  }
};

rive::ArtboardComponentList* rowList(rive::ArtboardInstance* row)
{
  REQUIRE(row != nullptr);
  REQUIRE(row->artboardComponentLists().size() == 1);
  return row->artboardComponentLists()[0];
}

// How many rows deep a chain of one-row lists goes.
uint32_t chainDepth(rive::ArtboardComponentList* list)
{
  uint32_t depth = 0;
  while (true)
  {
    REQUIRE(list->artboardCount() == 1);
    auto* row = list->artboardInstance(0);
    if (row == nullptr)
    {
      return depth;
    }
    depth++;
    REQUIRE(depth <= rive::ArtboardComponentList::maxRowNesting);
    list = rowList(row);
  }
}
} // namespace

TEST_CASE(
    "A row's list bound to the list that made it builds no rows of itself",
    "[recursive_lists]")
{
  RecursiveListScene scene("Feed");
  scene.machine->advanceAndApply(0.016f);

  REQUIRE(scene.list->artboardCount() == 2);
  for (int i = 0; i < 2; i++)
  {
    // The Card row is there, and its list has the Feed's items, but holds
    // no Card for either: each would hold every Card again, itself included.
    auto* inner = rowList(scene.list->artboardInstance(i));
    CHECK(inner->artboardCount() == 2);
    CHECK(inner->artboardInstance(0) == nullptr);
    CHECK(inner->artboardInstance(1) == nullptr);
  }
  // The file counts them, so an editor can say why rows are missing.
  CHECK(scene.file->refusedNestedListRows() == 4);
}

TEST_CASE(
    "Rows that list new rows of themselves stop at maxRowNesting",
    "[recursive_lists]")
{
  RecursiveListScene scene("Chain");
  scene.machine->advanceAndApply(0.016f);

  // Every Link lists one new Link. Nothing repeats -- each row's item is a
  // new default instance -- so only the depth stops it.
  CHECK(chainDepth(scene.list) == rive::ArtboardComponentList::maxRowNesting);
  // The deepest Link's one row.
  CHECK(scene.file->refusedNestedListRows() == 1);
}

TEST_CASE(
    "A tree of rows of one artboard builds every level its data has",
    "[recursive_lists]")
{
  RecursiveListScene scene("Forest");
  scene.machine->advanceAndApply(0.016f);

  // Forest lists A; A lists B and C; B lists D; C and D list nothing.
  REQUIRE(scene.list->artboardCount() == 1);
  auto* aList = rowList(scene.list->artboardInstance(0));
  REQUIRE(aList->artboardCount() == 2);
  auto* bList = rowList(aList->artboardInstance(0));
  auto* cList = rowList(aList->artboardInstance(1));
  CHECK(cList->artboardCount() == 0);
  REQUIRE(bList->artboardCount() == 1);
  auto* dList = rowList(bList->artboardInstance(0));
  CHECK(dList->artboardCount() == 0);
  CHECK(scene.file->refusedNestedListRows() == 0);
}

TEST_CASE(
    "A row may list the list that made it when it shows other rows",
    "[recursive_lists]")
{
  RecursiveListScene scene("Shelf");
  scene.machine->advanceAndApply(0.016f);

  // Each Page lists Shelf.pages again, as Tabs.
  REQUIRE(scene.list->artboardCount() == 2);
  for (int i = 0; i < 2; i++)
  {
    auto* inner = rowList(scene.list->artboardInstance(i));
    REQUIRE(inner->artboardCount() == 2);
    for (int j = 0; j < 2; j++)
    {
      auto* tab = inner->artboardInstance(j);
      REQUIRE(tab != nullptr);
      CHECK(tab->name() == "Tab");
    }
  }
  CHECK(scene.file->refusedNestedListRows() == 0);
}

TEST_CASE(
    "A list a converter makes from the list that made its row stops at "
    "maxRowNesting",
    "[recursive_lists]")
{
  RecursiveListScene scene("Album");
  scene.machine->advanceAndApply(0.016f);

  // Every Photo's list turns Album.photos into one new Photo. The source
  // repeats, but a converter's list can depend on the row it's in, so the
  // repeat alone doesn't stop it: the depth does.
  CHECK(chainDepth(scene.list) == rive::ArtboardComponentList::maxRowNesting);
  CHECK(scene.file->refusedNestedListRows() == 1);
}

TEST_CASE(
    "A virtualized row's list bound to the list that made it realizes none "
    "of its rows",
    "[recursive_lists]")
{
  RecursiveListScene scene("Wall");
  // Each pass realizes what the Tiles' lists show, and tries a refused row
  // again, outside the Tile's own build.
  for (int frame = 0; frame < 10; frame++)
  {
    scene.machine->advanceAndApply(0.016f);
  }

  REQUIRE(scene.list->artboardCount() == 2);
  for (int i = 0; i < 2; i++)
  {
    auto* inner = rowList(scene.list->artboardInstance(i));
    CHECK(inner->artboardCount() == 2);
    CHECK(inner->artboardInstance(0) == nullptr);
    CHECK(inner->artboardInstance(1) == nullptr);
  }
  CHECK(scene.file->refusedNestedListRows() > 0);
}

TEST_CASE(
    "A list under more nested artboards than maxRowNesting keeps its rows",
    "[recursive_lists]")
{
  auto file = ReadRiveFile("assets/recursive_lists.riv");
  auto artboard = file->artboardNamed("Deep");
  REQUIRE(artboard != nullptr);
  auto viewModelInstance = file->createDefaultViewModelInstance(artboard.get());
  REQUIRE(viewModelInstance != nullptr);
  artboard->bindViewModelInstance(viewModelInstance);
  auto machine = artboard->stateMachineAt(0);
  REQUIRE(machine != nullptr);
  machine->bindViewModelInstance(viewModelInstance);
  machine->advanceAndApply(0.0f);
  machine->advanceAndApply(0.016f);

  // Down through the nested artboards to the one with the list.
  const uint32_t levels = 16;
  REQUIRE(levels >= rive::ArtboardComponentList::maxRowNesting);
  rive::ArtboardInstance* level = artboard.get();
  for (uint32_t i = 0; i < levels; i++)
  {
    REQUIRE(level->nestedArtboards().size() == 1);
    level = level->nestedArtboards()[0]->artboardInstance();
    REQUIRE(level != nullptr);
  }
  auto* list = rowList(level);
  REQUIRE(list->artboardCount() == 2);
  CHECK(list->artboardInstance(0) != nullptr);
  CHECK(list->artboardInstance(1) != nullptr);
  CHECK(file->refusedNestedListRows() == 0);
}
