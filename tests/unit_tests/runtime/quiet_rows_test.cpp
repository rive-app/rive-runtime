#include <rive/file.hpp>
#include <rive/custom_property_trigger.hpp>
#include <rive/artboard.hpp>
#include <rive/artboard_component_list.hpp>
#include <rive/animation/state_machine_instance.hpp>
#include <rive/nested_artboard.hpp>
#include <rive/scripted/scripted_drawable.hpp>
#include <rive/scripted/scripted_path_effect.hpp>
#include <rive/shapes/paint/paint_image.hpp>
#include <rive/shapes/paint/solid_color.hpp>
#include <rive/shapes/paint/stroke.hpp>
#include <rive/shapes/shape.hpp>
#include <rive/shapes/shape_paint_path.hpp>
#include <rive/solo.hpp>
#include <rive/text/text.hpp>
#include <rive/text/text_style_global.hpp>
#include <rive/text/text_style_instance.hpp>
#include <rive/text/text_style_on_demand.hpp>
#include <rive/text/text_value_run.hpp>
#include <rive/viewmodel/viewmodel_instance.hpp>
#include <rive/viewmodel/viewmodel_instance_boolean.hpp>
#include <rive/viewmodel/viewmodel_instance_color.hpp>
#include <rive/viewmodel/viewmodel_instance_list.hpp>
#include <rive/viewmodel/viewmodel_instance_list_item.hpp>
#include <rive/viewmodel/viewmodel_instance_number.hpp>
#include <rive/viewmodel/viewmodel_instance_trigger.hpp>
#include <rive/viewmodel/viewmodel_instance_viewmodel.hpp>
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <functional>
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

namespace
{
// quiet_interpolator_rows.riv's Main: a list of Eased Rows, whose artboard
// binds ease, and a list of Gated Rows, whose machine's bind does.
struct EasedRows : HostingRows
{
  ArtboardComponentList* gates = nullptr;

  explicit EasedRows(bool quietRows)
      : HostingRows(quietRows, "assets/quiet_interpolator_rows.riv")
  {
    gates = artboard->find<ArtboardComponentList>("Gates");
    REQUIRE(gates != nullptr);
    REQUIRE(gates->artboardCount() == 2);
  }

  // Each Eased Row's Box x (n through Ease), y (n doubled, then eased,
  // through a group) and color (tint through Fade), then each Gated Row's
  // Flag color, which its machine turns green once its eased n reaches 50.
  std::vector<double> drawn()
  {
    std::vector<double> values;
    for (int i = 0; i < 2; i++)
    {
      auto box = row(i)->find<Shape>("Box");
      values.push_back(box->x());
      values.push_back(box->y());
      values.push_back(
          (uint32_t)row(i)->find<SolidColor>("Color")->colorValue());
    }
    for (int i = 0; i < 2; i++)
    {
      values.push_back((uint32_t)gates->artboardInstance(i)
                           ->find<SolidColor>("Flag Color")
                           ->colorValue());
    }
    return values;
  }

  bool allQuiet()
  {
    for (auto rows : {list, gates})
    {
      for (int i = 0; i < 2; i++)
      {
        if (rows->artboardInstance(i)->quietHostRow() != (uint32_t)i)
        {
          return false;
        }
      }
    }
    return true;
  }

  // Gives the first item, and the first gate, new values.
  void retarget(float n, int tint)
  {
    item(0)->propertyValue("n")->as<ViewModelInstanceNumber>()->propertyValue(
        n);
    item(0)->propertyValue("tint")->as<ViewModelInstanceColor>()->propertyValue(
        tint);
    gates->listItem(0)
        ->viewModelInstance()
        ->propertyValue("n")
        ->as<ViewModelInstanceNumber>()
        ->propertyValue(n);
  }
};
} // namespace

// A bind that eases through a DataConverterInterpolator, the row artboard's
// or its machine's, only advances while its value is on the way. A row whose
// eased values have all landed goes quiet.
TEST_CASE(
    "List rows with eased binds go quiet once their values land",
    "[component_list]")
{
  EasedRows scene(true);
  auto skips = ArtboardComponentList::sm_quietRowSkips;
  scene.advance(10);
  CHECK(ArtboardComponentList::sm_quietRowSkips > skips);
  CHECK(scene.allQuiet());
  CHECK(
      scene.drawn() == std::vector<double>{
                           10,
                           20,
                           0xFF808080,
                           60,
                           120,
                           0xFF2060E0,
                           0xFF808080,
                           0xFF3BA55C});
}

