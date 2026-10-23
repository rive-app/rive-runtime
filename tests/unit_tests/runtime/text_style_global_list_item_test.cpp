#include "rive/animation/state_machine_instance.hpp"
#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include "rive/shapes/paint/shape_paint.hpp"
#include "rive/shapes/paint/solid_color.hpp"
#include "rive/text/cursor.hpp"
#include "rive/text/text.hpp"
#include "rive/text/text_style_global.hpp"
#include "rive/text/text_style_instance.hpp"
#include "rive/text/text_style_on_demand.hpp"
#include "rive/text/text_style_paint.hpp"
#include "rive/text/text_value_run.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/viewmodel_instance_artboard.hpp"
#include "rive/viewmodel/viewmodel_instance_boolean.hpp"
#include "rive/viewmodel/viewmodel_instance_enum.hpp"
#include "rive/viewmodel/viewmodel_instance_list.hpp"
#include "rive/viewmodel/viewmodel_instance_list_item.hpp"
#include "rive_file_reader.hpp"
#include "utils/no_op_renderer.hpp"
#include "catch.hpp"

using namespace rive;

// text_style_global_list_item.riv (built with rive-cli from an RML scene):
//   "Main": the Text "List" bound to listProperty1, with a "Text Style1"
//   handle. Its two items (TextRunViewModel1) carry urlState (enum URLState:
//   visited, non-visited) and a globalTextStyle: "visited " names Text Style1,
//   "non visited " names Text Style2.
//   "Text Style1" and "Text Style2": the same style. Their state machine's
//   Any State goes to a "visited" state (red) or a "nonVisited" one (teal)
//   from TextRunViewModel1/urlState, which only a list item has. Entry goes
//   to visited. Text Style1's view model has a `pressed` bool, which a
//   target-less down listener sets on the style's own values.
namespace
{
constexpr int kVisited = (int)0xFFB40000;
constexpr int kNonVisited = (int)0xFF009EB9;
constexpr uint32_t kVisitedIndex = 0;
constexpr uint32_t kNonVisitedIndex = 1;

int colorOf(TextValueRun* run)
{
  auto paint = run->style();
  REQUIRE(paint != nullptr);
  REQUIRE(!paint->shapePaints().empty());
  for (auto child : paint->shapePaints().front()->children())
  {
    if (child->is<SolidColor>())
    {
      return child->as<SolidColor>()->colorValue();
    }
  }
  FAIL("no solid color");
  return 0;
}

struct ListScene
{
  std::unique_ptr<ArtboardInstance> artboard;
  std::unique_ptr<StateMachineInstance> machine;
  rcp<ViewModelInstance> viewModelInstance;
  Text* list = nullptr;

  ListScene(File* file)
  {
    artboard = file->artboard("Main")->instance();
    machine = artboard->defaultStateMachine();
    REQUIRE(machine != nullptr);
    viewModelInstance = file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    machine->bindViewModelInstance(viewModelInstance);
    list = artboard->find<Text>("List");
    REQUIRE(list != nullptr);
    frames();
  }

  void frames(int count = 4)
  {
    NoOpRenderer renderer;
    for (int i = 0; i < count; i++)
    {
      machine->advanceAndApply(0.016f);
      artboard->draw(&renderer);
    }
  }

  TextValueRun* run(size_t index)
  {
    REQUIRE(list->runs().size() > index);
    return list->runs()[index];
  }

  ViewModelInstance* item(size_t index)
  {
    auto items = viewModelInstance->propertyValue("listProperty1")
                     ->as<ViewModelInstanceList>()
                     ->listItems();
    REQUIRE(items.size() > index);
    return items[index]->viewModelInstance().get();
  }

  void urlState(size_t index, uint32_t value)
  {
    item(index)
        ->propertyValue("urlState")
        ->as<ViewModelInstanceEnum>()
        ->propertyValue(value);
    frames();
  }
};

// The middle of the glyph at [offset], in the root artboard's space.
Vec2D glyphAt(Text* text, uint32_t offset)
{
  GlyphLookup lookup;
  lookup.compute(text->unichars(), text->shape());
  SimpleArray<SimpleArray<GlyphLine>> unusedLines;
  TextLayoutView view(
      text->shape(),
      unusedLines,
      text->orderedLines(),
      lookup,
      (uint32_t)text->unichars().size());
  auto x = [&](uint32_t at) {
    CursorPosition position(at);
    position.resolveLine(view);
    auto visual = position.clamped(view).visualPosition(view);
    REQUIRE(visual.found());
    return Vec2D(visual.x(), (visual.top() + visual.bottom()) / 2);
  };
  Vec2D local = (x(offset) + x(offset + 1)) * 0.5f;
  return text->artboard()->rootTransform(text->shapeWorldTransform() * local);
}

bool pressedIn(ViewModelInstance* instance)
{
  REQUIRE(instance != nullptr);
  auto value = instance->propertyValue("pressed");
  REQUIRE(value != nullptr);
  return value->as<ViewModelInstanceBoolean>()->propertyValue();
}

int artboardIndex(File* file, const std::string& name)
{
  for (size_t i = 0; i < file->artboardCount(); i++)
  {
    if (file->artboard(i)->name() == name)
    {
      return (int)i;
    }
  }
  return -1;
}
} // namespace

