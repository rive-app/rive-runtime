#include <rive/file.hpp>
#include <rive/custom_property_trigger.hpp>
#include <rive/artboard.hpp>
#include <rive/artboard_component_list.hpp>
#include <rive/animation/state_machine_instance.hpp>
#include <rive/nested_artboard.hpp>
#include <rive/shapes/paint/paint_image.hpp>
#include <rive/shapes/paint/solid_color.hpp>
#include <rive/shapes/shape.hpp>
#include <rive/solo.hpp>
#include <rive/viewmodel/viewmodel_instance.hpp>
#include <rive/viewmodel/viewmodel_instance_boolean.hpp>
#include <rive/viewmodel/viewmodel_instance_list.hpp>
#include <rive/viewmodel/viewmodel_instance_list_item.hpp>
#include <rive/viewmodel/viewmodel_instance_number.hpp>
#include <rive/viewmodel/viewmodel_instance_trigger.hpp>
#include <rive/viewmodel/viewmodel_instance_viewmodel.hpp>
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <tuple>
#include <utility>
#include <vector>

// An ArtboardComponentList skips the per-frame work of rows that would do
// nothing ("quiet" rows) until something wakes them. Under TESTING every
// skipped row pass is still run, after the pass, and aborts if it did
// anything, so the whole suite checks the skipping; these cases pin down that
// rows do go quiet and that what the host pushes into them wakes them.

using namespace rive;

namespace
{
struct RowLayout
{
  float width;
  float height;
  float minX;
  float minY;
};

std::vector<RowLayout> rowLayouts(ArtboardComponentList* list)
{
  std::vector<RowLayout> rows;
  for (size_t i = 0; i < list->artboardCount(); i++)
  {
    auto row = list->artboardInstance((int)i);
    auto bounds = list->layoutBoundsForNode((int)i);
    rows.push_back(
        {row->layoutWidth(), row->layoutHeight(), bounds.minX, bounds.minY});
  }
  return rows;
}

bool operator==(const RowLayout& a, const RowLayout& b)
{
  return a.width == b.width && a.height == b.height && a.minX == b.minX &&
         a.minY == b.minY;
}

// Plays "Main" in artboard_list_overrides.riv until its rows could go quiet,
// halves the artboard's width, lets the layout settle, and returns where the
// rows were before and after, with quiet rows on or off.
std::pair<std::vector<RowLayout>, std::vector<RowLayout>> resizeHost(
    File* file,
    bool quietRows)
{
  ArtboardComponentList::sm_quietRowsEnabled = quietRows;
  auto artboard = file->artboardNamed("Main");
  REQUIRE(artboard != nullptr);
  auto stateMachine = artboard->stateMachineAt(0);
  auto viewModelId = artboard->viewModelId();
  auto viewModelInstance = viewModelId == -1
                               ? file->createViewModelInstance(artboard.get())
                               : file->createViewModelInstance(viewModelId, 0);
  stateMachine->bindViewModelInstance(viewModelInstance);
  stateMachine->advanceAndApply(0.1f);

  auto lists = artboard->find<ArtboardComponentList>();
  REQUIRE(lists.size() == 1);
  auto list = lists[0];
  REQUIRE(list->artboardCount() > 0);

  // Longer than the one-second empty animation a state without one plays,
  // which keeps its layer going until it ends.
  auto skips = ArtboardComponentList::sm_quietRowSkips;
  for (int i = 0; i < 90; i++)
  {
    stateMachine->advanceAndApply(1.0f / 60.0f);
  }
  CHECK((ArtboardComponentList::sm_quietRowSkips > skips) == quietRows);
  for (size_t i = 0; i < list->artboardCount(); i++)
  {
    CHECK(
        (list->artboardInstance((int)i)->quietHostRow() == (uint32_t)i) ==
        quietRows);
  }

  auto before = rowLayouts(list);
  artboard->width(artboard->width() * 0.5f);
  for (int i = 0; i < 60; i++)
  {
    stateMachine->advanceAndApply(1.0f / 60.0f);
  }

  auto after = rowLayouts(list);
  ArtboardComponentList::sm_quietRowsEnabled = true;
  return {before, after};
}
} // namespace

TEST_CASE(
    "Quiet list rows wake for layout the host pushes into them",
    "[component_list]")
{
  auto file = ReadRiveFile("assets/artboard_list_overrides.riv");
  auto quiet = resizeHost(file.get(), true);
  auto awake = resizeHost(file.get(), false);
  // The resize must move the rows, or matching proves nothing.
  CHECK_FALSE(quiet.first == quiet.second);
  REQUIRE(quiet.second.size() == awake.second.size());
  CHECK(quiet.second == awake.second);
}