// A quiet row's eased values ease to a new value frame by frame as an awake
// row's do, through a change of target mid way, and the row goes quiet again
// once they land.
TEST_CASE("Quiet rows with eased binds ease to a new value", "[component_list]")
{
  auto play = [](bool quietRows) {
    EasedRows scene(quietRows);
    std::vector<std::vector<double>> frames;
    // Past the gate, with a new tint.
    scene.retarget(80, (int)0xFFE0A557);
    for (int i = 0; i < 8; i++)
    {
      scene.advance(1);
      frames.push_back(scene.drawn());
    }
    // Back below the gate before those values land.
    scene.retarget(30, (int)0xFF2060E0);
    for (int i = 0; i < 40; i++)
    {
      scene.advance(1);
      frames.push_back(scene.drawn());
    }
    return std::make_pair(frames, scene.allQuiet());
  };
  auto quiet = play(true);
  auto awake = play(false);
  CHECK(quiet.first == awake.first);
  CHECK(quiet.second);
  // The values have to ease, and the gate to open and close, or matching
  // proves nothing.
  REQUIRE(quiet.first.size() == 48);
  CHECK(quiet.first[3][0] > 10);
  CHECK(quiet.first[3][0] < 80);
  bool gateOpened = false;
  for (auto& frame : quiet.first)
  {
    gateOpened = gateOpened || frame[6] == 0xFF3BA55C;
  }
  CHECK(gateOpened);
  CHECK(
      quiet.first.back() == std::vector<double>{
                                30,
                                60,
                                0xFF2060E0,
                                60,
                                120,
                                0xFF2060E0,
                                0xFF808080,
                                0xFF3BA55C});
}

namespace
{
// quiet_style_rows.riv's Main, bound with a Theme of its own and played for
// 1.5 s: long enough for every row, and every style its texts draw with, to
// have nothing left to do. Each row's texts draw with styles that have work
// of their own: a state machine (Current, Pressable, Visited), binds reading
// past the style's own view model (Current and Themed read the Theme global
// view model, Visited its run's list item), a machine listening to its runs
// (Pressable), and inputs copied from the Text's handle (Visited, whose runs
// each get an instance of their own).
struct StyleRows
{
  static constexpr const char* kStyles[] = {
      "Current Style",
      "Themed Style",
      "Pressable Style",
  };

  rcp<File> file;
  std::unique_ptr<ArtboardInstance> artboard;
  std::unique_ptr<StateMachineInstance> machine;
  rcp<ViewModelInstance> viewModelInstance;
  rcp<ViewModelInstance> theme;
  ArtboardComponentList* list = nullptr;

  explicit StyleRows(bool quietRows)
  {
    ArtboardComponentList::sm_quietRowsEnabled = quietRows;
    file = ReadRiveFile("assets/quiet_style_rows.riv");
    artboard = file->artboardNamed("Main");
    REQUIRE(artboard != nullptr);
    viewModelInstance = file->createDefaultViewModelInstance(artboard.get());
    theme = file->createDefaultViewModelInstance(file->viewModel("Theme"));
    REQUIRE(theme != nullptr);
    machine = artboard->stateMachineAt(0);
    machine->setViewModelInstance(viewModelInstance);
    REQUIRE(machine->setGlobalViewModelInstance("Theme", theme));
    machine->bind();
    list = artboard->find<ArtboardComponentList>("Rows");
    REQUIRE(list != nullptr);
    advance(90);
    REQUIRE(list->artboardCount() == 2);
  }

  ~StyleRows() { ArtboardComponentList::sm_quietRowsEnabled = true; }

  void advance(int frames)
  {
    for (int i = 0; i < frames; i++)
    {
      machine->advanceAndApply(1.0f / 60.0f);
    }
  }

  ArtboardInstance* row(int index) { return list->artboardInstance(index); }

  ViewModelInstance* item(int index)
  {
    return list->listItem(index)->viewModelInstance().get();
  }

  ViewModelInstance* runItem(int index)
  {
    auto runs = item(index)->propertyValue("runs")->as<ViewModelInstanceList>();
    REQUIRE(runs->listItems().size() == 1);
    return runs->listItems()[0]->viewModelInstance().get();
  }

  void setBoolean(ViewModelInstance* instance, const char* name, bool value)
  {
    instance->propertyValue(name)
        ->as<ViewModelInstanceBoolean>()
        ->propertyValue(value);
  }

