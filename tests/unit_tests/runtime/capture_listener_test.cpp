#ifdef WITH_RIVE_TEXT
#include "rive/animation/nested_state_machine.hpp"
#include "rive/animation/state_machine.hpp"
#include "rive/animation/state_machine_input_instance.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/animation/state_machine_listener.hpp"
#include "rive/artboard_component_list.hpp"
#include "rive/focus_data.hpp"
#include "rive/input/focus_manager.hpp"
#include "rive/listener_phase.hpp"
#include "rive/nested_artboard.hpp"
#include "rive/text/cursor.hpp"
#include "rive/text/text_input.hpp"
#include "rive/viewmodel/viewmodel_instance_list.hpp"
#include "rive/viewmodel/viewmodel_instance_list_item.hpp"
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"

using namespace rive;

// assets/capture_listeners.riv, source assets/rml/capture_listeners.rml. Every
// listener in it is in the normal phase; a test flips the ones it needs before
// instancing, since a state machine instance reads the phase once, when it
// registers its listeners.
//
// In Host's space: Slot A spans x 20..240, y 20..68, holding Field A; Slot B
// spans x 20..240, y 88..136, holding Field B; the opaque Overlay covers
// x 130..240 of Slot B. A field's text input takes presses over x 30..55; the
// input is opaque, so a field's Viewport listeners only hear the pointer in
// the rest of the field.
//
// In List Host's space: List Slot spans x 20..240, y 20..68, holding a list of
// one Row; the opaque List Overlay covers x 130..240 of it. A second row, once
// added, sits under the first, at y 68..116.
//
// In Claims' space: Outer spans x 20..240, y 20..80, Inner x 20..220, y 20..60,
// and Leaf x 20..120, y 20..40.
namespace
{
const Vec2D kOnFieldAText(40.0f, 44.0f);
const Vec2D kOnFieldAViewport(100.0f, 44.0f);
const Vec2D kOnFieldBText(40.0f, 112.0f);
const Vec2D kOnFieldBViewport(100.0f, 112.0f);
const Vec2D kUnderOverlay(200.0f, 112.0f);
const Vec2D kOutsideSlots(270.0f, 112.0f);
const Vec2D kOnListRow(100.0f, 44.0f);
const Vec2D kUnderListOverlay(200.0f, 44.0f);
const Vec2D kOnSecondListRow(100.0f, 80.0f);
const Vec2D kOnLeaf(40.0f, 30.0f);

void capture(File* file, const char* artboardName, const char* listenerName)
{
  auto artboard = file->artboard(artboardName);
  REQUIRE(artboard != nullptr);
  for (size_t m = 0; m < artboard->stateMachineCount(); m++)
  {
    auto machine = artboard->stateMachine(m);
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
  }
  FAIL("no listener named " << listenerName);
}

struct Field
{
  StateMachineInstance* machine = nullptr;
  TextInput* input = nullptr;
  FocusData* focusData = nullptr;

  bool flag(const char* name) { return machine->getBool(name)->value(); }
  void flag(const char* name, bool value)
  {
    machine->getBool(name)->value(value);
  }
  uint32_t caret()
  {
    return input->rawTextInput()->cursor().end().codePointIndex();
  }
  void caret(uint32_t index)
  {
    input->rawTextInput()->cursor(Cursor::collapsed(CursorPosition(index)));
  }
};

struct HostScene
{
  std::unique_ptr<ArtboardInstance> artboard;
  std::unique_ptr<StateMachineInstance> machine;
  Field a;
  Field b;

  explicit HostScene(File* file)
  {
    artboard = file->artboardNamed("Host");
    REQUIRE(artboard != nullptr);
    machine = artboard->stateMachineAt(0);
    REQUIRE(machine != nullptr);
    a = field("Field A");
    b = field("Field B");
    // The first update builds the draw order; the next advance sorts the
    // hit components by it.
    machine->advanceAndApply(0.0f);
    machine->advanceAndApply(0.0f);
  }