namespace
{
// quiet_hosting_rows.riv's Main (or another file's, built the same way),
// bound and played for 1.5 s: long enough for every row, and what each row
// hosts, to have nothing left to do.
struct HostingRows
{
  rcp<File> file;
  std::unique_ptr<ArtboardInstance> artboard;
  std::unique_ptr<StateMachineInstance> machine;
  rcp<ViewModelInstance> viewModelInstance;
  ArtboardComponentList* list = nullptr;

  explicit HostingRows(
      bool quietRows,
      const char* path = "assets/quiet_hosting_rows.riv")
  {
    ArtboardComponentList::sm_quietRowsEnabled = quietRows;
    file = ReadRiveFile(path);
    artboard = file->artboardNamed("Main");
    REQUIRE(artboard != nullptr);
    viewModelInstance = file->createViewModelInstance(
        artboard->viewModelId(),
        0);
    machine = artboard->stateMachineAt(0);
    machine->bindViewModelInstance(viewModelInstance);
    list = artboard->find<ArtboardComponentList>("Rows");
    REQUIRE(list != nullptr);
    advance(90);
    REQUIRE(list->artboardCount() == 2);
  }

  ~HostingRows() { ArtboardComponentList::sm_quietRowsEnabled = true; }

  void advance(int frames)
  {
    for (int i = 0; i < frames; i++)
    {
      machine->advanceAndApply(1.0f / 60.0f);
    }
  }

  ArtboardInstance* row(int index) { return list->artboardInstance(index); }

  ArtboardInstance* badge(int index)
  {
    return row(index)->nestedArtboard("Badge")->artboardInstance();
  }

  ArtboardComponentList* tags(int index)
  {
    return row(index)->find<ArtboardComponentList>("Tags");
  }

  ViewModelInstance* item(int index)
  {
    return list->listItem(index)->viewModelInstance().get();
  }

  // What each row draws that can change: its badge's color and where each
  // of its tags puts its box.
  std::vector<double> drawn()
  {
    std::vector<double> values;
    for (int i = 0; i < 2; i++)
    {
      values.push_back(
          (uint32_t)badge(i)->find<SolidColor>("Color")->colorValue());
      auto rowTags = tags(i);
      for (size_t t = 0; t < rowTags->artboardCount(); t++)
      {
        values.push_back(
            rowTags->artboardInstance((int)t)->find<Shape>("Box")->x());
      }
    }
    return values;
  }
};
} // namespace

// A row that hosts a nested artboard and a list goes quiet once everything
// in it would do nothing: the nested artboard, its state machine, and every
// row of the list. What it hosts is marked with it, so a change there wakes
// it.
TEST_CASE("List rows that host artboards go quiet", "[component_list]")
{
  HostingRows scene(true);
  auto skips = ArtboardComponentList::sm_quietRowSkips;
  scene.advance(10);
  CHECK(ArtboardComponentList::sm_quietRowSkips > skips);
  for (int i = 0; i < 2; i++)
  {
    CHECK(scene.row(i)->quietHostRow() == (uint32_t)i);
    CHECK(scene.badge(i)->quietHostRow() == 0);
    auto rowTags = scene.tags(i);
    for (size_t t = 0; t < rowTags->artboardCount(); t++)
    {
      CHECK(rowTags->artboardInstance((int)t)->quietHostRow() == t);
    }
  }
}

// A quiet row wakes when something it hosts changes: its nested artboard's
// state machine reading the item, or a row of its nested list reading a tag.
// The rows end up as they do without quiet rows.
TEST_CASE("Quiet rows wake for changes to what they host", "[component_list]")
{
  auto play = [](bool quietRows) {
    HostingRows scene(quietRows);
    if (quietRows)
    {
      REQUIRE(scene.row(0)->quietHostRow() == 0);
    }
    std::vector<std::vector<double>> frames;
    frames.push_back(scene.drawn());
    // The first item goes online: its badge's machine turns it green.
    scene.item(0)
        ->propertyValue("online")
        ->as<ViewModelInstanceBoolean>()
        ->propertyValue(true);
    scene.advance(30);
    frames.push_back(scene.drawn());
    // The second item's tag moves.
    auto tagList = scene.item(1)
                       ->propertyValue("tags")
                       ->as<ViewModelInstanceList>();
    tagList->listItems()[0]
        ->viewModelInstance()
        ->propertyValue("n")
        ->as<ViewModelInstanceNumber>()
        ->propertyValue(30.0f);
    scene.advance(30);
    frames.push_back(scene.drawn());
    return frames;
  };
  auto quiet = play(true);
  auto awake = play(false);
  CHECK(quiet == awake);
  // The changes have to show, or matching proves nothing.
  REQUIRE(quiet.size() == 3);
  CHECK(quiet[1][0] == 0xFF3BA55C);
  CHECK(quiet[2].back() == 30.0f);
}