  TextStyleGlobal* style(int index, const char* name)
  {
    auto style = row(index)->find<TextStyleGlobal>(name);
    REQUIRE(style != nullptr);
    return style;
  }

  // The style the row's one list run draws with: its own instance of
  // Visited, made on demand.
  TextStyleOnDemand* runStyle(int index)
  {
    auto text = row(index)->find<Text>("Runs");
    REQUIRE(text != nullptr);
    REQUIRE(text->runs().size() == 1);
    auto style = text->runs()[0]->styleReference();
    REQUIRE(style != nullptr);
    REQUIRE_FALSE(style->is<TextStyleGlobal>());
    auto onDemand = static_cast<TextStyleOnDemand*>(style);
    REQUIRE(onDemand->instance() != nullptr);
    return onDemand;
  }

  void themeColor(const char* name, int color)
  {
    theme->propertyValue(name)->as<ViewModelInstanceColor>()->propertyValue(
        color);
  }

  static uint32_t inkOf(ArtboardInstance* styleArtboard)
  {
    auto ink = styleArtboard->find<SolidColor>("C");
    REQUIRE(ink != nullptr);
    return (uint32_t)ink->colorValue();
  }

  // The ink each row's texts draw with, row by row: its handles' styles in
  // kStyles order, then its list run's.
  std::vector<uint32_t> drawn()
  {
    std::vector<uint32_t> values;
    for (int i = 0; i < 2; i++)
    {
      for (auto name : kStyles)
      {
        values.push_back(inkOf(style(i, name)->artboardInstance()));
      }
      values.push_back(inkOf(runStyle(i)->instance()->artboardInstance()));
    }
    return values;
  }

  bool allQuiet()
  {
    for (int i = 0; i < 2; i++)
    {
      if (row(i)->quietHostRow() != (uint32_t)i)
      {
        return false;
      }
    }
    return true;
  }
};
} // namespace

// A row whose texts draw with their own instances of global text styles goes
// quiet once those styles would do nothing too: their state machines
// settled, their binds applied. Each style's artboard is marked with the
// row, so whatever changes it wakes the row.
TEST_CASE(
    "List rows whose texts' global styles have work of their own go quiet",
    "[component_list]")
{
  StyleRows scene(true);
  auto skips = ArtboardComponentList::sm_quietRowSkips;
  scene.advance(10);
  CHECK(ArtboardComponentList::sm_quietRowSkips > skips);
  CHECK(scene.allQuiet());
  for (int i = 0; i < 2; i++)
  {
    std::vector<ArtboardInstance*> styleArtboards;
    for (auto name : StyleRows::kStyles)
    {
      auto style = scene.style(i, name);
      REQUIRE_FALSE(style->isShared());
      styleArtboards.push_back(style->artboardInstance());
    }
    styleArtboards.push_back(scene.runStyle(i)->instance()->artboardInstance());
    for (auto styleArtboard : styleArtboards)
    {
      CHECK(styleArtboard->quietHostRow() != Artboard::kNoQuietRow);
      CHECK(styleArtboard->quietOwner() == scene.row(i));
    }
  }
  CHECK(
      scene.drawn() == std::vector<uint32_t>{
                           0xFF444444,
                           0xFF777777,
                           0xFF000000,
                           0xFF888888,
                           0xFF444444,
                           0xFF777777,
                           0xFF000000,
                           0xFF888888});
}

