#include <rive/animation/nested_state_machine.hpp>
#include <rive/animation/state_machine.hpp>
#include <rive/animation/state_machine_instance.hpp>
#include <rive/animation/state_machine_input_instance.hpp>
#include <rive/animation/state_machine_listener.hpp>
#include <rive/constraints/scrolling/scroll_constraint.hpp>
#include <rive/listener_phase.hpp>
#include <rive/nested_artboard.hpp>
#include <rive/nested_artboard_mesh_host.hpp>
#include <rive/node.hpp>
#include <rive/scroll_event.hpp>
#include <rive/shapes/mesh_drawable.hpp>
#include "rive_file_reader.hpp"
#include <catch.hpp>

using namespace rive;

// Meshes only deform nested artboards, and so pointers, with RIVE_CANVAS.
#ifdef RIVE_CANVAS

// assets/rml/nested_mesh_pointers.rml nests Inner (100x100, split into Left
// and Right halves) twice in Host. Card skews it through a mesh, so Inner's
// (x, y) draws at (50 + x + y, 50 + y). Sliced N-slices it at (250, 50) with
// the center patch, Inner's [25..75] square, hidden.
namespace
{
struct NestedMeshScene
{
  rcp<File> file;
  std::unique_ptr<ArtboardInstance> artboard;
  std::unique_ptr<StateMachineInstance> stateMachine;

  // `captureListener` names a listener in Inner to run in the capture
  // phase, which state machine instances read as they're built.
  explicit NestedMeshScene(const char* captureListener = nullptr)
  {
    file = ReadRiveFile("assets/nested_mesh_pointers.riv");
    if (captureListener != nullptr)
    {
      capture(captureListener);
    }
    artboard = file->artboardNamed("Host");
    REQUIRE(artboard != nullptr);
    stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);
    // Hit components sort by draw order on the second advance.
    stateMachine->advanceAndApply(0.0f);
    stateMachine->advanceAndApply(0.0f);
  }

  void capture(const char* listenerName)
  {
    auto inner = file->artboard("Inner");
    REQUIRE(inner != nullptr);
    auto machine = inner->stateMachine(0);
    for (size_t i = 0; i < machine->listenerCount(); i++)
    {
      auto listener = machine->listener(i);
      if (listener->name() == listenerName)
      {
        const_cast<StateMachineListener*>(listener)->phaseValue(
            (uint32_t)ListenerPhase::capture);
        return;
      }
    }
    FAIL("no listener named " << listenerName);
  }

  NestedArtboard* nested(const char* name)
  {
    auto nested = artboard->find<NestedArtboard>(name);
    REQUIRE(nested != nullptr);
    return nested;
  }

  StateMachineInstance* inner(const char* name)
  {
    auto nestedStateMachine = nested(name)
                                  ->nestedAnimations()[0]
                                  ->as<NestedStateMachine>();
    REQUIRE(nestedStateMachine != nullptr);
    return nestedStateMachine->stateMachineInstance();
  }

  bool input(const char* nestedName, const char* name)
  {
    auto value = inner(nestedName)->getBool(name);
    REQUIRE(value != nullptr);
    return value->value();
  }

  void press(Vec2D position)
  {
    stateMachine->pointerDown(position);
    stateMachine->pointerUp(position);
    stateMachine->advanceAndApply(0.0f);
  }
};
} // namespace

TEST_CASE("pointers follow a nested artboard's mesh", "[hittest][mesh]")
{
  NestedMeshScene scene;

  // Inner (30, 50), Left. Undeformed this would be Inner (80, 50), Right.
  CHECK(scene.stateMachine->hitTest(Vec2D(130.0f, 100.0f)));
  scene.press(Vec2D(130.0f, 100.0f));
  CHECK(scene.input("Card", "Left"));
  CHECK_FALSE(scene.input("Card", "Right"));

  // Inner (60, 90), Right. Undeformed this would miss Inner altogether.
  scene.press(Vec2D(200.0f, 140.0f));
  CHECK(scene.input("Card", "Right"));
}