// A row's click listener selects its item through a bind the row's machine
// polls, as a view model reference can't push its changes. A list never runs
// its rows' binds target to source, so a clean polled bind is no work, and
// the row still goes quiet. Clicking it selects the item all the same.
TEST_CASE(
    "A quiet row's polled bind still selects its item",
    "[component_list]")
{
  HostingRows scene(true);
  REQUIRE(scene.row(1)->quietHostRow() == 1);
  auto selected = scene.viewModelInstance->propertyValue("selected")
                      ->as<ViewModelInstanceViewModel>();
  REQUIRE(selected->referenceViewModelInstance().get() != scene.item(1));

  // The second row, below the first.
  scene.machine->pointerDown(Vec2D(150.0f, 90.0f));
  scene.machine->pointerUp(Vec2D(150.0f, 90.0f));
  scene.advance(2);
  CHECK(selected->referenceViewModelInstance().get() == scene.item(1));

  scene.advance(30);
  CHECK(scene.row(1)->quietHostRow() == 1);
}

// A paused nested artboard doesn't advance, so a row that hosts one stays
// awake, and pausing or resuming it directly wakes the row. Paused on its own,
// the quiet row wakes; the badge, paused while its item went online, turns
// green once it plays again, and the row goes quiet again after.
TEST_CASE("Rows that host a paused artboard resume with it", "[component_list]")
{
  auto play = [](bool quietRows) {
    HostingRows scene(quietRows);
    auto badge = scene.row(0)->nestedArtboard("Badge");
    bool quietBefore = scene.row(0)->quietHostRow() == 0;
    badge->isPaused(true);
    // Nothing else changes: the pause alone has to wake the row.
    scene.advance(1);
    scene.item(0)
        ->propertyValue("online")
        ->as<ViewModelInstanceBoolean>()
        ->propertyValue(true);
    scene.advance(30);
    auto paused = (uint32_t)scene.badge(0)
                      ->find<SolidColor>("Color")
                      ->colorValue();
    badge->isPaused(false);
    scene.advance(30);
    auto resumed = (uint32_t)scene.badge(0)
                       ->find<SolidColor>("Color")
                       ->colorValue();
    scene.advance(60);
    bool quietAfter = scene.row(0)->quietHostRow() == 0;
    return std::make_tuple(quietBefore, paused, resumed, quietAfter);
  };
  auto quiet = play(true);
  auto awake = play(false);
  CHECK(std::get<0>(quiet));
  CHECK(std::get<3>(quiet));
  CHECK(std::get<1>(quiet) == std::get<1>(awake));
  CHECK(std::get<2>(quiet) == std::get<2>(awake));
  CHECK(std::get<1>(awake) == 0xFF808080);
  CHECK(std::get<2>(awake) == 0xFF3BA55C);
}

// A quantized nested artboard is never quiet, so quantizing one directly
// wakes the quiet row that hosts it, which then stays awake.
TEST_CASE(
    "Rows that host a quantized artboard wake with it",
    "[component_list]")
{
  HostingRows scene(true);
  REQUIRE(scene.row(0)->quietHostRow() == 0);
  scene.row(0)->nestedArtboard("Badge")->quantize(30);
  scene.advance(2);
  CHECK(scene.row(0)->quietHostRow() == Artboard::kNoQuietRow);
  CHECK(scene.row(1)->quietHostRow() == 1);
}

// A nested artboard can be handed an instance directly. A quiet row that
// hosts it, empty until then, wakes to advance the new instance, and goes
// quiet again with it marked.
TEST_CASE(
    "Rows wake when a nested artboard they host mounts an instance",
    "[component_list]")
{
  HostingRows scene(true);
  auto row = scene.row(0);
  REQUIRE(row->quietHostRow() == 0);
  auto slot = row->nestedArtboard("Slot");
  REQUIRE(slot != nullptr);
  REQUIRE(slot->artboardInstance() == nullptr);
  slot->referencedArtboard(scene.file->artboardNamed("Badge").release());
  CHECK(row->quietHostRow() == Artboard::kNoQuietRow);
  scene.advance(30);
  CHECK(row->quietHostRow() == 0);
  CHECK(slot->artboardInstance()->quietHostRow() == 0);
}