// A quiet row wakes for what changes its texts' styles: its item flipping a
// stateful style's input, a Theme color a style reads, a press on a run a
// style's machine listens to, a list run's item changing what its own
// style's machine reads. Each change shows on the frame it would without
// quiet rows, and the rows go quiet again after.
TEST_CASE(
    "Quiet rows wake for changes to their texts' global styles",
    "[component_list]")
{
  auto play = [](bool quietRows) {
    StyleRows scene(quietRows);
    std::vector<std::vector<uint32_t>> frames;
    std::vector<bool> quietBefore;
    auto step = [&](int count) {
      scene.advance(count);
      frames.push_back(scene.drawn());
    };
    // The first item becomes current: its Label's style moves to its active
    // state, which writes the Theme's active color into its ink. Its run's
    // instance copies the handle's `strong`, which shows nothing while the
    // run isn't visited.
    quietBefore.push_back(scene.allQuiet());
    scene.setBoolean(scene.item(0), "current", true);
    step(1);
    step(1);
    step(30);
    // Every Note reads the Theme's note color, and nothing else in a row
    // does.
    quietBefore.push_back(scene.allQuiet());
    scene.themeColor("note", (int)0xFF00AA00);
    step(1);
    step(30);
    // The second row's Press: its style's machine hears the press on its
    // run and keys its color.
    quietBefore.push_back(scene.allQuiet());
    scene.machine->pointerDown(Vec2D(210.0f, 112.0f));
    scene.machine->pointerUp(Vec2D(210.0f, 112.0f));
    step(1);
    step(1);
    step(30);
    // The second row's run is visited: only its style's machine reads that.
    quietBefore.push_back(scene.allQuiet());
    scene.setBoolean(scene.runItem(1), "visited", true);
    step(1);
    step(1);
    step(30);
    // The second item becomes current: its Label goes active, and its run's
    // instance copies `strong` from the handle and goes strong.
    quietBefore.push_back(scene.allQuiet());
    scene.setBoolean(scene.item(1), "current", true);
    step(1);
    step(1);
    step(30);
    return std::make_tuple(frames, quietBefore, scene.allQuiet());
  };
  auto quiet = play(true);
  auto awake = play(false);
  CHECK(std::get<0>(quiet) == std::get<0>(awake));
  // Each change reached rows that were asleep, and they went back to sleep.
  CHECK(std::get<1>(quiet) == std::vector<bool>{true, true, true, true, true});
  CHECK(std::get<2>(quiet));
  // The changes have to show, or matching proves nothing.
  auto& frames = std::get<0>(quiet);
  REQUIRE(frames.size() == 14);
  CHECK(
      frames[2] == std::vector<uint32_t>{
                       0xFF0B57D0,
                       0xFF777777,
                       0xFF000000,
                       0xFF888888,
                       0xFF444444,
                       0xFF777777,
                       0xFF000000,
                       0xFF888888});
  CHECK(
      frames[4] == std::vector<uint32_t>{
                       0xFF0B57D0,
                       0xFF00AA00,
                       0xFF000000,
                       0xFF888888,
                       0xFF444444,
                       0xFF00AA00,
                       0xFF000000,
                       0xFF888888});
  CHECK(
      frames[7] == std::vector<uint32_t>{
                       0xFF0B57D0,
                       0xFF00AA00,
                       0xFF000000,
                       0xFF888888,
                       0xFF444444,
                       0xFF00AA00,
                       0xFFFF0000,
                       0xFF888888});
  CHECK(
      frames[10] == std::vector<uint32_t>{
                        0xFF0B57D0,
                        0xFF00AA00,
                        0xFF000000,
                        0xFF888888,
                        0xFF444444,
                        0xFF00AA00,
                        0xFFFF0000,
                        0xFF00AAAA});
  CHECK(
      frames[13] == std::vector<uint32_t>{
                        0xFF0B57D0,
                        0xFF00AA00,
                        0xFF000000,
                        0xFF888888,
                        0xFF0B57D0,
                        0xFF00AA00,
                        0xFFFF0000,
                        0xFFAA00AA});
}

// quiet_script_rows.riv carries Luau bytecode; without the Luau backend its
// scripts cannot run.
#ifdef WITH_RIVE_SCRIPTING_LUAU
namespace
{
// quiet_script_rows.riv's Main (see its README), bound and played for 1.5 s:
// long enough for each row's scripts to have stopped advancing.
struct ScriptRows
{
  rcp<File> file;
  std::unique_ptr<ArtboardInstance> artboard;
  std::unique_ptr<StateMachineInstance> machine;
  rcp<ViewModelInstance> viewModelInstance;
  ArtboardComponentList* list = nullptr;

  explicit ScriptRows(bool quietRows)
  {
    ArtboardComponentList::sm_quietRowsEnabled = quietRows;
    file = ReadRiveFile("assets/quiet_script_rows.riv");
    artboard = file->artboardNamed("Main");
    REQUIRE(artboard != nullptr);
    viewModelInstance = file->createDefaultViewModelInstance(artboard.get());
    machine = artboard->stateMachineAt(0);
    machine->bindViewModelInstance(viewModelInstance);
    list = artboard->find<ArtboardComponentList>("Rows");
    REQUIRE(list != nullptr);
    advance(90);
    REQUIRE(list->artboardCount() == 2);
  }

  ~ScriptRows() { ArtboardComponentList::sm_quietRowsEnabled = true; }

  void advance(int frames)
  {
    for (int i = 0; i < frames; i++)
    {
      machine->advanceAndApply(1.0f / 60.0f);
    }
  }

  ArtboardInstance* row(int index) { return list->artboardInstance(index); }