TEST_CASE("a pointer past a nested mesh's edge hits nothing", "[hittest][mesh]")
{
  NestedMeshScene scene;

  // Left of the skewed shape. Undeformed this would be Inner (60, 90).
  CHECK_FALSE(scene.stateMachine->hitTest(Vec2D(110.0f, 140.0f)));
  scene.press(Vec2D(110.0f, 140.0f));
  CHECK_FALSE(scene.input("Card", "Left"));
  CHECK_FALSE(scene.input("Card", "Right"));
}

TEST_CASE(
    "a drag keeps following the pointer past a nested mesh's edge",
    "[hittest][mesh]")
{
  NestedMeshScene scene;
  auto follower = scene.inner("Card")->artboard()->find<Node>("Follower");
  REQUIRE(follower != nullptr);

  scene.stateMachine->pointerDown(Vec2D(130.0f, 100.0f));
  scene.stateMachine->pointerMove(Vec2D(140.0f, 100.0f));
  CHECK(follower->x() == Approx(40.0f));
  CHECK(follower->y() == Approx(50.0f));
  CHECK_FALSE(scene.input("Card", "Exited"));

  // Off the mesh: hover unwinds, but the drag follows the skew extended
  // past its edge, to Inner (-30, 90).
  scene.stateMachine->pointerMove(Vec2D(110.0f, 140.0f));
  scene.stateMachine->advanceAndApply(0.0f);
  CHECK(scene.input("Card", "Exited"));
  CHECK(follower->x() == Approx(-30.0f));
  CHECK(follower->y() == Approx(90.0f));
  scene.stateMachine->pointerUp(Vec2D(110.0f, 140.0f));
}

TEST_CASE(
    "a capture listener off a nested mesh sees the pointer leave",
    "[hittest][mesh][capture]")
{
  NestedMeshScene scene("LeftDown");
  REQUIRE(scene.stateMachine->hasCaptureListeners());

  // In Sliced's hidden center, over Inner (40, 50), Left.
  scene.press(Vec2D(290.0f, 100.0f));
  CHECK_FALSE(scene.input("Sliced", "Left"));

  scene.press(Vec2D(260.0f, 60.0f));
  CHECK(scene.input("Sliced", "Left"));
}

// A press the host passes on as a miss can't land anywhere, so capture
// listeners only see the pointer leave, as off the mesh.
TEST_CASE(
    "a missed press doesn't reach capture listeners",
    "[hittest][mesh][capture]")
{
  NestedMeshScene scene("LeftDown");

  // Over Card's Inner (30, 50), Left.
  scene.stateMachine->pointerMiss(Vec2D(130.0f, 100.0f), ListenerType::down);
  scene.stateMachine->pointerMiss(Vec2D(130.0f, 100.0f), ListenerType::up);
  scene.stateMachine->advanceAndApply(0.0f);
  CHECK_FALSE(scene.input("Card", "Left"));

  scene.press(Vec2D(130.0f, 100.0f));
  CHECK(scene.input("Card", "Left"));
}

TEST_CASE("a hidden N-slice patch is a hole for pointers", "[hittest][mesh]")
{
  NestedMeshScene scene;

  CHECK_FALSE(scene.stateMachine->hitTest(Vec2D(300.0f, 100.0f)));
  scene.press(Vec2D(300.0f, 100.0f));
  CHECK_FALSE(scene.input("Sliced", "Left"));
  CHECK_FALSE(scene.input("Sliced", "Right"));

  scene.press(Vec2D(260.0f, 60.0f));
  CHECK(scene.input("Sliced", "Left"));
}

TEST_CASE("nested mesh mapping round-trips", "[hittest][mesh]")
{
  NestedMeshScene scene;
  auto card = scene.nested("Card");

  Vec2D local;
  bool offMesh = true;
  REQUIRE(card->worldToLocal(Vec2D(130.0f, 100.0f), &local, &offMesh));
  CHECK_FALSE(offMesh);
  CHECK(local.x == Approx(30.0f));
  CHECK(local.y == Approx(50.0f));

  // The way back, which hitTestHost takes to check the host's clipping.
  REQUIRE(card->meshHostIfAny() != nullptr);
  Vec2D world;
  REQUIRE(card->meshHostIfAny()->worldThroughMesh(local, &world));
  CHECK(world.x == Approx(130.0f));
  CHECK(world.y == Approx(100.0f));
}

