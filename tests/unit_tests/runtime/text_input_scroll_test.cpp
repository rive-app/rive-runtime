#include "rive/animation/state_machine_instance.hpp"
#include "rive/artboard.hpp"
#include "rive/constraints/scrolling/scroll_constraint.hpp"
#include "rive/file.hpp"
#include "rive/hit_result.hpp"
#include "rive/nested_artboard.hpp"
#include "rive/scroll_event.hpp"
#include "rive/text/text_input.hpp"
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include <catch.hpp>
#include <memory>
#include <string>

using namespace rive;

// Wheel and trackpad scrolls that start over a text input.
//
// assets/text_input_scroll.riv, source assets/rml/text_input_scroll.rml. Every
// artboard is a 200 x 300 view scrolling y over [-700, 0]. On Field a
// TextInput is in the scrolled content itself, which makes the whole viewport
// its field. On Nested the input is in a nested artboard that is the
// content's first row, 100 tall, as a cell of a list would be.
namespace
{
// On the text input while the view is at 0.
const Vec2D kOnInput(20.0f, 20.0f);
// On Nested, the plain row under the input's cell.
const Vec2D kOnRow(100.0f, 200.0f);

class View
{
public:
  explicit View(const std::string& name)
      : m_file(ReadRiveFile("assets/text_input_scroll.riv"))
      , m_artboard(m_file->artboardNamed(name))
      , m_machine(m_artboard->stateMachineAt(0))
  {
    auto scrolls = m_artboard->find<ScrollConstraint>();
    m_scroll = scrolls.empty() ? nullptr : scrolls[0];
    // The input has no bounds to hit until its text is shaped and laid out.
    m_machine->advanceAndApply(0.0f);
    m_machine->advanceAndApply(1.0f / 60.0f);
    m_machine->advanceAndApply(1.0f / 60.0f);
  }

  // The text input, wherever it is: in this artboard or in its nested one.
  TextInput* input()
  {
    auto inputs = m_artboard->find<TextInput>();
    if (!inputs.empty())
    {
      return inputs[0];
    }
    for (auto nested : m_artboard->find<NestedArtboard>())
    {
      auto nestedInputs = nested->artboardInstance()->find<TextInput>();
      if (!nestedInputs.empty())
      {
        return nestedInputs[0];
      }
    }
    return nullptr;
  }

  StateMachineInstance* machine() { return m_machine.get(); }
  ScrollConstraint* scroll() { return m_scroll; }

  HitResult wheel(Vec2D at, float dy)
  {
    ScrollEvent event;
    event.delta = Vec2D(0.0f, dy);
    event.phase = ScrollPhase::update;
    event.precise = false;
    return m_machine->pointerScroll(at, event);
  }

  HitResult trackpad(Vec2D at, ScrollPhase phase, float dy)
  {
    ScrollEvent event;
    event.delta = Vec2D(0.0f, dy);
    event.phase = phase;
    event.precise = true;
    return m_machine->pointerScroll(at, event);
  }

private:
  rcp<File> m_file;
  std::unique_ptr<ArtboardInstance> m_artboard;
  std::unique_ptr<StateMachineInstance> m_machine;
  ScrollConstraint* m_scroll = nullptr;
};
} // namespace

TEST_CASE(
    "a wheel over a text input scrolls the view behind it",
    "[text_input_scroll]")
{
  View view("Field");
  REQUIRE(view.scroll() != nullptr);
  REQUIRE(view.scroll()->offsetY() == 0.0f);

  CHECK(view.wheel(kOnInput, -50.0f) != HitResult::none);
  CHECK(view.scroll()->offsetY() == -50.0f);
}

TEST_CASE(
    "a trackpad gesture begun over a text input scrolls the view behind it",
    "[text_input_scroll]")
{
  View view("Field");
  REQUIRE(view.scroll() != nullptr);

  // A real gesture's first event already carries travel; the view opens the
  // gesture on it and moves from the next.
  CHECK(view.trackpad(kOnInput, ScrollPhase::begin, -2.0f) != HitResult::none);
  CHECK(
      view.trackpad(kOnInput, ScrollPhase::update, -30.0f) != HitResult::none);
  CHECK(
      view.trackpad(kOnInput, ScrollPhase::update, -18.0f) != HitResult::none);
  CHECK(view.scroll()->offsetY() == -48.0f);
}

TEST_CASE(
    "a wheel over a text input in a nested row scrolls the view it is in",
    "[text_input_scroll]")
{
  View view("Nested");
  REQUIRE(view.scroll() != nullptr);

  CHECK(view.wheel(kOnInput, -50.0f) != HitResult::none);
  CHECK(view.scroll()->offsetY() == -50.0f);
}

TEST_CASE("the scrolls above start on the text input", "[text_input_scroll]")
{
  // What makes the cases above mean something: the point really is on the
  // input, which is still opaque to the pointer. In both artboards the input
  // sits at its artboard's origin, so one point serves.
  for (const char* name : {"Field", "Nested"})
  {
    View view(name);
    REQUIRE(view.input() != nullptr);
    CHECK(view.input()->hitTestPoint(kOnInput, true, true));
    CHECK(view.machine()->pointerDown(kOnInput) == HitResult::hitOpaque);
    view.machine()->pointerUp(kOnInput);
  }
}

TEST_CASE(
    "a wheel beside the nested text input scrolls the view",
    "[text_input_scroll]")
{
  View view("Nested");
  REQUIRE(view.scroll() != nullptr);

  CHECK(view.wheel(kOnRow, -50.0f) != HitResult::none);
  CHECK(view.scroll()->offsetY() == -50.0f);
}
