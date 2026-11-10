#include "rive/animation/state_machine_instance.hpp"
#include "rive/artboard.hpp"
#include "rive/artboard_component_list.hpp"
#include "rive/constraints/scrolling/scroll_constraint.hpp"
#include "rive/constraints/scrolling/scroll_physics.hpp"
#include "rive/animation/nested_state_machine.hpp"
#include "rive/file.hpp"
#include "rive/nested_artboard.hpp"
#include "rive/node.hpp"
#include "deterministic_clock.hpp"
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include <catch.hpp>
#include <functional>
#include <string>

using namespace rive;

// assets/scroll_fingers.riv, source assets/rml/scroll_fingers.rml. Every
// artboard scrolls a list from 0 to -700 in a 300 tall view. On Bar the track
// spans x 180..200, with the thumb starting at its top.
namespace
{
const int kFirst = 11;
const int kSecond = 22;
const float kFrame = 1.0f / 60.0f;
const Vec2D kFirstAt(100.0f, 250.0f);
const Vec2D kSecondAt(100.0f, 100.0f);

class Scroller
{
public:
  explicit Scroller(const std::string& name)
      : m_file(ReadRiveFile("assets/scroll_fingers.riv"))
      , m_artboard(m_file->artboardNamed(name))
      , m_machine(m_artboard->stateMachineAt(0))
  {
    if (m_artboard->viewModelId() != -1)
    {
      m_machine->bindViewModelInstance(
          m_file->createDefaultViewModelInstance(m_artboard.get()));
    }
    auto scrolls = m_artboard->find<ScrollConstraint>();
    m_scroll = scrolls.empty() ? nullptr : scrolls[0];
    frames(1);
  }

  void down(Vec2D position, int pointerId)
  {
    m_machine->pointerDown(position, pointerId);
  }
  void move(Vec2D position, int pointerId)
  {
    m_machine->pointerMove(position, m_clock.next(), pointerId);
    frames(1);
  }
  void up(Vec2D position, int pointerId)
  {
    m_machine->pointerUp(position, pointerId);
  }
  void exit(Vec2D position, int pointerId)
  {
    m_machine->pointerExit(position, pointerId);
  }
  void collapse()
  {
    m_artboard->scaleX(0.0f);
    m_artboard->scaleY(0.0f);
  }
  void frames(int count)
  {
    for (int i = 0; i < count; i++)
    {
      m_machine->advanceAndApply(kFrame);
    }
  }

  // The state machine of the nested artboard called name.
  StateMachineInstance* nested(const char* name)
  {
    return m_artboard->find<NestedArtboard>(name)
        ->nestedAnimations()[0]
        ->as<NestedStateMachine>()
        ->stateMachineInstance();
  }
  void collapse(const char* name, bool collapsed)
  {
    m_artboard->find<Component>(name)->collapse(collapsed);
  }
  void scale(const char* name, float value)
  {
    auto node = m_artboard->find<TransformComponent>(name);
    node->scaleX(value);
    node->scaleY(value);
  }
  ArtboardInstance* artboard() { return m_artboard.get(); }

  float offset() { return m_scroll->scrollOffsetY(); }
  bool isSettling() { return m_scroll->physics()->isRunning(); }
  bool isActive() { return m_scroll->scrollActive(); }

private:
  rcp<File> m_file;
  std::unique_ptr<ArtboardInstance> m_artboard;
  std::unique_ptr<StateMachineInstance> m_machine;
  ScrollConstraint* m_scroll = nullptr;
  DeterministicClock m_clock;
};

// Both fingers travel the same distance from different heights. The list has to
// follow the second, the newest down, after every single move.
void dragBothUp(Scroller& view, Vec2D& first, Vec2D& second, int rounds)
{
  float start = view.offset();
  for (int i = 1; i <= rounds; i++)
  {
    first.y -= 5.0f;
    view.move(first, kFirst);
    REQUIRE(view.offset() == Approx(start - 5.0f * (i - 1)));
    second.y -= 5.0f;
    view.move(second, kSecond);
    REQUIRE(view.offset() == Approx(start - 5.0f * i));
  }
}
} // namespace

TEST_CASE(
    "a finger resting on the view does not block scrolling",
    "[scroll_fingers]")
{
  Scroller view(GENERATE("Elastic", "Clamped"));
  view.down(kFirstAt, kFirst);
  view.frames(10);
  Vec2D second = kSecondAt;
  view.down(second, kSecond);
  for (int i = 1; i <= 4; i++)
  {
    second.y -= 5.0f;
    view.move(second, kSecond);
    REQUIRE(view.offset() == Approx(-5.0f * i));
  }
  // With the resting finger still down, the view holds still.
  view.up(second, kSecond);
  view.frames(10);
  REQUIRE(view.offset() == Approx(-20.0f));
}

