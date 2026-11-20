#include "rive/animation/state_machine_instance.hpp"
#include "rive/constraints/scrolling/clamped_scroll_physics.hpp"
#include "rive/constraints/scrolling/elastic_scroll_physics.hpp"
#include "rive/constraints/scrolling/scroll_constraint.hpp"
#include "rive/layout_component.hpp"
#include "rive/scroll_event.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "deterministic_clock.hpp"
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include <catch.hpp>
#include <memory>

// A host keeps its frame loop running while advanceAndApply returns true.
// assets/layout/layout_scroll_vertical.riv scrolls y over [-610, 0] with
// elastic physics.

static rive::ScrollEvent precise(rive::ScrollPhase phase, float dx, float dy)
{
  rive::ScrollEvent event;
  event.delta = rive::Vec2D(dx, dy);
  event.phase = phase;
  event.precise = true;
  return event;
}

struct SettleScene
{
  DeterministicClock clock;
  rive::rcp<rive::File> file;
  std::unique_ptr<rive::ArtboardInstance> artboard;
  std::unique_ptr<rive::StateMachineInstance> smi;
  rive::ScrollConstraint* scroll = nullptr;
  rive::Vec2D over = rive::Vec2D(50.0f, 250.0f);

  explicit SettleScene(
      const char* path = "assets/layout/layout_scroll_vertical.riv",
      const char* artboardName = nullptr)
  {
    file = ReadRiveFile(path);
    artboard = artboardName == nullptr
                   ? file->artboard()->instance()
                   : file->artboard(artboardName)->instance();
    auto vmi = file->createDefaultViewModelInstance(artboard.get());
    if (vmi != nullptr)
    {
      artboard->bindViewModelInstance(vmi);
    }
    smi = artboard->stateMachineAt(0);
    scroll = artboard->find<rive::ScrollConstraint>()[0];
    smi->advanceAndApply(0.0f);
  }

  // A pixel wheel as the web runtime sends it: precise, no begin or end.
  void wheel(float dx, float dy, rive::Vec2D at)
  {
    smi->pointerScroll(
        at,
        precise(rive::ScrollPhase::update, dx, dy),
        clock.next());
  }
  void wheel(float dx, float dy) { wheel(dx, dy, over); }

  void drag(rive::Vec2D from, rive::Vec2D to)
  {
    smi->pointerMove(from, clock.next());
    smi->pointerDown(from);
    smi->advanceAndApply(1.0f / 60.0f);
    smi->pointerMove(to, clock.next());
    smi->advanceAndApply(0.0f);
    smi->pointerUp(to);
  }

  // Frames until the state machine asks for none, or -1 if it never stops.
  int framesToRest(int limit = 600)
  {
    for (int i = 0; i < limit; i++)
    {
      if (!smi->advanceAndApply(1.0f / 60.0f))
      {
        return i;
      }
    }
    return -1;
  }

  // True when the next frames ask for nothing and nothing moves.
  bool staysAtRest(int frames = 120)
  {
    float x = scroll->offsetX();
    float y = scroll->offsetY();
    for (int i = 0; i < frames; i++)
    {
      if (smi->advanceAndApply(1.0f / 60.0f) || scroll->offsetX() != x ||
          scroll->offsetY() != y)
      {
        return false;
      }
    }
    return true;
  }
};

TEST_CASE(
    "A wheel scroll into the middle lets the scene rest",
    "[scrollsettle]")
{
  SettleScene scene;
  REQUIRE(scene.scroll->physics()->is<rive::ElasticScrollPhysics>());
  REQUIRE(scene.staysAtRest());

  scene.wheel(0, -40.0f);
  scene.wheel(0, -40.0f);
  scene.wheel(0, -40.0f);
  REQUIRE(scene.scroll->offsetY() == -120.0f);
  REQUIRE(scene.scroll->isOverscrolled() == false);

  CHECK(scene.framesToRest() >= 0);
  CHECK(scene.staysAtRest());
  REQUIRE(scene.scroll->offsetY() == -120.0f);
}

TEST_CASE("A live wheel gesture still asks for frames", "[scrollsettle]")
{
  SettleScene scene;
  scene.wheel(0, -40.0f);
  // Inside the idle window the gesture is still open.
  REQUIRE(scene.smi->advanceAndApply(0.05f) == true);
  REQUIRE(scene.scroll->isScrolling() == true);
}

TEST_CASE("A wheel past the edge springs back, then rests", "[scrollsettle]")
{
  SettleScene scene;
  scene.wheel(0, 60.0f);
  REQUIRE(scene.scroll->offsetY() > 0.0f);

  int frames = scene.framesToRest();
  CHECK(frames > 1);
  CHECK(scene.scroll->offsetY() == 0.0f);
  CHECK(scene.staysAtRest());
}

TEST_CASE("A wheel on a snapping view snaps, then rests", "[scrollsettle]")
{
  SettleScene scene("assets/layout/layout_scroll_snap.riv", "main");
  REQUIRE(scene.scroll->snap() == true);
  scene.over = rive::Vec2D(
      scene.artboard->width() / 2.0f,
      scene.artboard->height() / 2.0f);
  scene.wheel(-30.0f, 0);
  REQUIRE(scene.scroll->offsetX() == -30.0f);

  int frames = scene.framesToRest();
  CHECK(frames > 1);
  // The settle carried it off the wheel's offset onto a snap point.
  CHECK(scene.scroll->offsetX() != -30.0f);
  CHECK(scene.staysAtRest());
}