TEST_CASE("pointers skip a nested mesh that didn't draw", "[hittest][mesh]")
{
  NestedMeshScene scene;
  // As when a clip forces the vector fallback.
  scene.nested("Card")->meshHostIfAny()->drewThroughMesh(false);

  // Undeformed, (110, 140) is Inner (60, 90).
  scene.press(Vec2D(110.0f, 140.0f));
  CHECK(scene.input("Card", "Right"));
}

// One level further down: Deep, nested in Card's Inner at (80, 80), keeps its
// drag when the pointer leaves Card's mesh. Card's miss reaches it as a miss,
// not as the pointer leaving, which would release its press.
TEST_CASE(
    "a drag in an artboard nested in a meshed one survives the mesh's edge",
    "[hittest][mesh]")
{
  NestedMeshScene scene;
  auto inner = scene.inner("Card");
  auto deep = inner->artboard()->find<NestedArtboard>("Deep");
  REQUIRE(deep != nullptr);
  auto deepMachine = deep->nestedAnimations()[0]
                         ->as<NestedStateMachine>()
                         ->stateMachineInstance();
  auto follower = deepMachine->artboard()->find<Node>("Follower");
  REQUIRE(follower != nullptr);

  // Inner (90, 90), Deep (10, 10).
  scene.stateMachine->pointerDown(Vec2D(230.0f, 140.0f));
  scene.stateMachine->pointerMove(Vec2D(231.0f, 140.0f));
  CHECK(follower->x() == Approx(11.0f));
  CHECK(follower->y() == Approx(10.0f));
  CHECK_FALSE(deepMachine->getBool("Exited")->value());

  // Past Card's right edge: Inner (105, 90), Deep (25, 10).
  scene.stateMachine->pointerMove(Vec2D(245.0f, 140.0f));
  CHECK(deepMachine->getBool("Exited")->value());
  CHECK(follower->x() == Approx(25.0f));
  CHECK(follower->y() == Approx(10.0f));
  scene.stateMachine->pointerUp(Vec2D(245.0f, 140.0f));
}

// Ending a drag runs its own listener passes (dragEnd, then a pointerMove to
// settle hover). Inside a miss they must miss too, or releasing over the
// hidden center patch would reach what's under the hole: Right's move
// listener there.
TEST_CASE(
    "ending a drag in an N-slice hole reaches nothing there",
    "[hittest][mesh]")
{
  NestedMeshScene scene;

  // Inner (10, 50), Left; then into the hole at Inner (60, 50), over Right.
  scene.stateMachine->pointerDown(Vec2D(260.0f, 100.0f));
  scene.stateMachine->pointerMove(Vec2D(310.0f, 100.0f));
  CHECK_FALSE(scene.input("Sliced", "RightMoved"));
  scene.stateMachine->pointerUp(Vec2D(310.0f, 100.0f));
  scene.stateMachine->advanceAndApply(0.0f);
  CHECK_FALSE(scene.input("Sliced", "RightMoved"));

  // Right does listen where it's drawn, in the ring at Inner (60, 10).
  scene.stateMachine->pointerMove(Vec2D(310.0f, 60.0f));
  CHECK(scene.input("Sliced", "RightMoved"));
}

// A drag of Host's own, from HostPad beside Holed, turns Holed's Inner off
// and its end turns it back on. Released over the hidden center patch, that
// end misses there too, so its settling move can't reach Right in the hole.
TEST_CASE(
    "a host's drag ending over an N-slice hole reaches nothing there",
    "[hittest][mesh]")
{
  NestedMeshScene scene;
  auto hostDragged = scene.stateMachine->getBool("HostDragged");
  REQUIRE(hostDragged != nullptr);

  // On HostPad, left of Holed; then into the hole at Inner (60, 50), over
  // Right.
  scene.stateMachine->pointerDown(Vec2D(430.0f, 300.0f));
  scene.stateMachine->pointerMove(Vec2D(440.0f, 300.0f));
  REQUIRE(hostDragged->value());
  scene.stateMachine->pointerMove(Vec2D(510.0f, 300.0f));
  scene.stateMachine->pointerUp(Vec2D(510.0f, 300.0f));
  scene.stateMachine->advanceAndApply(0.0f);
  CHECK_FALSE(scene.input("Holed", "RightMoved"));

  // Back on, Right listens where it's drawn, in the ring at Inner (60, 10).
  scene.stateMachine->pointerMove(Vec2D(510.0f, 260.0f));
  CHECK(scene.input("Holed", "RightMoved"));
}