TEST_CASE(
    "a press that never lifts does not hold later drags",
    "[scroll_fingers]")
{
  Scroller view(GENERATE("Elastic", "Clamped"));
  view.down(kFirstAt, kFirst);
  for (int round = 0; round < 2; round++)
  {
    int id = kSecond + round;
    Vec2D finger = kSecondAt;
    view.down(finger, id);
    finger.y -= 20.0f;
    view.move(finger, id);
    view.up(finger, id);
    view.frames(120);
    REQUIRE_FALSE(view.isActive());
  }
}

TEST_CASE("a pointer id the host reuses still presses", "[scroll_fingers]")
{
  // Some hosts send -1 as a mouse's id, and others reuse ids once a pointer
  // has gone. Each press here pages the track toward a different spot.
  int id = GENERATE(-1, 0);
  Scroller view("Bar");
  view.down(Vec2D(190.0f, 280.0f), id);
  view.frames(60);
  float first = view.offset();
  REQUIRE(first < 0.0f);
  view.exit(Vec2D(190.0f, 280.0f), id);
  view.down(Vec2D(190.0f, 40.0f), id);
  view.frames(60);
  REQUIRE(view.offset() > first);
}

TEST_CASE(
    "a finger landing mid-drag takes over where it lands",
    "[scroll_fingers]")
{
  Scroller view(GENERATE("Elastic", "Clamped"));
  Vec2D first = kFirstAt;
  Vec2D second = kSecondAt;
  view.down(first, kFirst);
  for (int i = 0; i < 4; i++)
  {
    first.y -= 5.0f;
    view.move(first, kFirst);
  }
  REQUIRE(view.offset() == Approx(-20.0f));

  view.down(second, kSecond);
  REQUIRE_FALSE(view.isSettling());
  view.frames(10);
  REQUIRE(view.offset() == Approx(-20.0f));

  second.y -= 5.0f;
  view.move(second, kSecond);
  REQUIRE(view.offset() == Approx(-25.0f));
  first.y -= 5.0f;
  view.move(first, kFirst);
  REQUIRE(view.offset() == Approx(-25.0f));
}

TEST_CASE("lifting two fingers does not jump the scroll", "[scroll_fingers]")
{
  Scroller view(GENERATE("Elastic", "Clamped"));
  Vec2D first = kFirstAt;
  Vec2D second = kSecondAt;
  view.down(first, kFirst);
  view.down(second, kSecond);
  dragBothUp(view, first, second, 10);
  bool ownerFirst = GENERATE(true, false);
  if (ownerFirst)
  {
    view.up(second, kSecond);
    view.up(first, kFirst);
  }
  else
  {
    view.up(first, kFirst);
    view.up(second, kSecond);
  }
  // The fingers moved up, so nothing may carry the list back down past where
  // they left it, or out of its range.
  for (int i = 0; i < 120; i++)
  {
    view.frames(1);
    REQUIRE(view.offset() <= Approx(-50.0f));
    REQUIRE(view.offset() >= -700.0f);
  }
}

TEST_CASE("a finger lifting off a held scroll is ignored", "[scroll_fingers]")
{
  Scroller view(GENERATE("Elastic", "Clamped"));
  Vec2D first = kFirstAt;
  Vec2D second = kSecondAt;
  view.down(first, kFirst);
  view.down(second, kSecond);
  second.y -= 20.0f;
  view.move(second, kSecond);
  view.up(first, kFirst);
  REQUIRE_FALSE(view.isSettling());
  second.y -= 5.0f;
  view.move(second, kSecond);
  REQUIRE(view.offset() == Approx(-25.0f));
}

TEST_CASE("the drag passes to the finger left down", "[scroll_fingers]")
{
  Scroller view(GENERATE("Elastic", "Clamped"));
  Vec2D first = kFirstAt;
  Vec2D second = kSecondAt;
  view.down(first, kFirst);
  view.down(second, kSecond);
  for (int i = 0; i < 4; i++)
  {
    second.y -= 5.0f;
    view.move(second, kSecond);
  }
  REQUIRE(view.offset() == Approx(-20.0f));

  SECTION("when the driving finger lifts")
  {
    view.up(second, kSecond);
    view.exit(second, kSecond);
  }
  SECTION("when the driving finger is cancelled")
  {
    view.exit(second, kSecond);
  }

  view.frames(10);
  REQUIRE(view.offset() == Approx(-20.0f));

  // The remaining finger takes the drag from where it is on its first move, so
  // only its own travel after that counts.
  first.y -= 5.0f;
  view.move(first, kFirst);
  REQUIRE(view.offset() == Approx(-20.0f));
  first.y -= 5.0f;
  view.move(first, kFirst);
  REQUIRE(view.offset() == Approx(-25.0f));

  // The last finger releases as a single finger does.
  view.up(first, kFirst);
  REQUIRE(view.isSettling());
}

