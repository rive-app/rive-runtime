#include "rive/animation/state_machine_input_instance.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include "rive/nested_artboard.hpp"
#include "rive/shapes/paint/shape_paint.hpp"
#include "rive/shapes/paint/solid_color.hpp"
#include "rive/text/cursor.hpp"
#include "rive/text/text.hpp"
#include "rive/text/text_style_global.hpp"
#include "rive/text/text_style_instance.hpp"
#include "rive/text/text_style_paint.hpp"
#include "rive/text/text_style_share_cache.hpp"
#include "rive/text/text_value_run.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/viewmodel_instance_artboard.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "rive_file_reader.hpp"
#include "utils/no_op_renderer.hpp"
#include "catch.hpp"

using namespace rive;

// text_style_global_listeners.riv (written by rive_core's
// global_text_style_listener_test.dart):
//   "Link" (origin 0.5/0.5): its default state machine has a `hover` bool set
//   by targetless enter/exit listeners and a `clicked` bool set by a
//   targetless down+click listener; Idle/Hover states key the payload's
//   color black/blue. Its font size is bound to its exposed `size`.
//   "Plain": no state machine.
//   "Main" (Main VM: linkSize 30, linkStyle = Link) with a state machine: the
//   Text "Links" with runs "one " and "two " on one Link handle (its size
//   bound to linkSize, its style to linkStyle), a plain run and a run "three"
//   on a Link handle of its own (size 30), and a nested "Card" at (300, 200)
//   (with its own state machine) whose Text "Card Link" has one Link run.
namespace
{
constexpr int kIdle = (int)0xFF000000;
constexpr int kHover = (int)0xFF0000FF;

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

// The state machine of [run]'s style, shared by every run drawn with it.
StateMachineInstance* machineOf(TextValueRun* run)
{
  REQUIRE(run->styleReference() != nullptr);
  auto machine = run->styleReference()->runStateMachine();
  REQUIRE(machine != nullptr);
  return machine;
}

bool hovered(TextValueRun* run)
{
  return machineOf(run)->getBool("hover")->value();
}

bool clicked(TextValueRun* run)
{
  return machineOf(run)->getBool("clicked")->value();
}

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

struct LinkScene
{
  std::unique_ptr<ArtboardInstance> artboard;
  std::unique_ptr<StateMachineInstance> machine;
  rcp<ViewModelInstance> viewModelInstance;
  Text* links = nullptr;

  LinkScene(File* file)
  {
    artboard = file->artboard("Main")->instance();
    machine = artboard->defaultStateMachine();
    REQUIRE(machine != nullptr);
    viewModelInstance = file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    machine->bindViewModelInstance(viewModelInstance);
    links = artboard->find<Text>("Links");
    REQUIRE(links != nullptr);
    frame();
    frame();
  }

  void frame(float seconds = 0.016f)
  {
    NoOpRenderer renderer;
    machine->advanceAndApply(seconds);
    artboard->draw(&renderer);
  }

  TextValueRun* run(size_t index)
  {
    REQUIRE(links->runs().size() > index);
    return links->runs()[index];
  }

  Text* cardText()
  {
    auto nested = artboard->find<NestedArtboard>("Card");
    REQUIRE(nested != nullptr);
    auto text = nested->artboardInstance()->find<Text>("Card Link");
    REQUIRE(text != nullptr);
    return text;
  }

  void moveTo(Vec2D position)
  {
    machine->pointerMove(position);
    frame();
  }
};

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
    "A style with targetless pointer listeners listens to its runs",
    "[textStyleGlobal][textStyleGlobalListeners]")
{
  auto file = ReadRiveFile("assets/text_style_global_listeners.riv");
  auto link = file->artboard("Link");
  auto plain = file->artboard("Plain");
  REQUIRE(link != nullptr);
  REQUIRE(plain != nullptr);
  CHECK(TextStyleShareCache::analyze(link, file.get()).listensToRuns);
  CHECK(!TextStyleShareCache::analyze(link, file.get()).shareable);
  CHECK(!TextStyleShareCache::analyze(plain, file.get()).listensToRuns);
  CHECK(file->textStyleShares().anyListensToRuns());
  CHECK(link->stateMachine(0)->listenerCount() == 3);

  // Files without such styles forward nothing.
  auto sharing = ReadRiveFile("assets/text_style_global_sharing.riv");
  CHECK(!sharing->textStyleShares().anyListensToRuns());
  auto sharingMain = sharing->artboard("Main")->instance();
  auto staticText = sharingMain->find<Text>("Text A");
  REQUIRE(staticText != nullptr);
  CHECK(!staticText->mayHaveListeningRuns());
}