  ViewModelInstance* item(int index)
  {
    return list->listItem(index)->viewModelInstance().get();
  }

  ScriptedDrawable* probe(int index)
  {
    auto probe = row(index)->find<ScriptedDrawable>("Probe");
    REQUIRE(probe != nullptr);
    return probe;
  }

  ScriptedPathEffect* shift(int index)
  {
    auto shift = row(index)->find<ScriptedPathEffect>("Shift");
    REQUIRE(shift != nullptr);
    return shift;
  }

  // The first point of the path Shift gave the row's Line stroke: x moves
  // with the item's offset and Shift's steps, y with each update.
  Vec2D shifted(int index)
  {
    auto stroke = row(index)->find<Stroke>("Line Stroke");
    REQUIRE(stroke != nullptr);
    auto path = shift(index)->effectPath(stroke);
    REQUIRE(path != nullptr);
    REQUIRE(!path->rawPath()->points().empty());
    return path->rawPath()->points()[0];
  }

  double number(int index, const char* name)
  {
    return item(index)
        ->propertyValue(name)
        ->as<ViewModelInstanceNumber>()
        ->propertyValue();
  }

  void number(int index, const char* name, float value)
  {
    item(index)
        ->propertyValue(name)
        ->as<ViewModelInstanceNumber>()
        ->propertyValue(value);
  }

  void boolean(int index, const char* name, bool value)
  {
    item(index)
        ->propertyValue(name)
        ->as<ViewModelInstanceBoolean>()
        ->propertyValue(value);
  }

  // What each row's script reported, row by row.
  std::vector<double> reported()
  {
    std::vector<double> values;
    for (int i = 0; i < 2; i++)
    {
      for (auto name : {"doubled", "presses", "advances", "seen"})
      {
        values.push_back(number(i, name));
      }
    }
    return values;
  }

  bool quiet(int index)
  {
    return row(index)->quietHostRow() == (uint32_t)index;
  }
  bool allQuiet() { return quiet(0) && quiet(1); }
};
} // namespace

// A row hosting a script goes quiet once the script's advance has returned
// false: until something wakes the script, the row has no work. A script
// that keeps advancing keeps its row awake.
TEST_CASE(
    "List rows that host a script go quiet once it stops advancing",
    "[component_list]")
{
  ScriptRows scene(true);
  auto skips = ArtboardComponentList::sm_quietRowSkips;
  scene.advance(10);
  CHECK(ArtboardComponentList::sm_quietRowSkips > skips);
  CHECK(scene.allQuiet());
  CHECK(scene.probe(0)->quietState() == AdvancingComponent::QuietState::quiet);
  CHECK(scene.shift(0)->quietState() == AdvancingComponent::QuietState::quiet);
  // Each script advanced its first frame and two more, then slept.
  CHECK(scene.number(0, "advances") == 3);
  CHECK(scene.number(1, "advances") == 3);

  // Busy, the first item's script keeps advancing once a poke wakes it, and
  // its row stays awake. Freed, it sleeps again.
  scene.boolean(0, "busy", true);
  scene.number(0, "poke", 1);
  scene.advance(30);
  CHECK_FALSE(scene.quiet(0));
  CHECK(scene.quiet(1));
  CHECK(scene.number(0, "advances") > 30);
  scene.boolean(0, "busy", false);
  scene.advance(10);
  CHECK(scene.allQuiet());
}