  Field field(const char* name)
  {
    Field result;
    auto nested = artboard->find<NestedArtboard>(name);
    REQUIRE(nested != nullptr);
    REQUIRE(nested->nestedAnimations().size() == 1);
    result.machine = nested->nestedAnimations()[0]
                         ->as<NestedStateMachine>()
                         ->stateMachineInstance();
    REQUIRE(result.machine != nullptr);
    result.input = nested->artboardInstance()->objects<TextInput>().first();
    REQUIRE(result.input != nullptr);
    for (auto child : result.input->children())
    {
      if (child->is<FocusData>())
      {
        result.focusData = child->as<FocusData>();
      }
    }
    REQUIRE(result.focusData != nullptr);
    return result;
  }

  bool flag(const char* name) { return machine->getBool(name)->value(); }
  void flag(const char* name, bool value)
  {
    machine->getBool(name)->value(value);
  }

  void focus(Field& field)
  {
    machine->focusManager()->setFocus(field.focusData->focusNode());
    REQUIRE(field.input->isFocused());
  }

  bool press(Key key, KeyModifiers modifiers = KeyModifiers::none)
  {
    bool claimed = machine->keyInput(key, modifiers, true, false);
    machine->advanceAndApply(0.0f);
    return claimed;
  }

  bool release(Key key, KeyModifiers modifiers = KeyModifiers::none)
  {
    bool claimed = machine->keyInput(key, modifiers, false, false);
    machine->advanceAndApply(0.0f);
    return claimed;
  }

  void click(Vec2D position, int pointerId = 0)
  {
    machine->pointerDown(position, pointerId);
    machine->pointerUp(position, pointerId);
    machine->advanceAndApply(0.0f);
  }
};
struct ListScene
{
  std::unique_ptr<ArtboardInstance> artboard;
  std::unique_ptr<StateMachineInstance> machine;
  rcp<ViewModelInstance> viewModel;
  ArtboardComponentList* list = nullptr;

  explicit ListScene(File* file)
  {
    artboard = file->artboardNamed("List Host");
    REQUIRE(artboard != nullptr);
    machine = artboard->stateMachineAt(0);
    REQUIRE(machine != nullptr);
    viewModel = file->createDefaultViewModelInstance(artboard.get());
    machine->bindViewModelInstance(viewModel);
    settle();
    list = artboard->find<ArtboardComponentList>("Rows");
    REQUIRE(list != nullptr);
    REQUIRE(list->itemCount() == 1);
  }

  void settle()
  {
    machine->advanceAndApply(0.0f);
    machine->advanceAndApply(0.0f);
  }

  void addRow(File* file)
  {
    auto item = make_rcp<ViewModelInstanceListItem>();
    item->viewModelInstance(file->createViewModelInstance("Row"));
    viewModel->propertyValue("rows")->as<ViewModelInstanceList>()->addItem(
        item);
    settle();
    REQUIRE(list->itemCount() == 2);
  }

  bool rowHovered(int index = 0)
  {
    auto row = list->stateMachineInstance(index);
    REQUIRE(row != nullptr);
    return row->getBool("rowHovered")->value();
  }
};
} // namespace

TEST_CASE(
    "the fixture's fields take keys and presses without capture",
    "[capture]")
{
  auto file = ReadRiveFile("assets/capture_listeners.riv");
  HostScene scene(file.get());
  CHECK(StateMachineInstance::liveCaptureMachines() == 0);

  // A press on a field's text focuses it. The input is opaque, so the slot
  // around it never hears the press: the case capture is for.
  scene.click(kOnFieldBText);
  CHECK(scene.b.input->isFocused());
  CHECK_FALSE(scene.flag("slotBPressed"));

  // Up reaches the input first: a single line moves its caret to the start
  // and claims the key, so the slot's traversal never runs.
  scene.b.caret(2);
  CHECK(scene.press(Key::up));
  CHECK(scene.b.input->isFocused());
  CHECK(scene.b.caret() == 0);
}