// A collapsed host only notes where the change sequence stands, so the state
// machines inside don't take a change made while they couldn't run once they
// do. A row may skip that only while nothing inside needs the note:
// - The Pinger and the Buzzer read pending changes, a view model trigger and
//   a component trigger, and not every such change wakes anything (a
//   component trigger fired directly, or a second fire while the first one's
//   bind is still dirty). A row that hides one stays awake.
// - The tags in a list in the row, and the held tags in a list in Holder,
//   are list rows, which go quiet only while every change their machines
//   read wakes them. The wake reaches the row, whose next advance notes the
//   change. A row that hides only one of those lists goes quiet. No other
//   list reads the held tags, so their wake is the only one the row gets.
// Each fires twice while hidden, 30 frames apart, and doesn't play once shown;
// a fire after does.
TEST_CASE(
    "Quiet rows with collapsed hosts don't replay old changes",
    "[component_list]")
{
  enum class Fired
  {
    pinger,
    buzzer,
    tags,
    heldTags
  };
  auto play = [](bool quietRows, Fired fired) {
    HostingRows scene(quietRows, "assets/quiet_inactive_hosts.riv");
    auto list = fired == Fired::buzzer
                    ? scene.artboard->find<ArtboardComponentList>("Buzz Rows")
                    : scene.list;
    REQUIRE(list != nullptr);
    auto row = list->artboardInstance(0);
    REQUIRE(row != nullptr);
    if (fired == Fired::heldTags)
    {
      auto holder = row->nestedArtboard("Holder")->artboardInstance();
      REQUIRE(holder != nullptr);
      auto heldTags = holder->find<ArtboardComponentList>("Held Tags");
      REQUIRE(heldTags != nullptr);
      REQUIRE(heldTags->artboardCount() == 1);
    }
    if (fired == Fired::buzzer)
    {
      auto buzzer = row->nestedArtboard("Buzzer")->artboardInstance();
      REQUIRE(buzzer != nullptr);
      REQUIRE(buzzer->find<CustomPropertyTrigger>("Buzz") != nullptr);
    }
    auto color = [](ArtboardInstance* artboard) {
      return (double)(uint32_t)artboard->find<SolidColor>("Color")
          ->colorValue();
    };
    auto boxX = [](ArtboardComponentList* tags) {
      return (double)tags->artboardInstance(0)->find<Shape>("Box")->x();
    };
    auto drawn = [&]() -> std::vector<double> {
      switch (fired)
      {
        case Fired::pinger:
          return {color(row->nestedArtboard("Pinger")->artboardInstance())};
        case Fired::buzzer:
          return {color(row->nestedArtboard("Buzzer")->artboardInstance())};
        case Fired::tags:
          return {boxX(row->find<ArtboardComponentList>("Tags"))};
        case Fired::heldTags:
          return {boxX(row->nestedArtboard("Holder")
                           ->artboardInstance()
                           ->find<ArtboardComponentList>("Held Tags"))};
      }
      return {};
    };
    auto fire = [&]() {
      auto item = scene.item(0);
      switch (fired)
      {
        case Fired::pinger:
          break;
        case Fired::buzzer:
        {
          auto buzz = row->nestedArtboard("Buzzer")
                          ->artboardInstance()
                          ->find<CustomPropertyTrigger>("Buzz");
          buzz->propertyValue(buzz->propertyValue() + 1);
          return;
        }
        case Fired::tags:
        case Fired::heldTags:
          item = item->propertyValue(fired == Fired::tags ? "tags" : "heldTags")
                     ->as<ViewModelInstanceList>()
                     ->listItems()[0]
                     ->viewModelInstance()
                     .get();
          break;
      }
      item->propertyValue("ping")->as<ViewModelInstanceTrigger>()->trigger();
    };
    auto solo = row->find<Solo>(
        fired == Fired::tags       ? "Tag Switch"
        : fired == Fired::heldTags ? "Holder Switch"
                                   : "Switch");
    solo->updateByName("Off");
    scene.advance(30);
    bool quietWhileHidden = row->quietHostRow() == 0;
    fire();
    scene.advance(30);
    fire();
    scene.advance(30);
    solo->updateByName("On");
    scene.advance(30);
    auto shown = drawn();
    fire();
    scene.advance(30);
    return std::make_tuple(quietWhileHidden, shown, drawn());
  };
  for (auto fired :
       {Fired::pinger, Fired::buzzer, Fired::tags, Fired::heldTags})
  {
    bool hidesListRows = fired == Fired::tags || fired == Fired::heldTags;
    auto quiet = play(true, fired);
    auto awake = play(false, fired);
    // Only a row that hides nothing but list rows goes quiet. Its being
    // quiet is what the tags cases test.
    CHECK(std::get<0>(quiet) == hidesListRows);
    CHECK(std::get<1>(quiet) == std::get<1>(awake));
    CHECK(std::get<2>(quiet) == std::get<2>(awake));
    // Still, then played: grey then gold, or each tag at 0 then 30.
    if (hidesListRows)
    {
      CHECK(std::get<1>(awake) == std::vector<double>{0.0});
      CHECK(std::get<2>(awake) == std::vector<double>{30.0});
    }
    else
    {
      CHECK(std::get<1>(awake) == std::vector<double>{0xFF808080});
      CHECK(std::get<2>(awake) == std::vector<double>{0xFFE0A557});
    }
  }
}