// A quiet row wakes for whatever wakes its script: a view model listener
// that asks for an update, a change to an input, a press, a change to a node
// its advance read, and async work the host delivers. Each shows on the
// frame it would without quiet rows, and the rows go quiet again after.
TEST_CASE("Quiet rows wake for what wakes their script", "[component_list]")
{
  auto play = [](bool quietRows) {
    ScriptRows scene(quietRows);
    std::vector<std::vector<double>> frames;
    std::vector<bool> quietBefore;
    auto step = [&](int count) {
      scene.advance(count);
      frames.push_back(scene.reported());
    };
    auto wake = [&](std::function<void()> change) {
      quietBefore.push_back(scene.allQuiet());
      change();
      step(1);
      step(1);
      step(30);
    };
    // The first item's poke runs its script's listener, which asks for an
    // update; nothing else in the row reads poke.
    wake([&]() { scene.number(0, "poke", 1); });
    // The second item's level reaches its script's input.
    wake([&]() { scene.number(1, "level", 4); });
    // A press on the second row reaches its script.
    wake([&]() {
      scene.machine->pointerDown(Vec2D(150.0f, 90.0f));
      scene.machine->pointerUp(Vec2D(150.0f, 90.0f));
    });
    // The first item's box moves its row's Box, which its script's last
    // advance read.
    wake([&]() { scene.number(0, "box", 40); });
    // The host delivers async work, as a finished image decode or fetch
    // does.
    wake([&]() { scene.probe(1)->asyncWorkDelivered(); });
    return std::make_tuple(frames, quietBefore, scene.allQuiet());
  };
  auto quiet = play(true);
  auto awake = play(false);
  CHECK(std::get<0>(quiet) == std::get<0>(awake));
  // Each change reached rows that were asleep, and they went back to sleep.
  CHECK(std::get<1>(quiet) == std::vector<bool>{true, true, true, true, true});
  CHECK(std::get<2>(quiet));
  // The changes have to show, or matching proves nothing. Per row: doubled,
  // presses, advances, seen.
  auto& frames = std::get<0>(quiet);
  REQUIRE(frames.size() == 15);
  // The poke woke the first script: an update, then three advances.
  CHECK(frames[2] == std::vector<double>{0, 0, 6, 10, 0, 0, 3, 10});
  // The input reached the second: doubled, and three advances.
  CHECK(frames[5] == std::vector<double>{0, 0, 6, 10, 8, 0, 6, 10});
  // The press: counted, and an advance.
  CHECK(frames[8] == std::vector<double>{0, 0, 6, 10, 8, 1, 7, 10});
  // The moved Box: read again on the next frame.
  CHECK(frames[11][3] == 40);
  CHECK(frames[11][2] > 6);
  // The async work: the second script advanced again.
  CHECK(frames[14][6] > frames[11][6]);
}

// A path effect sleeps the same way, but whatever wakes it (a change to an
// input, an update it asked for, async work the host delivers) asks for an
// update, which recomputes its path and re-arms its advance. A quiet row
// wakes for each, and its path changes on the frame it would without quiet
// rows.
TEST_CASE(
    "Quiet rows wake for what wakes their path effect",
    "[component_list]")
{
  auto play = [](bool quietRows) {
    ScriptRows scene(quietRows);
    std::vector<std::vector<float>> frames;
    std::vector<bool> quietBefore;
    auto step = [&](int count) {
      scene.advance(count);
      std::vector<float> points;
      for (int i = 0; i < 2; i++)
      {
        auto point = scene.shifted(i);
        points.push_back(point.x);
        points.push_back(point.y);
      }
      frames.push_back(points);
    };
    step(0);
    auto wake = [&](std::function<void()> change) {
      quietBefore.push_back(scene.allQuiet());
      change();
      step(1);
      step(1);
      step(1);
      step(30);
    };
    // The first item's offset reaches its effect's input, which steps twice
    // after it.
    wake([&]() { scene.number(0, "offset", 5); });
    // The host delivers async work to the second row's effect.
    wake([&]() { scene.shift(1)->asyncWorkDelivered(); });
    return std::make_tuple(frames, quietBefore, scene.allQuiet());
  };
  auto quiet = play(true);
  auto awake = play(false);
  CHECK(std::get<0>(quiet) == std::get<0>(awake));
  CHECK(std::get<1>(quiet) == std::vector<bool>{true, true});
  CHECK(std::get<2>(quiet));
  // The changes have to show, or matching proves nothing. Per row: the
  // first point's x and y, from before the changes.
  auto& frames = std::get<0>(quiet);
  REQUIRE(frames.size() == 9);
  auto moved = [&](int frame, int index) {
    return Vec2D(
        frames[frame][index * 2] - frames[0][index * 2],
        frames[frame][index * 2 + 1] - frames[0][index * 2 + 1]);
  };
  // The offset: 5 on the frame it changed (an update), then a step on each
  // of the next two (an update each), then nothing more.
  CHECK(moved(1, 0) == Vec2D(5, 1));
  CHECK(moved(2, 0) == Vec2D(6, 2));
  CHECK(moved(3, 0) == Vec2D(7, 3));
  CHECK(moved(4, 0) == Vec2D(7, 3));
  CHECK(moved(4, 1) == Vec2D(0, 0));
  // The async work: one more update on the frame it was delivered, and no
  // step, since the offset is unchanged.
  CHECK(moved(5, 1) == Vec2D(0, 1));
  CHECK(moved(8, 1) == Vec2D(0, 1));
  CHECK(moved(8, 0) == Vec2D(7, 3));
}
#endif
