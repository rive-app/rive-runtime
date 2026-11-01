// A list hands a removed item's row to a new item rather than making a new
// row for it. The reused row has to start the way a new one does, whatever
// the old item's row was doing. The fixture's markup is
// assets/rml/list_row_reuse/scene.rml.

#include "rive/animation/animation_state.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/animation/nested_number.hpp"
#include "rive/animation/nested_state_machine.hpp"
#include "rive/animation/state_machine_input_instance.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/artboard_component_list.hpp"
#include "rive/constraints/scrolling/scroll_constraint.hpp"
#include "rive/layout/artboard_component_list_override.hpp"
#include "rive/nested_artboard.hpp"
#include "rive/property_recorder.hpp"
#include "rive/viewmodel/viewmodel_instance_boolean.hpp"
#include "rive/viewmodel/viewmodel_instance_list.hpp"
#include "rive/viewmodel/viewmodel_instance_list_item.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "rive/viewmodel/viewmodel_instance_viewmodel.hpp"
#include "utils/no_op_factory.hpp"
#include "utils/svg_factory.hpp"
#include "utils/svg_renderer.hpp"
#include "rive_file_reader.hpp"
#include <algorithm>
#include <catch.hpp>
#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace
{
struct ReuseScene
{
  rive::rcp<rive::File> file;
  std::unique_ptr<rive::ArtboardInstance> artboard;
  std::unique_ptr<rive::StateMachineInstance> machine;
  rive::rcp<rive::ViewModelInstance> viewModelInstance;
  rive::ArtboardComponentList* list = nullptr;
  const char* listName;

  explicit ReuseScene(const char* artboardName, const char* listName = "items")
      : listName(listName)
  {
    file = ReadRiveFile("assets/list_row_reuse.riv");
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

  rive::ViewModelInstanceList* items() const
  {
    return viewModelInstance->propertyValue(listName)
        ->as<rive::ViewModelInstanceList>();
  }

  // Replaces every item with a new instance of the same view model, and
  // returns the new instances.
  std::vector<rive::rcp<rive::ViewModelInstance>> replaceItems()
  {
    auto list = items();
    auto count = list->listItems().size();
    auto viewModel = list->listItems()[0]->viewModelInstance()->viewModel();
    list->removeAllItems();
    std::vector<rive::rcp<rive::ViewModelInstance>> replacements;
    for (size_t i = 0; i < count; i++)
    {
      auto replacement = file->createDefaultViewModelInstance(viewModel);
      replacements.push_back(replacement);
      auto item = rive::make_rcp<rive::ViewModelInstanceListItem>();
      item->viewModelInstance(replacement);
      list->addItem(item);
    }
    return replacements;
  }

  // Whether the scene stops asking for frames within a few.
  bool settles()
  {
    for (int frame = 0; frame < 10; frame++)
    {
      if (!machine->advanceAndApply(0.016f))
      {
        return true;
      }
    }
    return false;
  }
};

rive::ViewModelInstanceNumber* number(
    const rive::rcp<rive::ViewModelInstance>& instance,
    const char* name)
{
  return instance->propertyValue(name)->as<rive::ViewModelInstanceNumber>();
}

rive::ViewModelInstanceBoolean* boolean(
    const rive::rcp<rive::ViewModelInstance>& instance,
    const char* name)
{
  return instance->propertyValue(name)->as<rive::ViewModelInstanceBoolean>();
}
} // namespace

// A row taken out of its list halfway through a transition is reused with
// its layers back on their entry states. A layer left holding the old
// transition's mix and the values it mixes from applied those values every
// frame until its next transition, and asked for frames whenever it wasn't
// frozen.
TEST_CASE(
    "A row reused mid transition starts its layers over",
    "[list_row_reuse]")
{
  ReuseScene scene("Main");
  REQUIRE(scene.list->artboardCount() == 2);
  // New rows have nothing to do.
  CHECK(scene.settles());

  std::vector<rive::StateMachineInstance*> rows = {
      scene.list->stateMachineInstance(0),
      scene.list->stateMachineInstance(1)};
  for (auto* row : rows)
  {
    row->getBool("hovered")->value(true);
  }
  // A third of the way into the 300 ms lift.
  scene.machine->advanceAndApply(0.1f);
  for (auto* row : rows)
  {
    REQUIRE(row->isLayerMixing(0));
  }

  scene.replaceItems();
  scene.machine->advanceAndApply(0.016f);
  REQUIRE(scene.list->artboardCount() == 2);
  for (int i = 0; i < 2; i++)
  {
    auto* row = scene.list->stateMachineInstance(i);
    // Reused, not made again.
    CHECK((row == rows[0] || row == rows[1]));
    CHECK(row->getBool("hovered")->value() == false);
    CHECK_FALSE(row->isLayerMixing(0));
  }
  CHECK(scene.settles());
}

// A view model listener of a reused row listens to its new item only. A
// change to the old item, before or after the reuse, must not run the
// listener's actions against the new one.
TEST_CASE(
    "A reused row doesn't act on its old item's changes",
    "[list_row_reuse]")
{
  ReuseScene scene("Main");
  REQUIRE(scene.list->artboardCount() == 2);
  scene.machine->advanceAndApply(0.016f);
  auto old = scene.items()->listItems()[0]->viewModelInstance();
  REQUIRE(boolean(old, "flag")->propertyValue() == false);

  // Changed just before its row is taken away: the row has the change
  // pending when it is reused.
  number(old, "n")->propertyValue(5.0f);
  auto replacements = scene.replaceItems();
  scene.machine->advanceAndApply(0.016f);
  scene.machine->advanceAndApply(0.016f);
  for (auto& replacement : replacements)
  {
    CHECK(boolean(replacement, "flag")->propertyValue() == false);
  }

  // Changed after the reuse.
  number(old, "n")->propertyValue(6.0f);
  scene.machine->advanceAndApply(0.016f);
  scene.machine->advanceAndApply(0.016f);
  for (auto& replacement : replacements)
  {
    CHECK(boolean(replacement, "flag")->propertyValue() == false);
  }

  // The new item's own changes still reach the listener.
  number(replacements[0], "n")->propertyValue(7.0f);
  scene.machine->advanceAndApply(0.016f);
  scene.machine->advanceAndApply(0.016f);
  CHECK(boolean(replacements[0], "flag")->propertyValue() == true);
  CHECK(boolean(replacements[1], "flag")->propertyValue() == false);
}

namespace
{
rive::ArtboardComponentListOverride* listOverride(
    rive::ArtboardComponentList* list)
{
  for (auto* child : list->children())
  {
    if (child->is<rive::ArtboardComponentListOverride>())
    {
      return child->as<rive::ArtboardComponentListOverride>();
    }
  }
  return nullptr;
}
} // namespace

// An override names the artboard it sizes. A row a map rule picks is an
// instance of that artboard, not of the first one bound to the item's view
// model, and takes that artboard's override.
TEST_CASE("A list override sizes the rows a map rule picks", "[list_row_reuse]")
{
  ReuseScene scene("Mapped");
  scene.machine->advanceAndApply(0.016f);
  REQUIRE(scene.list->artboardCount() == 2);
  auto wide = listOverride(scene.list);
  REQUIRE(wide != nullptr);
  CHECK(wide->artboards().size() == 2);
  for (int i = 0; i < 2; i++)
  {
    auto row = scene.list->artboardInstance(i);
    CHECK(row->name() == "Wide Row");
    CHECK(row->layoutWidth() == 150.0f);
  }
}

// An override keeps the rows it sizes, and sizes them again whenever its
// own sizes change. Unbinding the list destroys its rows, so the override
// lets go of them too: a later change would size freed rows.
TEST_CASE(
    "A list override forgets the rows unbinding destroys",
    "[list_row_reuse]")
{
  ReuseScene scene("Mapped");
  scene.machine->advanceAndApply(0.016f);
  auto wide = listOverride(scene.list);
  REQUIRE(wide != nullptr);
  REQUIRE(wide->artboards().size() == 2);

  scene.list->unbind();
  CHECK(scene.list->artboardCount() == 0);
  CHECK(wide->artboards().empty());
  // Nothing left to size.
  wide->instanceWidth(120.0f);
}

// A virtualized list pools the rows that scroll out. A row whose item lost
// its view model, and with it the artboard it maps to, was destroyed instead,
// in the middle of the scroll pass: its layout node was still in the layout
// parent, which the next sync read, and its state machine went to the pool
// and read the freed artboard when the pool went. Address sanitizer catches
// both. The row is pooled like any other, under no artboard, and goes with
// the pool, state machine first.
TEST_CASE(
    "A virtualized row whose item lost its artboard stays until the pool goes",
    "[list_row_reuse]")
{
  auto file = ReadRiveFile(
      "assets/virtualized_artboard_databound_children.riv");
  auto artboard = file->artboardNamed("Main");
  REQUIRE(artboard != nullptr);
  auto stateMachine = artboard->stateMachineAt(0);
  REQUIRE(stateMachine != nullptr);
  auto vmi = file->createViewModelInstance(artboard->viewModelId(), 0);
  REQUIRE(vmi != nullptr);
  stateMachine->bindViewModelInstance(vmi);
  stateMachine->advanceAndApply(0.1f);

  rive::ArtboardComponentList* list = nullptr;
  for (auto* candidate : artboard->artboardComponentLists())
  {
    if (candidate->virtualizationEnabled() && candidate->artboardCount() > 0)
    {
      list = candidate;
    }
  }
  REQUIRE(list != nullptr);
  REQUIRE(list->artboardInstance(0) != nullptr);
  REQUIRE(list->stateMachineInstance(0) != nullptr);

  // Its item no longer has a view model, so no artboard to pool the row
  // under. Nothing updates the list for it: the item itself is the same.
  list->listItem(0)->viewModelInstance(nullptr);

  // Drag the first row out of view.
  float y = 200.0f;
  stateMachine->pointerDown(rive::Vec2D(60.0f, y));
  stateMachine->advanceAndApply(0.016f);
  while (y > -500.0f)
  {
    y -= 20.0f;
    stateMachine->pointerMove(rive::Vec2D(60.0f, y));
    stateMachine->advanceAndApply(0.016f);
  }
  stateMachine->pointerUp(rive::Vec2D(60.0f, y));
  stateMachine->advanceAndApply(0.016f);
  CHECK(list->artboardInstance(0) == nullptr);
  CHECK(list->stateMachineInstance(0) == nullptr);
  // The pool goes with the list, here.
}

// A list resets a reused row's properties to the values its state machine
// and binds start from, recorded on the row's source artboard. A nested
// input's getter reads the nested state machine's input, which the source
// doesn't run, so the recording held 0 for an input authored as 7.
TEST_CASE(
    "A row's recorded values give its nested inputs their authored values",
    "[list_row_reuse]")
{
  auto file = ReadRiveFile("assets/list_row_reuse.riv");
  auto source = file->artboard("Host Row");
  REQUIRE(source != nullptr);
  rive::PropertyRecorder recorder;
  recorder.recordArtboard(source);

  auto row = source->instance();
  auto avatar = row->nestedArtboard("Avatar");
  REQUIRE(avatar != nullptr);
  auto glow = avatar->input("glow");
  REQUIRE(glow != nullptr);
  REQUIRE(glow->is<rive::NestedNumber>());
  CHECK(glow->as<rive::NestedNumber>()->nestedValue() == 7.0f);

  glow->as<rive::NestedNumber>()->nestedValue(20.0f);
  recorder.apply(row.get());
  CHECK(glow->as<rive::NestedNumber>()->nestedValue() == 7.0f);
}

namespace
{
// Every list reachable from instance: its own, its view model properties'
// and its list items', a few levels down. Held, so a list inside an item
// stays alive when changing an outer list lets go of the item.
void collectLists(
    const rive::rcp<rive::ViewModelInstance>& instance,
    std::vector<rive::rcp<rive::ViewModelInstanceValue>>& lists,
    std::set<rive::ViewModelInstance*>& seen,
    int depth)
{
  if (instance == nullptr || depth > 3 || !seen.insert(instance.get()).second)
  {
    return;
  }
  for (auto& value : instance->propertyValues())
  {
    if (value->is<rive::ViewModelInstanceList>())
    {
      auto list = value->as<rive::ViewModelInstanceList>();
      lists.push_back(value);
      for (auto& item : list->listItems())
      {
        collectLists(item->viewModelInstance(), lists, seen, depth + 1);
      }
    }
    else if (value->is<rive::ViewModelInstanceViewModel>())
    {
      collectLists(
          value->as<rive::ViewModelInstanceViewModel>()
              ->referenceViewModelInstance(),
          lists,
          seen,
          depth + 1);
    }
  }
}

// Changes every list so rows leave and others come: step 0 replaces each
// item with a new instance of its view model, in reverse; step 1 moves the
// first item to the end as a new instance; step 2 reverses the items.
void churnLists(
    rive::File* file,
    const rive::rcp<rive::ViewModelInstance>& root,
    int step)
{
  std::vector<rive::rcp<rive::ViewModelInstanceValue>> lists;
  std::set<rive::ViewModelInstance*> seen;
  collectLists(root, lists, seen, 0);
  for (auto& value : lists)
  {
    auto list = value->as<rive::ViewModelInstanceList>();
    auto span = list->listItems();
    std::vector<rive::rcp<rive::ViewModelInstanceListItem>> items(
        span.begin(),
        span.end());
    if (items.empty())
    {
      continue;
    }
    auto fresh = [&](const rive::rcp<rive::ViewModelInstanceListItem>& item) {
      auto instance = item->viewModelInstance();
      if (instance == nullptr || instance->viewModel() == nullptr)
      {
        return item;
      }
      auto replacement = rive::make_rcp<rive::ViewModelInstanceListItem>();
      replacement->viewModelInstance(
          file->createDefaultViewModelInstance(instance->viewModel()));
      return replacement;
    };
    std::vector<rive::rcp<rive::ViewModelInstanceListItem>> next;
    switch (step % 3)
    {
      case 0:
        for (auto itr = items.rbegin(); itr != items.rend(); itr++)
        {
          next.push_back(fresh(*itr));
        }
        break;
      case 1:
        next.assign(items.begin() + 1, items.end());
        next.push_back(fresh(items.front()));
        break;
      default:
        next.assign(items.rbegin(), items.rend());
        break;
    }
    list->removeAllItems();
    for (auto& item : next)
    {
      list->addItem(item);
    }
  }
}

void listAssets(std::vector<std::string>& out)
{
  for (auto& entry : std::filesystem::recursive_directory_iterator("assets"))
  {
    if (entry.is_regular_file() && entry.path().extension() == ".riv")
    {
      out.push_back(entry.path().string());
    }
  }
  std::sort(out.begin(), out.end());
}

bool showsLists(rive::File* file)
{
  for (size_t i = 0; i < file->artboardCount(); i++)
  {
    if (!file->artboard(i)->artboardComponentLists().empty())
    {
      return true;
    }
  }
  return false;
}

// The default artboard's frames, drawn to SVG, as its lists change.
std::vector<std::string> churnedFrames(
    const std::vector<uint8_t>& bytes,
    bool reuseHostingRows)
{
  rive::ArtboardComponentList::sm_reuseHostingRows = reuseHostingRows;
  std::vector<std::string> frames;
  rive::SVGFactory factory;
  rive::ImportResult result;
  auto file = rive::File::import(bytes, &factory, &result);
  auto artboard = file->artboardDefault();
  if (artboard == nullptr || artboard->viewModelId() == -1)
  {
    return frames;
  }
  auto machine = artboard->stateMachineAt(0);
  auto root = file->createViewModelInstance(artboard->viewModelId(), 0);
  if (machine == nullptr || root == nullptr)
  {
    return frames;
  }
  machine->bindViewModelInstance(root);
  int step = 0;
  for (int frame = 0; frame < 40; frame++)
  {
    if (frame % 8 == 4)
    {
      churnLists(file.get(), root, step++);
    }
    machine->advanceAndApply(1.0f / 60.0f);
    rive::SVGRenderer renderer;
    artboard->draw(&renderer);
    frames.push_back(
        renderer.finalize((int)artboard->width(), (int)artboard->height()));
  }
  return frames;
}
} // namespace

// Handing a row that hosts artboards to a new item has to draw what making
// a new row would, frame for frame, however the lists change.
TEST_CASE(
    "Reusing rows that host artboards draws what new rows would",
    "[list_row_reuse]")
{
  bool deterministic = rive::File::deterministicMode;
  rive::File::deterministicMode = true;
  std::vector<std::string> assets;
  listAssets(assets);
  int compared = 0;
  uint64_t reused = 0;
  for (const auto& path : assets)
  {
    std::vector<uint8_t> bytes = ReadFile(path.c_str());
    rive::ImportResult result;
    auto probe = rive::File::import(bytes, &gNoOpFactory, &result);
    // Some assets are malformed on purpose.
    if (result != rive::ImportResult::success || probe == nullptr ||
        !showsLists(probe.get()))
    {
      continue;
    }
    auto fresh = churnedFrames(bytes, false);
    auto before = rive::ArtboardComponentList::sm_reusedHostingRows;
    auto reusing = churnedFrames(bytes, true);
    reused += rive::ArtboardComponentList::sm_reusedHostingRows - before;
    INFO(path);
    REQUIRE(fresh.size() == reusing.size());
    for (size_t frame = 0; frame < fresh.size(); frame++)
    {
      INFO("frame " << frame);
      CHECK(fresh[frame] == reusing[frame]);
    }
    compared++;
  }
  rive::ArtboardComponentList::sm_reuseHostingRows = true;
  rive::File::deterministicMode = deterministic;
  CHECK(compared > 0);
  CHECK(reused > 0);
  printf(
      "hosting row reuse: %d files compared, %llu rows reused\n",
      compared,
      (unsigned long long)reused);
}

// A row that hosts artboards is reused too. What it hosts starts over: its
// nested artboard is mounted anew, with its nested input back at the value
// authored on the row, and the row gets a new state machine, which hears
// the new nested artboard's events.
TEST_CASE("A reused row's nested artboards start over", "[list_row_reuse]")
{
  ReuseScene scene("Hosts", "hosts");
  scene.machine->advanceAndApply(0.016f);
  REQUIRE(scene.list->artboardCount() == 2);
  auto row = scene.list->artboardInstance(0);
  REQUIRE(row->nestedArtboard("Avatar") != nullptr);
  auto glow = [](rive::ArtboardInstance* row) {
    return row->nestedArtboard("Avatar")
        ->input("glow")
        ->as<rive::NestedNumber>()
        ->nestedValue();
  };
  // The state Avatar's machine is in.
  auto avatarState = [](rive::ArtboardInstance* row) {
    auto state = row->nestedArtboard("Avatar")
                     ->stateMachine("Avatar Machine")
                     ->stateMachineInstance()
                     ->layerState(0);
    return state != nullptr && state->is<rive::AnimationState>()
               ? state->as<rive::AnimationState>()->animation()->name()
               : std::string();
  };
  CHECK(glow(row) == 7.0f);
  CHECK(avatarState(row) == "Dim");
  CHECK(row->artboardComponentLists()[0]->artboardCount() == 1);

  // Hovering keys the glow to 20, which lights the avatar up; its lit event
  // sets the item's flag.
  auto first = scene.items()->listItems()[0]->viewModelInstance();
  scene.list->stateMachineInstance(0)->getBool("hovered")->value(true);
  for (int frame = 0; frame < 5; frame++)
  {
    scene.machine->advanceAndApply(0.016f);
  }
  CHECK(glow(row) == 20.0f);
  CHECK(avatarState(row) == "Lit");
  CHECK(boolean(first, "flag")->propertyValue() == true);

  auto reused = rive::ArtboardComponentList::sm_reusedHostingRows;
  auto replacements = scene.replaceItems();
  scene.machine->advanceAndApply(0.016f);
  REQUIRE(scene.list->artboardCount() == 2);
  // Both rows are reused rather than made again.
  CHECK(rive::ArtboardComponentList::sm_reusedHostingRows - reused == 2);
  for (int i = 0; i < 2; i++)
  {
    CHECK_FALSE(
        scene.list->stateMachineInstance(i)->getBool("hovered")->value());
    CHECK(glow(scene.list->artboardInstance(i)) == 7.0f);
    CHECK(avatarState(scene.list->artboardInstance(i)) == "Dim");
    // The nested list shows the new item's tags.
    CHECK(
        scene.list->artboardInstance(i)
            ->artboardComponentLists()[0]
            ->artboardCount() == 1);
  }
  for (auto& replacement : replacements)
  {
    CHECK(boolean(replacement, "flag")->propertyValue() == false);
  }

  // The new machine hears the new avatar.
  scene.list->stateMachineInstance(0)->getBool("hovered")->value(true);
  for (int frame = 0; frame < 5; frame++)
  {
    scene.machine->advanceAndApply(0.016f);
  }
  CHECK(glow(scene.list->artboardInstance(0)) == 20.0f);
  CHECK(boolean(replacements[0], "flag")->propertyValue() == true);
  CHECK(boolean(replacements[1], "flag")->propertyValue() == false);
}

// A nested artboard is instanced on its host's factory, which needn't be the
// file's. Remounting it keeps that factory.
TEST_CASE(
    "A remounted nested artboard keeps its instance's factory",
    "[list_row_reuse]")
{
  auto file = ReadRiveFile("assets/list_row_reuse.riv");
  rive::NoOpFactory other;
  auto row = file->artboard("Host Row")
                 ->instance<rive::ArtboardInstance>(&other);
  auto avatar = row->nestedArtboard("Avatar");
  REQUIRE(avatar != nullptr);
  REQUIRE(avatar->artboardInstance()->factory() == &other);
  avatar->remount();
  REQUIRE(avatar->artboardInstance() != nullptr);
  CHECK(avatar->artboardInstance()->factory() == &other);
}

// A reused row's lists make all their rows again for the new item, as a new
// row's would, even for the items both items show. Here the new item wraps
// the same view model instance, so its tags are the very same list items.
TEST_CASE("A reused row's lists start their rows over", "[list_row_reuse]")
{
  ReuseScene scene("Hosts", "hosts");
  scene.machine->advanceAndApply(0.016f);
  auto tags = [&]() {
    return scene.list->artboardInstance(0)->artboardComponentLists()[0];
  };
  REQUIRE(tags()->artboardCount() == 1);
  tags()->stateMachineInstance(0)->getBool("hovered")->value(true);
  // A third of the way into the tag's 300 ms lift.
  scene.machine->advanceAndApply(0.1f);
  REQUIRE(tags()->stateMachineInstance(0)->isLayerMixing(0));

  auto hosts = scene.items();
  auto shown = hosts->listItems()[0]->viewModelInstance();
  auto wrapper = rive::make_rcp<rive::ViewModelInstanceListItem>();
  wrapper->viewModelInstance(shown);
  hosts->removeItem(0);
  hosts->addItemAt(wrapper, 0);
  auto reused = rive::ArtboardComponentList::sm_reusedHostingRows;
  scene.machine->advanceAndApply(0.016f);
  REQUIRE(rive::ArtboardComponentList::sm_reusedHostingRows - reused == 1);

  REQUIRE(tags()->artboardCount() == 1);
  auto tag = tags()->stateMachineInstance(0);
  CHECK_FALSE(tag->getBool("hovered")->value());
  CHECK_FALSE(tag->isLayerMixing(0));
}

// A row's scroll constraint keeps its offset and momentum, which the reuse
// doesn't reset: the new item's row opened where the old one was scrolled to.
// A row that scrolls is made anew instead.
TEST_CASE(
    "A row that scrolls opens at the top for a new item",
    "[list_row_reuse]")
{
  ReuseScene scene("Scrolls");
  scene.machine->advanceAndApply(0.016f);
  REQUIRE(scene.list->artboardCount() == 2);
  auto scroll = [&](int index) {
    return scene.list->artboardInstance(index)->find<rive::ScrollConstraint>(
        "Scroll");
  };
  REQUIRE(scroll(0) != nullptr);
  scroll(0)->scrollOffsetY(-60.0f);
  scene.machine->advanceAndApply(0.016f);
  REQUIRE(scroll(0)->offsetY() == -60.0f);

  scene.replaceItems();
  scene.machine->advanceAndApply(0.016f);
  REQUIRE(scene.list->artboardCount() == 2);
  for (int i = 0; i < 2; i++)
  {
    CHECK(scroll(i)->offsetY() == 0.0f);
  }
}