TEST_CASE(
    "a finger left down ends the drag when it lifts without moving",
    "[scroll_fingers]")
{
  Scroller view(GENERATE("Elastic", "Clamped"));
  Vec2D second = kSecondAt;
  view.down(kFirstAt, kFirst);
  view.down(second, kSecond);
  second.y -= 20.0f;
  view.move(second, kSecond);
  SECTION("after the driving finger lifts")
  {
    view.up(second, kSecond);
    view.exit(second, kSecond);
  }
  SECTION("after the driving finger is cancelled")
  {
    view.exit(second, kSecond);
  }
  view.up(kFirstAt, kFirst);
  view.frames(120);
  REQUIRE_FALSE(view.isActive());
  REQUIRE(view.offset() == Approx(-20.0f));
}

TEST_CASE("collapsing the artboard ends a two finger drag", "[scroll_fingers]")
{
  Scroller view(GENERATE("Elastic", "Clamped"));
  Vec2D first = kFirstAt;
  Vec2D second = kSecondAt;
  view.down(first, kFirst);
  view.down(second, kSecond);
  second.y -= 20.0f;
  view.move(second, kSecond);
  view.collapse();
  view.move(first, kFirst);
  view.frames(120);
  REQUIRE_FALSE(view.isActive());
}

TEST_CASE("a finger left down carries no fling", "[scroll_fingers]")
{
  // -1 is a pointer id some hosts send.
  int firstId = GENERATE(11, -1);
  Scroller view("Elastic");
  Vec2D first = kFirstAt;
  Vec2D second = kSecondAt;
  view.down(first, firstId);
  view.down(second, kSecond);
  for (int i = 0; i < 4; i++)
  {
    second.y -= 20.0f;
    view.move(second, kSecond);
  }
  view.up(second, kSecond);
  view.up(first, firstId);
  view.frames(120);
  REQUIRE(view.offset() == Approx(-80.0f));
}

TEST_CASE(
    "a second finger does not feed the scroll bar thumb",
    "[scroll_fingers]")
{
  // What one finger's travel scrolls, to compare against.
  float expected = 0.0f;
  {
    Scroller view("Bar");
    Vec2D finger(190.0f, 10.0f);
    view.down(finger, kFirst);
    for (int i = 0; i < 4; i++)
    {
      finger.y += 5.0f;
      view.move(finger, kFirst);
    }
    expected = view.offset();
    REQUIRE(expected < 0.0f);
  }

  Scroller view("Bar");
  Vec2D first(190.0f, 40.0f);
  Vec2D second(190.0f, 10.0f);
  view.down(first, kFirst);
  view.down(second, kSecond);
  for (int i = 0; i < 4; i++)
  {
    second.y += 5.0f;
    view.move(second, kSecond);
    view.move(first, kFirst);
  }
  REQUIRE(view.offset() == Approx(expected));

  // Once the second lifts, the first finger's next move takes the drag and
  // scrolls nothing.
  view.up(second, kSecond);
  view.frames(10);
  REQUIRE(view.offset() == Approx(expected));
  first.y += 5.0f;
  view.move(first, kFirst);
  REQUIRE(view.offset() == Approx(expected));
  first.y += 5.0f;
  view.move(first, kFirst);
  REQUIRE(view.offset() < expected);
  REQUIRE(view.offset() > expected * 1.5f);
}

TEST_CASE(
    "a finger taking the drag does not press the scroll bar track",
    "[scroll_fingers]")
{
  Scroller view("Bar");
  Vec2D first(190.0f, 280.0f);
  Vec2D second(190.0f, 200.0f);
  view.down(first, kFirst);
  view.frames(60);
  float pressed = view.offset();
  REQUIRE(pressed < 0.0f);
  view.down(second, kSecond);
  view.frames(60);
  REQUIRE(view.offset() == Approx(pressed));
  view.up(second, kSecond);
  view.frames(60);
  REQUIRE(view.offset() == Approx(pressed));
}

TEST_CASE(
    "a finger the drag has left moving with the scroll does not click",
    "[scroll_fingers]")
{
  Scroller view("Rows");
  Vec2D first(100.0f, 60.0f);
  Vec2D second = kFirstAt;
  view.down(first, kFirst);
  view.down(second, kSecond);
  dragBothUp(view, first, second, 4);
  view.up(first, kFirst);
  view.up(second, kSecond);
  view.frames(1);
  REQUIRE_FALSE(view.nested("Button")->getBool("Clicked")->value());

  // A tap on its own still clicks.
  view.frames(120);
  Vec2D tap(100.0f, 60.0f);
  view.down(tap, kSecond);
  view.up(tap, kSecond);
  view.frames(1);
  REQUIRE(view.nested("Button")->getBool("Clicked")->value());
}

