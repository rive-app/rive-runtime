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
#include "rive_file_reader.hpp"
#include "utils/no_op_renderer.hpp"
#include "catch.hpp"

using namespace rive;

// text_style_global_bound_run.riv (built with rive-cli from
// assets/rml/text_style_global_bound_run.rml):
//   "Main": the Text "Runs" with a local style (white) and a "Plain" handle.
//   Its runs A and B bind styleId to styleA (Stateful), C to styleB (Plain);
//   D is unbound. All four are authored with the local style.
//   "Stateful": red, blue once its view model's `pressed` is set, which a
//   target-less down listener does on the style's own values.
//   "Plain" (green) and "Plain2" (yellow): no state machine. "Empty": no
//   text style paint, so not a text style (the runtime has no artboard kind:
//   any artboard with one counts).
namespace
{
constexpr int kLocal = (int)0xFFFFFFFF;
constexpr int kRed = (int)0xFFB40000;
constexpr int kBlue = (int)0xFF0000FF;
constexpr int kGreen = (int)0xFF00B400;
constexpr int kYellow = (int)0xFFFFC800;

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

struct BoundScene
{
  File* file;
  std::unique_ptr<ArtboardInstance> artboard;
  std::unique_ptr<StateMachineInstance> machine;
  rcp<ViewModelInstance> viewModelInstance;
  Text* text = nullptr;

  BoundScene(File* file) : file(file)
  {
    artboard = file->artboard("Main")->instance();
    machine = artboard->defaultStateMachine();
    REQUIRE(machine != nullptr);
    viewModelInstance = file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    machine->bindViewModelInstance(viewModelInstance);
    text = artboard->find<Text>("Runs");
    REQUIRE(text != nullptr);
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
    REQUIRE(text->runs().size() > index);
    return text->runs()[index];
  }

  TextStyleGlobal* handle()
  {
    for (auto child : text->children())
    {
      if (child->is<TextStyleGlobal>())
      {
        return child->as<TextStyleGlobal>();
      }
    }
    return nullptr;
  }

  void style(const char* property, int index)
  {
    viewModelInstance->propertyValue(property)
        ->as<ViewModelInstanceArtboard>()
        ->propertyValue(index);
    frames();
  }

  void style(const char* property, const std::string& artboardName)
  {
    int index = artboardIndex(file, artboardName);
    REQUIRE(index != -1);
    style(property, index);
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

TextStyleOnDemand* onDemand(TextValueRun* run)
{
  auto style = run->styleReference();
  REQUIRE(style != nullptr);
  REQUIRE(!style->is<TextStyleGlobal>());
  REQUIRE(!style->is<TextStylePaint>());
  return static_cast<TextStyleOnDemand*>(style);
}

bool pressedIn(TextValueRun* run)
{
  auto style = onDemand(run);
  REQUIRE(style->instance() != nullptr);
  auto value = style->instance()->viewModelInstance()->propertyValue("pressed");
  REQUIRE(value != nullptr);
  return value->as<ViewModelInstanceBoolean>()->propertyValue();
}
} // namespace

TEST_CASE(
    "Runs bound to a stateful style get an instance each",
    "[textStyleGlobal]")
{
  auto file = ReadRiveFile("assets/text_style_global_bound_run.riv");
  BoundScene scene(file.get());
  auto a = scene.run(0);
  auto b = scene.run(1);
  auto c = scene.run(2);
  auto d = scene.run(3);

  CHECK(onDemand(a) != onDemand(b));
  CHECK(onDemand(a)->owner() == a);
  CHECK(onDemand(b)->owner() == b);
  CHECK(onDemand(a)->instance() != onDemand(b)->instance());
  CHECK(colorOf(a) == kRed);
  CHECK(colorOf(b) == kRed);
  // A plain style the Text has a handle for draws the handle.
  CHECK(c->styleReference() == scene.handle());
  CHECK(colorOf(c) == kGreen);
  // An unbound run keeps its authored style.
  CHECK(d->styleReference()->is<TextStylePaint>());
  CHECK(colorOf(d) == kLocal);
  CHECK(scene.text->onDemandStyleCount() == 2);
}

TEST_CASE(
    "A bound run's own instance keeps what its listeners write",
    "[textStyleGlobal]")
{
  auto file = ReadRiveFile("assets/text_style_global_bound_run.riv");
  BoundScene scene(file.get());
  auto a = scene.run(0);
  auto b = scene.run(1);

  // "aaa " is offsets 0-3.
  auto point = glyphAt(scene.text, 1);
  scene.machine->pointerDown(point);
  scene.machine->pointerUp(point);
  scene.frames();
  CHECK(pressedIn(a));
  CHECK(!pressedIn(b));
  CHECK(colorOf(a) == kBlue);
  CHECK(colorOf(b) == kRed);
}

TEST_CASE("Runs bound to a plain style share it", "[textStyleGlobal]")
{
  auto file = ReadRiveFile("assets/text_style_global_bound_run.riv");
  BoundScene scene(file.get());
  auto a = scene.run(0);
  auto b = scene.run(1);

  // No handle: one instance on demand, for both; the per-run ones go.
  scene.style("styleA", "Plain2");
  CHECK(onDemand(a) == onDemand(b));
  CHECK(onDemand(a)->owner() == nullptr);
  CHECK(colorOf(a) == kYellow);
  CHECK(colorOf(b) == kYellow);
  CHECK(scene.text->onDemandStyleCount() == 1);

  // The Text's handle.
  scene.style("styleA", "Plain");
  CHECK(a->styleReference() == scene.handle());
  CHECK(b->styleReference() == scene.handle());
  CHECK(colorOf(a) == kGreen);
  CHECK(scene.text->onDemandStyleCount() == 0);

  // Back to the stateful style: an instance each again.
  scene.style("styleA", "Stateful");
  CHECK(onDemand(a) != onDemand(b));
  CHECK(colorOf(a) == kRed);
  CHECK(scene.text->onDemandStyleCount() == 2);
}

TEST_CASE(
    "A bound run naming no text style draws its authored one",
    "[textStyleGlobal]")
{
  auto file = ReadRiveFile("assets/text_style_global_bound_run.riv");
  BoundScene scene(file.get());
  auto a = scene.run(0);
  auto c = scene.run(2);

  scene.style("styleA", -1);
  CHECK(a->styleReference()->is<TextStylePaint>());
  CHECK(colorOf(a) == kLocal);

  // An artboard that isn't a text style.
  scene.style("styleB", "Empty");
  CHECK(c->styleReference()->is<TextStylePaint>());
  CHECK(colorOf(c) == kLocal);
  CHECK(scene.text->onDemandStyleCount() == 0);
}