TEST_CASE("Two wheel scrolls with a pause both end at rest", "[scrollsettle]")
{
  SettleScene scene;
  scene.wheel(0, -40.0f);
  CHECK(scene.framesToRest() >= 0);
  CHECK(scene.staysAtRest(30));

  scene.wheel(0, -40.0f);
  REQUIRE(scene.scroll->offsetY() == -80.0f);
  CHECK(scene.framesToRest() >= 0);
  CHECK(scene.staysAtRest());
  CHECK(scene.scroll->offsetY() == -80.0f);
}

TEST_CASE("A drag still flings, then rests", "[scrollsettle]")
{
  SettleScene scene;
  scene.drag(rive::Vec2D(50.0f, 250.0f), rive::Vec2D(50.0f, 50.0f));
  REQUIRE(scene.scroll->physics()->isRunning() == true);
  float released = scene.scroll->offsetY();

  int frames = scene.framesToRest();
  CHECK(frames > 1);
  CHECK(scene.scroll->offsetY() < released);
  CHECK(scene.staysAtRest());
}

TEST_CASE("A held drag keeps asking for frames", "[scrollsettle]")
{
  SettleScene scene;
  scene.smi->pointerMove(scene.over, scene.clock.next());
  scene.smi->pointerDown(scene.over);
  for (int i = 0; i < 30; i++)
  {
    REQUIRE(scene.smi->advanceAndApply(1.0f / 60.0f) == true);
  }
  scene.smi->pointerUp(scene.over);
  CHECK(scene.framesToRest() >= 0);
}

TEST_CASE("A wheel during a fling takes over, then rests", "[scrollsettle]")
{
  SettleScene scene;
  scene.drag(rive::Vec2D(50.0f, 250.0f), rive::Vec2D(50.0f, 150.0f));
  REQUIRE(scene.scroll->physics()->isRunning() == true);
  scene.smi->advanceAndApply(1.0f / 60.0f);
  scene.smi->advanceAndApply(1.0f / 60.0f);
  REQUIRE(scene.scroll->physics()->isRunning() == true);

  scene.wheel(0, -20.0f);
  REQUIRE(scene.scroll->physics()->isRunning() == false);
  float wheeled = scene.scroll->offsetY();
  CHECK(scene.framesToRest() >= 0);
  CHECK(scene.staysAtRest());
  CHECK(scene.scroll->offsetY() == wheeled);
}

TEST_CASE("Platform momentum without an end rests", "[scrollsettle]")
{
  SettleScene scene;
  scene.smi->pointerScroll(
      scene.over,
      precise(rive::ScrollPhase::momentum, 0, -30.0f),
      scene.clock.next());
  scene.smi->pointerScroll(
      scene.over,
      precise(rive::ScrollPhase::momentum, 0, -20.0f),
      scene.clock.next());
  REQUIRE(scene.scroll->offsetY() == -50.0f);
  CHECK(scene.framesToRest() >= 0);
  CHECK(scene.staysAtRest());
  CHECK(scene.scroll->offsetY() == -50.0f);
}

TEST_CASE("A clamped view rests after a wheel", "[scrollsettle]")
{
  SettleScene scene;
  delete scene.scroll->physics();
  scene.scroll->physics(new rive::ClampedScrollPhysics());
  scene.wheel(0, -40.0f);
  REQUIRE(scene.scroll->offsetY() == -40.0f);
  CHECK(scene.framesToRest() >= 0);
  CHECK(scene.staysAtRest());
  CHECK(scene.scroll->offsetY() == -40.0f);
}

TEST_CASE("A horizontal view rests after a wheel", "[scrollsettle]")
{
  SettleScene scene("assets/layout/layout_scroll_horizontal.riv");
  REQUIRE(scene.scroll->physics()->is<rive::ElasticScrollPhysics>());
  scene.over = rive::Vec2D(
      scene.artboard->width() / 2.0f,
      scene.artboard->height() / 2.0f);
  scene.wheel(-40.0f, 0);
  REQUIRE(scene.scroll->offsetX() == -40.0f);
  CHECK(scene.framesToRest() >= 0);
  CHECK(scene.staysAtRest());
  CHECK(scene.scroll->offsetX() == -40.0f);
}

TEST_CASE("Nested views rest after a wheel on each", "[scrollsettle]")
{
  SettleScene scene("assets/layout/scroll_nested.riv");
  auto outer = scene.artboard->find<rive::ScrollConstraint>("Scroll V");
  auto strip = scene.artboard->find<rive::ScrollConstraint>("Scroll H");
  REQUIRE(outer != nullptr);
  REQUIRE(strip != nullptr);
  auto stripViewport = scene.artboard->find<rive::LayoutComponent>(
      "Strip Viewport");
  REQUIRE(stripViewport != nullptr);
  rive::Vec2D overStrip = stripViewport->worldBounds().center();

  scene.wheel(-40.0f, 0, overStrip);
  REQUIRE(strip->offsetX() == -40.0f);
  CHECK(scene.framesToRest() >= 0);

  scene.wheel(0, -40.0f, overStrip);
  REQUIRE(outer->offsetY() == -40.0f);
  CHECK(scene.framesToRest() >= 0);
  CHECK(scene.staysAtRest());
  CHECK(strip->offsetX() == -40.0f);
  CHECK(outer->offsetY() == -40.0f);
}