TEST_CASE(
    "a host capture key listener runs ahead of a nested input",
    "[capture]")
{
  auto file = ReadRiveFile("assets/capture_listeners.riv");
  capture(file.get(), "Host", "Slot B Up");
  HostScene scene(file.get());
  scene.focus(scene.b);
  scene.b.caret(2);

  // The slot's capture listener claims Up: focus moves to the previous
  // field, and the input keeps its caret and its text.
  CHECK(scene.press(Key::up));
  CHECK(scene.a.input->isFocused());
  CHECK_FALSE(scene.b.input->isFocused());
  CHECK(scene.b.caret() == 2);
  CHECK(scene.b.input->text() == "AB");
  // The input's own Up listener never saw it.
  CHECK_FALSE(scene.b.flag("fieldUp"));

  // Down has no capture listener and still goes to the input, whose single
  // line moves the caret to the end.
  scene.focus(scene.b);
  scene.b.caret(0);
  CHECK(scene.press(Key::down));
  CHECK(scene.b.input->isFocused());
  CHECK(scene.b.caret() == 2);
}

TEST_CASE("an outer capture key listener beats a nested one", "[capture]")
{
  SECTION("host and field both capture Up")
  {
    auto file = ReadRiveFile("assets/capture_listeners.riv");
    capture(file.get(), "Host", "Slot B Up");
    capture(file.get(), "Field", "Field Up");
    HostScene scene(file.get());
    scene.focus(scene.b);
    CHECK(scene.press(Key::up));
    CHECK(scene.a.input->isFocused());
    CHECK_FALSE(scene.b.flag("fieldUp"));
  }
  SECTION("only the field captures Up")
  {
    auto file = ReadRiveFile("assets/capture_listeners.riv");
    capture(file.get(), "Field", "Field Up");
    HostScene scene(file.get());
    scene.focus(scene.b);
    scene.b.caret(2);
    CHECK(scene.press(Key::up));
    // The field's listener ran and claimed the key, ahead of the input's
    // editing and of the slot's normal traversal.
    CHECK(scene.b.flag("fieldUp"));
    CHECK(scene.b.input->isFocused());
    CHECK(scene.b.caret() == 2);
  }
}

TEST_CASE(
    "a capture key listener on the release phase claims the press too",
    "[capture]")
{
  auto file = ReadRiveFile("assets/capture_listeners.riv");
  capture(file.get(), "Host", "Slot B Enter Release");
  HostScene scene(file.get());
  scene.focus(scene.b);

  // The press is claimed without firing: the field's own Enter listener
  // never hears it.
  CHECK(scene.press(Key::enter));
  CHECK_FALSE(scene.flag("slotBEnterReleased"));
  CHECK_FALSE(scene.b.flag("fieldEnter"));
  // The release fires it.
  CHECK(scene.release(Key::enter));
  CHECK(scene.flag("slotBEnterReleased"));
}

TEST_CASE("a text input's own editing runs once per key", "[capture]")
{
  // The fixture's font shapes some letter pairs as one cluster, so a
  // Backspace can take two letters. What one edit does is read off the
  // input itself, and two edits must do more.
  auto backspaced = [](int times) {
    auto file = ReadRiveFile("assets/capture_listeners.riv");
    HostScene scene(file.get());
    scene.b.input->text("ABCDEF");
    scene.machine->advanceAndApply(0.0f);
    scene.b.caret(6);
    for (int i = 0; i < times; i++)
    {
      scene.b.input->keyInput(Key::backspace, KeyModifiers::none, true, false);
    }
    return scene.b.input->text();
  };
  const std::string once = backspaced(1);
  REQUIRE(once != backspaced(2));

  auto file = ReadRiveFile("assets/capture_listeners.riv");
  HostScene scene(file.get());
  scene.b.input->text("ABCDEF");
  scene.machine->advanceAndApply(0.0f);
  scene.focus(scene.b);
  scene.b.caret(6);
  // Two match-any listeners opt the input in; it still edits once.
  CHECK(scene.press(Key::backspace));
  CHECK(scene.b.input->text() == once);
}

TEST_CASE(
    "a filtered key listener on a text input runs its actions",
    "[capture]")
{
  auto file = ReadRiveFile("assets/capture_listeners.riv");
  HostScene scene(file.get());
  scene.focus(scene.b);

  // A single line leaves Enter alone; the listener asked for it by name, so
  // it fires and claims it.
  CHECK(scene.press(Key::enter));
  CHECK(scene.b.flag("fieldEnter"));
  // Its release isn't the listener's phase and the input ignores releases,
  // so it bubbles on to the slot's Enter release listener.
  CHECK(scene.release(Key::enter));
  CHECK(scene.flag("slotBEnterReleased"));
}