// A paused host, like a collapsed one, only notes where the change sequence
// stands, so a row that hosts one stays awake while it's paused. A ping fired
// at a paused Pinger has had its frame, and doesn't play once it's unpaused;
// one fired after does.
TEST_CASE(
    "Quiet rows with paused hosts don't replay old changes",
    "[component_list]")
{
  auto play = [](bool quietRows) {
    HostingRows scene(quietRows, "assets/quiet_inactive_hosts.riv");
    auto row = scene.row(0);
    auto host = row->nestedArtboard("Pinger");
    auto pinger = host->artboardInstance();
    auto drawn = [&]() {
      return (uint32_t)pinger->find<SolidColor>("Color")->colorValue();
    };
    auto ping = [&]() {
      scene.item(0)
          ->propertyValue("ping")
          ->as<ViewModelInstanceTrigger>()
          ->trigger();
    };
    bool quietBefore = row->quietHostRow() == 0;
    host->isPaused(true);
    scene.advance(30);
    ping();
    scene.advance(30);
    host->isPaused(false);
    scene.advance(30);
    auto resumed = drawn();
    ping();
    scene.advance(30);
    return std::make_tuple(quietBefore, resumed, drawn());
  };
  auto quiet = play(true);
  auto awake = play(false);
  // The row has to be quiet before the pause, or this proves nothing.
  CHECK(std::get<0>(quiet));
  CHECK(std::get<1>(quiet) == std::get<1>(awake));
  CHECK(std::get<2>(quiet) == std::get<2>(awake));
  // Still, then played: grey, then gold.
  CHECK(std::get<1>(awake) == 0xFF808080);
  CHECK(std::get<2>(awake) == 0xFFE0A557);
}

// An artboard paint's source isn't marked with what a row hosts, so a change
// only its machine reads wakes nothing. A row whose collapsed host draws one
// stays awake. Its Flasher starts hidden: a flash fired then has had its frame,
// and doesn't play once the Flasher shows; one fired after does.
TEST_CASE(
    "Quiet rows with collapsed artboard paints don't replay old changes",
    "[component_list]")
{
  auto play = [](bool quietRows) {
    HostingRows scene(quietRows, "assets/quiet_inactive_hosts.riv");
    auto list = scene.artboard->find<ArtboardComponentList>("Paint Rows");
    REQUIRE(list != nullptr);
    REQUIRE(list->artboardCount() == 1);
    auto row = list->artboardInstance(0);
    auto flasher = row->nestedArtboard("Flasher")->artboardInstance();
    auto source = flasher->find<PaintImage>("Flash Paint")->artboardInstance();
    REQUIRE(source != nullptr);
    auto drawn = [&]() {
      return (uint32_t)source->find<SolidColor>("Color")->colorValue();
    };
    auto flash = [&]() {
      list->listItem(0)
          ->viewModelInstance()
          ->propertyValue("flash")
          ->as<ViewModelInstanceTrigger>()
          ->trigger();
    };
    flash();
    scene.advance(30);
    row->find<Solo>("Switch")->updateByName("On");
    scene.advance(30);
    auto shown = drawn();
    flash();
    scene.advance(30);
    return std::make_pair(shown, drawn());
  };
  auto quiet = play(true);
  auto awake = play(false);
  CHECK(quiet == awake);
  // Still, then played: grey, then gold.
  CHECK(awake.first == 0xFF808080);
  CHECK(awake.second == 0xFFE0A557);
}