namespace
{
enum class Lift
{
  up,
  exit,
  upThenExit,
};

// Presses, hides what holds the scroll view, lifts, then shows it again. Hosts
// lift with an up, an exit (a cancel), or both.
void loseAPress(
    Scroller& host,
    const std::function<void(bool)>& hide,
    Lift lift)
{
  host.down(kFirstAt, kFirst);
  hide(true);
  host.frames(1);
  if (lift != Lift::exit)
  {
    host.up(kFirstAt, kFirst);
  }
  if (lift != Lift::up)
  {
    host.exit(kFirstAt, kFirst);
  }
  hide(false);
  host.frames(1);
}

// A new finger drags and lifts. Its release flings as a lone finger's does,
// which a press still recorded as down would stop.
void requireReleaseFlings(Scroller& host, ScrollConstraint* scroll)
{
  Vec2D next = kSecondAt;
  host.down(next, kSecond);
  next.y -= 20.0f;
  host.move(next, kSecond);
  REQUIRE(scroll->scrollOffsetY() == Approx(-20.0f));
  host.up(next, kSecond);
  host.frames(120);
  REQUIRE(scroll->scrollOffsetY() < -21.0f);
  REQUIRE_FALSE(scroll->scrollActive());
}

// Where the hidden artboard's exit listener put its Mark, against where the
// lost press was in that artboard's own space.
void requireExitAt(ArtboardInstance* artboard, Vec2D local)
{
  REQUIRE(local.x != Approx(kFirstAt.x));
  auto mark = artboard->find<Node>("Mark");
  REQUIRE(mark->x() == Approx(local.x));
  REQUIRE(mark->y() == Approx(local.y));
}
} // namespace

TEST_CASE(
    "a press lost in a hidden nested artboard does not hold its scroll",
    "[scroll_fingers]")
{
  Lift lift = GENERATE(Lift::up, Lift::exit, Lift::upThenExit);
  Scroller host("Nest");
  auto nestedArtboard = host.artboard()->find<NestedArtboard>("List");
  auto scroll = nestedArtboard->artboardInstance()->find<ScrollConstraint>()[0];
  SECTION("collapsed")
  {
    Vec2D local;
    REQUIRE(nestedArtboard->worldToLocal(kFirstAt, &local));
    loseAPress(host, [&](bool hidden) { host.collapse("List", hidden); }, lift);
    requireExitAt(nestedArtboard->artboardInstance(), local);
    requireReleaseFlings(host, scroll);
  }
  SECTION("scaled to nothing")
  {
    loseAPress(
        host,
        [&](bool hidden) { host.scale("List", hidden ? 0.0f : 1.0f); },
        lift);
    requireReleaseFlings(host, scroll);
  }
}

TEST_CASE(
    "a press lost in a hidden list does not hold its rows' scroll",
    "[scroll_fingers]")
{
  Lift lift = GENERATE(Lift::up, Lift::exit, Lift::upThenExit);
  Scroller host("List Host");
  auto list = host.artboard()->find<ArtboardComponentList>("Rows");
  auto row = list->artboardInstance(0);
  REQUIRE(row != nullptr);
  auto scroll = row->find<ScrollConstraint>()[0];
  SECTION("collapsed")
  {
    Vec2D local;
    REQUIRE(list->worldToLocal(kFirstAt, &local, 0));
    loseAPress(host, [&](bool hidden) { host.collapse("Rows", hidden); }, lift);
    requireExitAt(row, local);
    requireReleaseFlings(host, scroll);
  }
  SECTION("scaled to nothing")
  {
    loseAPress(
        host,
        [&](bool hidden) { host.scale("List", hidden ? 0.0f : 1.0f); },
        lift);
    requireReleaseFlings(host, scroll);
  }
}

TEST_CASE(
    "a paused nested artboard sees no exit for a press it never got",
    "[scroll_fingers]")
{
  Scroller host("Nest");
  auto nestedArtboard = host.artboard()->find<NestedArtboard>("List");
  host.move(kFirstAt, kFirst);
  nestedArtboard->isPaused(true);
  host.down(kFirstAt, kFirst);
  host.up(kFirstAt, kFirst);
  auto mark = nestedArtboard->artboardInstance()->find<Node>("Mark");
  REQUIRE(mark->x() == 0.0f);
  REQUIRE(mark->y() == 0.0f);
}