TEST_CASE(
    "keys a text input leaves alone keep bubbling past match-any "
    "listeners",
    "[capture]")
{
  auto file = ReadRiveFile("assets/capture_listeners.riv");
  HostScene scene(file.get());
  scene.focus(scene.b);
  CHECK_FALSE(scene.press(Key::tab));
  CHECK_FALSE(scene.press(Key::escape));
  CHECK(scene.b.input->isFocused());
}

TEST_CASE(
    "a host capture press on a field's slot keeps it from the field",
    "[capture]")
{
  auto file = ReadRiveFile("assets/capture_listeners.riv");
  capture(file.get(), "Host", "Slot B Press");
  HostScene scene(file.get());
  CHECK(StateMachineInstance::liveCaptureMachines() == 1);
  scene.b.caret(2);

  // Hovering the field still reaches it.
  scene.machine->pointerMove(kOnFieldBViewport);
  CHECK(scene.b.flag("fieldHovered"));

  // The press fires the slot's listener and never reaches the field: no
  // focus, no caret.
  CHECK(scene.machine->pointerDown(kOnFieldBText) != HitResult::none);
  scene.machine->advanceAndApply(0.0f);
  CHECK(scene.flag("slotBPressed"));
  CHECK_FALSE(scene.b.input->isFocused());
  CHECK(scene.b.caret() == 2);

  // The rest of the gesture is the slot's too: dragging across the text
  // selects nothing, and leaving the field doesn't reach its exit listener.
  scene.machine->pointerMove(kOnFieldBViewport);
  scene.machine->pointerMove(kOutsideSlots);
  scene.machine->advanceAndApply(0.0f);
  CHECK(scene.b.caret() == 2);
  CHECK(scene.b.flag("fieldHovered"));

  // The release hands the field back, and its hover catches up with where
  // the pointer is.
  scene.machine->pointerUp(kOutsideSlots);
  scene.machine->advanceAndApply(0.0f);
  CHECK_FALSE(scene.b.flag("fieldHovered"));
  CHECK_FALSE(scene.b.input->isFocused());

  // A press outside the claiming slot reaches its field as usual: the
  // Viewport's listener focuses the input, and the slot's listener under it
  // fires too.
  scene.click(kOnFieldAViewport);
  CHECK(scene.a.input->isFocused());
  CHECK(scene.flag("slotAPressed"));
}

TEST_CASE(
    "a nested machine's capture listener runs under a host without one",
    "[capture]")
{
  auto file = ReadRiveFile("assets/capture_listeners.riv");
  capture(file.get(), "Field", "Viewport Down");
  HostScene scene(file.get());
  scene.b.caret(0);

  // The field's Viewport claims the press on its text: its own action
  // focuses the input, and the input's built-in press handling never runs,
  // so the caret stays put.
  scene.click(kOnFieldBText);
  CHECK(scene.b.input->isFocused());
  CHECK(scene.b.caret() == 0);
}

TEST_CASE("a capture press that moves focus keeps it", "[capture]")
{
  auto file = ReadRiveFile("assets/capture_listeners.riv");
  capture(file.get(), "Field", "Viewport Down");
  HostScene scene(file.get());
  // Field B's whitespace, outside its input: the capture listener focuses
  // the input, and the press that did it doesn't blur it.
  scene.click(kOnFieldBViewport);
  CHECK(scene.b.input->isFocused());
  // A later press outside the input still does.
  scene.click(kOutsideSlots);
  CHECK_FALSE(scene.b.input->isFocused());
}

TEST_CASE("a capture move listener leaves nested hover alone", "[capture]")
{
  auto file = ReadRiveFile("assets/capture_listeners.riv");
  capture(file.get(), "Host", "Slot B Move");
  HostScene scene(file.get());

  scene.machine->pointerMove(kOnFieldBViewport);
  CHECK(scene.flag("slotBMoved"));
  CHECK(scene.b.flag("fieldHovered"));
  scene.machine->pointerMove(kOutsideSlots);
  CHECK_FALSE(scene.b.flag("fieldHovered"));

  // Without a press type it claims nothing.
  scene.click(kOnFieldBText);
  CHECK(scene.b.input->isFocused());
}