TEST_CASE(
    "Runs on one handle share its instance",
    "[textStyleGlobal][textStyleGlobalListeners]")
{
  auto file = ReadRiveFile("assets/text_style_global_listeners.riv");
  LinkScene scene(file.get());
  auto one = scene.run(0);
  auto two = scene.run(1);
  auto plain = scene.run(2);
  auto three = scene.run(3);
  CHECK(one->styleReference() == two->styleReference());
  CHECK(three->styleReference() != one->styleReference());
  auto handle = one->styleReference()->as<TextStyleGlobal>();
  CHECK(one->style() == handle->resolvedPaint());
  CHECK(two->style() == handle->resolvedPaint());
  CHECK(three->style() != one->style());
  CHECK(machineOf(one) == machineOf(two));
  CHECK(machineOf(three) != machineOf(one));
  CHECK(plain->styleReference()->runStateMachine() == nullptr);
  CHECK(scene.machine->hasListeners());
  std::vector<StateMachineInstance*> machines;
  scene.links->runStateMachines(machines);
  CHECK(machines.size() == 2);
  // The exposed size follows the host into the shared instance.
  CHECK(one->style()->fontSize() == 30.0f);
  CHECK(three->style()->fontSize() == 30.0f);
  CHECK(colorOf(one) == kIdle);
  CHECK(colorOf(three) == kIdle);
}

TEST_CASE(
    "Hovering or pressing a run restyles every run on its handle",
    "[textStyleGlobal][textStyleGlobalListeners]")
{
  auto file = ReadRiveFile("assets/text_style_global_listeners.riv");
  LinkScene scene(file.get());
  auto one = scene.run(0);
  auto two = scene.run(1);
  auto three = scene.run(3);

  // "one " is offsets 0-3, "two " 4-7, "plain" 8-12, "three" 13-17.
  scene.moveTo(glyphAt(scene.links, 1));
  CHECK(hovered(one));
  CHECK(!hovered(three));
  scene.frame();
  CHECK(colorOf(one) == kHover);
  CHECK(colorOf(two) == kHover);
  CHECK(colorOf(three) == kIdle);

  // The runs on the handle are one target: moving between them stays in.
  scene.moveTo(glyphAt(scene.links, 5));
  CHECK(hovered(two));
  scene.frame();
  CHECK(colorOf(one) == kHover);

  scene.moveTo(glyphAt(scene.links, 15));
  CHECK(!hovered(one));
  CHECK(hovered(three));
  scene.frame();
  CHECK(colorOf(one) == kIdle);
  CHECK(colorOf(two) == kIdle);
  CHECK(colorOf(three) == kHover);

  // Over the plain run, then off the text.
  scene.moveTo(glyphAt(scene.links, 9));
  CHECK(!hovered(one));
  CHECK(!hovered(three));
  scene.moveTo(Vec2D(590.0f, 390.0f));
  scene.frame();
  CHECK(colorOf(one) == kIdle);
  CHECK(colorOf(three) == kIdle);

  auto point = glyphAt(scene.links, 5);
  scene.machine->pointerDown(point);
  scene.machine->pointerUp(point);
  scene.frame();
  CHECK(clicked(one));
  CHECK(!clicked(three));
}