TEST_CASE(
    "A style drawn for one list run alone reads that run's item",
    "[textStyleGlobal]")
{
  auto file = ReadRiveFile("assets/text_style_global_list_item.riv");
  ListScene scene(file.get());
  auto visited = scene.run(0);
  auto nonVisited = scene.run(1);

  // Each run's instance reads its item.
  CHECK(colorOf(visited) == kVisited);
  CHECK(colorOf(nonVisited) == kNonVisited);

  // And keep following it.
  scene.urlState(0, kNonVisitedIndex);
  CHECK(colorOf(visited) == kNonVisited);
  scene.urlState(1, kVisitedIndex);
  CHECK(colorOf(nonVisited) == kVisited);
}

TEST_CASE(
    "List runs naming one stateful style get an instance each",
    "[textStyleGlobal]")
{
  auto file = ReadRiveFile("assets/text_style_global_list_item.riv");
  ListScene scene(file.get());
  // Both items now name Text Style1, which the Text has a handle for.
  scene.item(1)
      ->propertyValue("globalTextStyle")
      ->as<ViewModelInstanceArtboard>()
      ->propertyValue(artboardIndex(file.get(), "Text Style1"));
  scene.frames();
  auto visited = scene.run(0);
  auto nonVisited = scene.run(1);
  // Its state machine keeps state: each item gets its own instance (not
  // the handle), which reads that item.
  CHECK(visited->styleReference() != nonVisited->styleReference());
  CHECK(!visited->styleReference()->is<TextStyleGlobal>());
  CHECK(!nonVisited->styleReference()->is<TextStyleGlobal>());
  CHECK(colorOf(visited) == kVisited);
  CHECK(colorOf(nonVisited) == kNonVisited);

  scene.urlState(0, kNonVisitedIndex);
  CHECK(colorOf(visited) == kNonVisited);
  CHECK(colorOf(nonVisited) == kNonVisited);
  scene.urlState(1, kVisitedIndex);
  CHECK(colorOf(visited) == kNonVisited);
  CHECK(colorOf(nonVisited) == kVisited);
}

TEST_CASE(
    "A list item's own instance keeps what its listeners write",
    "[textStyleGlobal]")
{
  auto file = ReadRiveFile("assets/text_style_global_list_item.riv");
  ListScene scene(file.get());
  // Both items name Text Style1, which the Text has a handle for.
  scene.item(1)
      ->propertyValue("globalTextStyle")
      ->as<ViewModelInstanceArtboard>()
      ->propertyValue(artboardIndex(file.get(), "Text Style1"));
  scene.frames();
  auto visited = scene.run(0);
  auto other = scene.run(1);
  REQUIRE(!visited->styleReference()->is<TextStyleGlobal>());
  REQUIRE(!other->styleReference()->is<TextStyleGlobal>());
  auto valuesOf = [](TextValueRun* run) {
    auto style = static_cast<TextStyleOnDemand*>(run->styleReference());
    REQUIRE(style->instance() != nullptr);
    return style->instance()->viewModelInstance().get();
  };
  TextStyleGlobal* handle = nullptr;
  for (auto child : scene.list->children())
  {
    if (child->is<TextStyleGlobal>())
    {
      handle = child->as<TextStyleGlobal>();
    }
  }
  REQUIRE(handle != nullptr);
  // Each item's instance has values of its own, not the handle's.
  CHECK(valuesOf(visited) != handle->styleViewModelInstance().get());
  CHECK(valuesOf(visited) != valuesOf(other));

  // "visited " is offsets 0-7.
  auto point = glyphAt(scene.list, 1);
  scene.machine->pointerDown(point);
  scene.machine->pointerUp(point);
  scene.frames();
  CHECK(pressedIn(valuesOf(visited)));
  CHECK(!pressedIn(valuesOf(other)));
  CHECK(!pressedIn(handle->styleViewModelInstance().get()));
}