TEST_CASE("an opaque overlay above a capture target shields it", "[capture]")
{
  auto file = ReadRiveFile("assets/capture_listeners.riv");
  capture(file.get(), "Host", "Slot B Press");
  HostScene scene(file.get());

  scene.click(kUnderOverlay);
  CHECK(scene.flag("overlayPressed"));
  CHECK_FALSE(scene.flag("slotBPressed"));

  scene.click(kOnFieldBViewport);
  CHECK(scene.flag("slotBPressed"));
}

TEST_CASE("a capture claim holds one pointer only", "[capture]")
{
  auto file = ReadRiveFile("assets/capture_listeners.riv");
  capture(file.get(), "Host", "Slot B Press");
  HostScene scene(file.get());
  scene.b.caret(2);

  // Pointer 1 presses Field B and is claimed.
  scene.machine->pointerDown(kOnFieldBText, 1);
  scene.machine->advanceAndApply(0.0f);
  CHECK(scene.flag("slotBPressed"));
  CHECK_FALSE(scene.b.input->isFocused());

  // Pointer 2 still reaches Field B: hovering it runs the field's enter.
  scene.b.flag("fieldHovered", false);
  scene.machine->pointerMove(kOnFieldBViewport, 0, 2);
  CHECK(scene.b.flag("fieldHovered"));
  // ... and pressing Field A focuses it.
  scene.click(kOnFieldAText, 2);
  CHECK(scene.a.input->isFocused());

  // Pointer 2's events didn't hand pointer 1's gesture back to Field B.
  scene.machine->pointerMove(kOnFieldBViewport, 0, 1);
  scene.machine->pointerUp(kOnFieldBViewport, 1);
  scene.machine->advanceAndApply(0.0f);
  CHECK_FALSE(scene.b.input->isFocused());
  CHECK(scene.b.caret() == 2);
}

TEST_CASE(
    "a press with another button keeps the first button's claim",
    "[capture]")
{
  auto file = ReadRiveFile("assets/capture_listeners.riv");
  capture(file.get(), "Host", "Slot B Press");
  HostScene scene(file.get());

  // Primary presses Field B and is claimed. Secondary joins it, and lets go
  // first: neither says the primary press lost its release.
  scene.machine->pointerDown(kOnFieldBText, 0, PointerButton::primary);
  scene.machine->pointerDown(kOnFieldBText, 0, PointerButton::secondary);
  scene.machine->pointerUp(kOnFieldBText, 0, PointerButton::secondary);
  scene.machine->advanceAndApply(0.0f);
  CHECK(scene.flag("slotBPressed"));
  scene.machine->pointerMove(kOnFieldBViewport);
  scene.machine->advanceAndApply(0.0f);
  CHECK_FALSE(scene.b.flag("fieldHovered"));
  CHECK_FALSE(scene.b.input->isFocused());

  // The primary release hands the field back.
  scene.machine->pointerUp(kOnFieldBViewport, 0, PointerButton::primary);
  scene.machine->advanceAndApply(0.0f);
  CHECK(scene.b.flag("fieldHovered"));
}

TEST_CASE("a capture drag's release stays off the claimed field", "[capture]")
{
  auto file = ReadRiveFile("assets/capture_listeners.riv");
  {
    // In the normal phase, a release on the field reaches its up listener.
    HostScene scene(file.get());
    scene.click(kOnFieldAViewport);
    REQUIRE(scene.a.flag("fieldReleased"));
  }
  capture(file.get(), "Host", "Slot A Drag");
  HostScene scene(file.get());
  const Vec2D dragged = kOnFieldAViewport + Vec2D(5.0f, 0.0f);
  scene.machine->pointerDown(kOnFieldAViewport);
  scene.machine->pointerMove(dragged);
  scene.machine->advanceAndApply(0.0f);
  CHECK(scene.flag("slotADragged"));

  // The release ends the drag, which turns the slot's machine back on, but
  // the claim holds through the release itself.
  scene.machine->pointerUp(dragged);
  scene.machine->advanceAndApply(0.0f);
  CHECK_FALSE(scene.a.flag("fieldReleased"));
  // Then the field's hover catches up with the pointer.
  CHECK(scene.a.flag("fieldHovered"));
}