TEST_CASE(
    "Runs in a nested artboard listen too",
    "[textStyleGlobal][textStyleGlobalListeners]")
{
  auto file = ReadRiveFile("assets/text_style_global_listeners.riv");
  LinkScene scene(file.get());
  auto card = scene.cardText();
  REQUIRE(card->runs().size() == 1);
  auto run = card->runs()[0];

  scene.moveTo(glyphAt(card, 1));
  CHECK(hovered(run));
  CHECK(!hovered(scene.run(0)));
  scene.frame();
  CHECK(colorOf(run) == kHover);

  scene.moveTo(Vec2D(590.0f, 390.0f));
  CHECK(!hovered(run));
}

TEST_CASE(
    "Exposed inputs reach the shared instance",
    "[textStyleGlobal][textStyleGlobalListeners]")
{
  auto file = ReadRiveFile("assets/text_style_global_listeners.riv");
  LinkScene scene(file.get());
  auto one = scene.run(0);
  auto two = scene.run(1);
  auto three = scene.run(3);
  scene.moveTo(glyphAt(scene.links, 1));
  scene.frame();
  REQUIRE(hovered(one));
  float twoAt = glyphAt(scene.links, 5).x;

  scene.viewModelInstance->propertyValue("linkSize")
      ->as<ViewModelInstanceNumber>()
      ->propertyValue(44.0f);
  scene.frame();
  CHECK(one->style()->fontSize() == 44.0f);
  CHECK(two->style()->fontSize() == 44.0f);
  // The Text re-shaped with the new size: "two" moved right.
  CHECK(glyphAt(scene.links, 5).x > twoAt);
  // The other handle has values of its own.
  CHECK(three->style()->fontSize() == 30.0f);
  // The state survives.
  CHECK(hovered(one));
  CHECK(colorOf(one) == kHover);
}

TEST_CASE(
    "Swapping a listening style rebuilds the handle's listening",
    "[textStyleGlobal][textStyleGlobalListeners]")
{
  auto file = ReadRiveFile("assets/text_style_global_listeners.riv");
  LinkScene scene(file.get());
  auto linkStyle = scene.viewModelInstance->propertyValue("linkStyle")
                       ->as<ViewModelInstanceArtboard>();
  scene.moveTo(glyphAt(scene.links, 1));
  REQUIRE(hovered(scene.run(0)));

  linkStyle->propertyValue(artboardIndex(file.get(), "Plain"));
  scene.frame();
  scene.frame();
  CHECK(scene.run(0)->styleReference()->runStateMachine() == nullptr);
  CHECK(
      scene.run(0)->style() == scene.run(0)->styleReference()->resolvedPaint());
  // Pointers over the runs reach nothing now.
  scene.moveTo(glyphAt(scene.links, 5));

  linkStyle->propertyValue(artboardIndex(file.get(), "Link"));
  scene.frame();
  scene.frame();
  scene.moveTo(Vec2D(590.0f, 390.0f));
  CHECK(!hovered(scene.run(0)));
  scene.moveTo(glyphAt(scene.links, 5));
  CHECK(hovered(scene.run(0)));
  CHECK(hovered(scene.run(1)));
}

TEST_CASE(
    "Listening style instances tear down in any order",
    "[textStyleGlobal][textStyleGlobalListeners]")
{
  auto file = ReadRiveFile("assets/text_style_global_listeners.riv");
  SECTION("state machine first, while hovered")
  {
    LinkScene scene(file.get());
    scene.moveTo(glyphAt(scene.links, 1));
    scene.machine = nullptr;
    scene.artboard = nullptr;
  }
  SECTION("artboard first, while hovered")
  {
    LinkScene scene(file.get());
    scene.moveTo(glyphAt(scene.links, 5));
    scene.artboard = nullptr;
    scene.machine = nullptr;
  }
  SECTION("artboard without a state machine")
  {
    auto artboard = file->artboard("Main")->instance();
    NoOpRenderer renderer;
    artboard->advance(0.0f);
    artboard->draw(&renderer);
    auto links = artboard->find<Text>("Links");
    REQUIRE(links != nullptr);
    std::vector<StateMachineInstance*> machines;
    links->runStateMachines(machines);
    CHECK(machines.size() == 2);
    artboard = nullptr;
  }
  CHECK(file->textStyleShares().liveCount() == 0);
}