// A mesh that draws but has nothing to map through draws nothing, so
// nothing behind it may be hit at its undeformed position either.
TEST_CASE(
    "an N-slice with every patch hidden takes no pointers",
    "[hittest][mesh]")
{
  NestedMeshScene scene;

  // Undeformed, these are Inner (50, 50), Right, and (10, 10), Left.
  CHECK_FALSE(scene.stateMachine->hitTest(Vec2D(450.0f, 100.0f)));
  scene.press(Vec2D(450.0f, 100.0f));
  scene.press(Vec2D(410.0f, 60.0f));
  CHECK_FALSE(scene.input("Hollow", "Left"));
  CHECK_FALSE(scene.input("Hollow", "Right"));

  Vec2D local;
  bool offMesh = false;
  REQUIRE(scene.nested("Hollow")
              ->worldToLocal(Vec2D(450.0f, 100.0f), &local, &offMesh));
  CHECK(offMesh);
}

// Tiled repeats its center column, so the UV under the pointer is drawn in
// several places. The host's clipping check (hitTestHost) has to use the
// place the pointer actually is, not wherever that UV was drawn last.
TEST_CASE(
    "a repeated N-slice tile maps back to the pointer's own tile",
    "[hittest][mesh]")
{
  NestedMeshScene scene;
  auto tiled = scene.nested("Tiled");
  const Vec2D pointer(137.5f, 300.0f);

  Vec2D local;
  bool offMesh = true;
  REQUIRE(tiled->worldToLocal(pointer, &local, &offMesh));
  CHECK_FALSE(offMesh);

  auto meshHost = tiled->meshHostIfAny();
  REQUIRE(meshHost != nullptr);
  Vec2D world;
  REQUIRE(meshHost->worldThroughMesh(local, &world));
  CHECK(world.x == Approx(pointer.x));
  CHECK(world.y == Approx(pointer.y));

  // Mapping the UV back on its own lands on another tile.
  Vec2D lastTile;
  REQUIRE(meshHost->hostedMesh()->positionAtUV(
      Vec2D(local.x / 100.0f, local.y / 100.0f),
      &lastTile));
  CHECK(lastTile.x != Approx(pointer.x));
}

// A scroll delta maps through the tile under the pointer. Where the delta
// would end is in the tile before, at a larger x in Strip, so mapping that
// point instead would run the strip backwards.
TEST_CASE(
    "a scroll over a repeated N-slice tile keeps its direction",
    "[hittest][mesh][scroll]")
{
  NestedMeshScene scene;
  auto tiledStrip = scene.nested("TiledStrip");
  auto scroll = scene.inner("TiledStrip")
                    ->artboard()
                    ->find<ScrollConstraint>("Scroll");
  REQUIRE(scroll != nullptr);
  const Vec2D pointer(137.5f, 450.0f);
  const Vec2D delta(-40.0f, 0.0f);

  Vec2D local;
  Vec2D end;
  REQUIRE(tiledStrip->worldToLocal(pointer, &local));
  REQUIRE(tiledStrip->worldToLocal(pointer + delta, &end));
  REQUIRE(end.x > local.x);

  ScrollEvent event;
  event.delta = delta;
  CHECK(scene.stateMachine->pointerScroll(pointer, event) != HitResult::none);
  CHECK(scroll->offsetX() == Approx(-40.0f));

  // The pointer's own tile is still the one the host's clipping checks.
  Vec2D world;
  REQUIRE(tiledStrip->meshHostIfAny()->worldThroughMesh(local, &world));
  CHECK(world.x == Approx(pointer.x));
}

#endif