TEST_CASE(
    "a host claim holds while something drawn above covers the field",
    "[capture]")
{
  auto file = ReadRiveFile("assets/capture_listeners.riv");
  capture(file.get(), "Host", "Slot B Press");
  SECTION("the field's own listeners") {}
  SECTION("a capture listener in the field")
  {
    capture(file.get(), "Field", "Viewport Enter");
  }
  HostScene scene(file.get());

  scene.machine->pointerDown(kOnFieldBViewport);
  scene.machine->advanceAndApply(0.0f);
  REQUIRE(scene.flag("slotBPressed"));
  // Under the opaque Overlay, which covers Field B and so sends it an exit,
  // and back. The press is still held, so the claim still is.
  scene.machine->pointerMove(kUnderOverlay);
  scene.machine->pointerMove(kOnFieldBViewport);
  scene.machine->advanceAndApply(0.0f);
  CHECK_FALSE(scene.b.flag("fieldHovered"));
  CHECK_FALSE(scene.b.input->isFocused());

  scene.machine->pointerUp(kOnFieldBViewport);
  scene.machine->advanceAndApply(0.0f);
  CHECK(scene.b.flag("fieldHovered"));
}

TEST_CASE(
    "a host claim holds while something drawn above covers a list row",
    "[capture]")
{
  auto file = ReadRiveFile("assets/capture_listeners.riv");
  {
    // Without a claim, the overlay covering the row is the row's exit.
    ListScene scene(file.get());
    scene.machine->pointerMove(kOnListRow);
    REQUIRE(scene.rowHovered());
    scene.machine->pointerMove(kUnderListOverlay);
    REQUIRE_FALSE(scene.rowHovered());
  }
  capture(file.get(), "List Host", "List Slot Press");
  ListScene scene(file.get());
  scene.machine->pointerDown(kOnListRow);
  scene.machine->advanceAndApply(0.0f);
  REQUIRE(scene.machine->getBool("listSlotPressed")->value());
  scene.machine->pointerMove(kUnderListOverlay);
  scene.machine->pointerMove(kOnListRow);
  scene.machine->advanceAndApply(0.0f);
  CHECK_FALSE(scene.rowHovered());

  scene.machine->pointerUp(kOnListRow);
  scene.machine->advanceAndApply(0.0f);
  CHECK(scene.rowHovered());
}

TEST_CASE(
    "an outer claim for another button keeps what an inner one held",
    "[capture]")
{
  auto file = ReadRiveFile("assets/capture_listeners.riv");
  capture(file.get(), "Claims", "Outer Secondary Press");
  capture(file.get(), "Claims", "Inner Press");
  capture(file.get(), "Claims", "Leaf Move");
  auto artboard = file->artboardNamed("Claims");
  REQUIRE(artboard != nullptr);
  auto machine = artboard->stateMachineAt(0);
  machine->advanceAndApply(0.0f);
  machine->advanceAndApply(0.0f);
  auto flag = [&](const char* name) { return machine->getBool(name)->value(); };

  // Primary claims Inner, which turns Leaf's capture listener off; then
  // secondary claims Outer, which covers Leaf too.
  machine->pointerDown(kOnLeaf, 0, PointerButton::primary);
  machine->pointerDown(kOnLeaf, 0, PointerButton::secondary);
  machine->advanceAndApply(0.0f);
  REQUIRE(flag("innerPressed"));
  REQUIRE(flag("outerPressed"));

  // Letting go of primary ends Inner's claim, but Outer's still holds Leaf.
  machine->pointerUp(kOnLeaf, 0, PointerButton::primary);
  machine->pointerMove(kOnLeaf + Vec2D(5.0f, 0.0f));
  machine->advanceAndApply(0.0f);
  CHECK_FALSE(flag("leafMoved"));

  machine->pointerUp(kOnLeaf, 0, PointerButton::secondary);
  machine->pointerMove(kOnLeaf);
  machine->advanceAndApply(0.0f);
  CHECK(flag("leafMoved"));
}

TEST_CASE(
    "a nested machine's own release leaves its host's claim in place",
    "[capture]")
{
  auto file = ReadRiveFile("assets/capture_listeners.riv");
  // Primary is claimed inside Field B by its Viewport, then secondary by
  // the host's Slot B, which turns the whole field off.
  capture(file.get(), "Field", "Viewport Down");
  capture(file.get(), "Host", "Slot B Secondary Press");
  HostScene scene(file.get());
  scene.machine->pointerDown(kOnFieldBViewport, 0, PointerButton::primary);
  scene.machine->pointerDown(kOnFieldBViewport, 0, PointerButton::secondary);
  scene.machine->advanceAndApply(0.0f);
  REQUIRE(scene.flag("slotBSecondaryPressed"));

  // The field's primary release ends its own claim, not the host's.
  scene.machine->pointerUp(kOnFieldBViewport, 0, PointerButton::primary);
  scene.machine->pointerMove(kOnFieldBViewport + Vec2D(5.0f, 0.0f));
  scene.machine->advanceAndApply(0.0f);
  CHECK_FALSE(scene.b.flag("fieldHovered"));

  scene.machine->pointerUp(kOnFieldBViewport, 0, PointerButton::secondary);
  scene.machine->advanceAndApply(0.0f);
  CHECK(scene.b.flag("fieldHovered"));
}

TEST_CASE("a list row added during a claim is under it too", "[capture]")
{
  auto file = ReadRiveFile("assets/capture_listeners.riv");
  {
    // Without a claim, the new row hears the pointer.
    ListScene scene(file.get());
    scene.addRow(file.get());
    scene.machine->pointerMove(kOnSecondListRow);
    REQUIRE(scene.rowHovered(1));
  }
  capture(file.get(), "List Host", "List Slot Press");
  ListScene scene(file.get());
  scene.machine->pointerDown(kOnListRow);
  scene.machine->advanceAndApply(0.0f);
  REQUIRE(scene.machine->getBool("listSlotPressed")->value());

  scene.addRow(file.get());
  scene.machine->pointerMove(kOnSecondListRow);
  scene.machine->advanceAndApply(0.0f);
  CHECK_FALSE(scene.rowHovered(1));

  scene.machine->pointerUp(kOnSecondListRow);
  scene.machine->advanceAndApply(0.0f);
  CHECK(scene.rowHovered(1));
}

TEST_CASE(
    "pointer dispatch skips the capture phase while nothing captures",
    "[capture]")
{
  auto file = ReadRiveFile("assets/capture_listeners.riv");
  HostScene scene(file.get());
  REQUIRE(StateMachineInstance::liveCaptureMachines() == 0);

  auto visits = StateMachineInstance::sm_capturePassVisits;
  scene.machine->pointerMove(kOnFieldBViewport);
  scene.click(kOnFieldBText);
  scene.machine->pointerExit(kOutsideSlots);
  CHECK(StateMachineInstance::sm_capturePassVisits == visits);

  // Another file's machine with a capture listener doesn't turn it on for
  // a tree without one.
  auto captureFile = ReadRiveFile("assets/capture_listeners.riv");
  capture(captureFile.get(), "Host", "Slot B Move");
  HostScene captureScene(captureFile.get());
  REQUIRE(StateMachineInstance::liveCaptureMachines() == 1);
  visits = StateMachineInstance::sm_capturePassVisits;
  scene.machine->pointerMove(kOnFieldBViewport);
  scene.click(kOnFieldBText);
  CHECK(StateMachineInstance::sm_capturePassVisits == visits);

  // In the tree that has one, the pass runs, and only reaches the machines
  // that capture: the nested fields have no capture listener.
  visits = StateMachineInstance::sm_capturePassVisits;
  captureScene.machine->pointerMove(kOnFieldBViewport);
  CHECK(StateMachineInstance::sm_capturePassVisits == visits + 1);
}
#endif
