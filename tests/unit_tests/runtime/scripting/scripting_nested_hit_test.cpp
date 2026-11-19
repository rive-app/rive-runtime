#if defined(WITH_RIVE_SCRIPTING_LUAU) && defined(WITH_RIVE_TOOLS)
#include "catch.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include "rive/nested_artboard.hpp"
#include "rive/scripted/scripted_layout.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/viewmodel_instance_boolean.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "rive_file_reader.hpp"

using namespace rive;

// nested_script_hit.riv (source: assets/rml/nested_script_hit/, unsigned, so
// tools builds only): Game's script node claims every press, move and
// release and counts them on the root view model. Host nests Game in a
// clipping box at (50,50)..(250,250), beside a Button at (450,100)..(550,200).
// The Layout artboards are the same with the script as a scripted layout.
namespace
{
struct TouchScene
{
  rcp<File> file;
  std::unique_ptr<ArtboardInstance> artboard;
  std::unique_ptr<StateMachineInstance> machine;
  rcp<ViewModelInstance> vm;

  explicit TouchScene(const char* name)
  {
    file = ReadRiveFile("assets/nested_script_hit.riv");
    artboard = file->artboardNamed(name);
    REQUIRE(artboard != nullptr);
    machine = artboard->stateMachineAt(0);
    REQUIRE(machine != nullptr);
    vm = file->createViewModelInstance(std::string("H"));
    REQUIRE(vm != nullptr);
    machine->bindViewModelInstance(vm);
    advance();
  }

  void advance()
  {
    machine->advanceAndApply(0.0f);
    machine->advanceAndApply(0.0f);
  }

  void down(float x, float y, PointerButton button = PointerButton::primary)
  {
    machine->pointerDown(Vec2D(x, y), 0, button);
  }
  void move(float x, float y) { machine->pointerMove(Vec2D(x, y)); }
  void up(float x, float y, PointerButton button = PointerButton::primary)
  {
    machine->pointerUp(Vec2D(x, y), 0, button);
  }

  bool flag(const char* name)
  {
    return vm->propertyValue(name)
        ->as<ViewModelInstanceBoolean>()
        ->propertyValue();
  }
  void press(Vec2D at)
  {
    vm->propertyValue("downs")->as<ViewModelInstanceNumber>()->propertyValue(0);
    vm->propertyValue("hit")->as<ViewModelInstanceBoolean>()->propertyValue(
        false);
    machine->pointerDown(at);
    machine->pointerUp(at);
    advance();
  }
  int count(const char* name)
  {
    return (int)vm->propertyValue(name)
        ->as<ViewModelInstanceNumber>()
        ->propertyValue();
  }
  Vec2D layoutSize()
  {
    Vec2D size;
    for (auto* object : artboard->objects())
    {
      if (object != nullptr && object->is<ScriptedLayout>())
      {
        object->as<ScriptedLayout>()->layoutSize(size);
      }
    }
    return size;
  }
};

void checkHostGetsPressFar(const char* host)
{
  TouchScene s(host);
  s.down(500, 150);
  s.up(500, 150);
  s.advance();
  CHECK(s.flag("pressed"));
  CHECK(s.flag("released"));
  CHECK(s.count("downs") == 0);
  CHECK(s.count("ups") == 0);
}

void checkHostGetsHoverFar(const char* host)
{
  TouchScene s(host);
  s.move(500, 150);
  s.advance();
  CHECK(s.flag("over"));
  CHECK(s.count("moves") == 0);
  CHECK(s.count("exits") == 0);
}

void checkTakesPressInside(const char* host)
{
  TouchScene s(host);
  s.down(150, 150);
  s.up(150, 150);
  s.advance();
  CHECK(s.count("downs") == 1);
  CHECK(s.count("ups") == 1);
  CHECK_FALSE(s.flag("pressed"));
  CHECK_FALSE(s.flag("released"));
}

void checkKeepsDrag(const char* host)
{
  TouchScene s(host);
  s.down(150, 150);
  s.move(300, 150);
  s.move(500, 150);
  s.up(500, 150);
  s.advance();
  CHECK(s.count("downs") == 1);
  CHECK(s.count("moves") == 2);
  CHECK(s.count("ups") == 1);
  CHECK_FALSE(s.flag("over"));
  CHECK_FALSE(s.flag("pressed"));
  CHECK_FALSE(s.flag("released"));
}

void checkOneExit(const char* host)
{
  TouchScene s(host);
  s.move(150, 150);
  s.move(500, 150);
  s.move(510, 150);
  s.advance();
  CHECK(s.count("moves") == 1);
  CHECK(s.count("exits") == 1);
  CHECK(s.flag("over"));
}

void checkRootHearsAnywhere(const char* game)
{
  TouchScene s(game);
  s.move(500, 150);
  s.down(500, 150);
  s.up(500, 150);
  s.advance();
  CHECK(s.count("moves") == 1);
  CHECK(s.count("downs") == 1);
  CHECK(s.count("ups") == 1);
  CHECK(s.count("exits") == 0);
}

// The overflow hosts nest the game at (50,50)..(250,250) in a clip of
// (50,50)..(200,300).
void checkPressOnlyWhereShown(const char* host)
{
  TouchScene s(host);
  s.down(100, 100);
  s.up(100, 100);
  CHECK(s.count("downs") == 1);
  // In the game's box, clipped away by the host.
  s.down(225, 100);
  s.up(225, 100);
  CHECK(s.count("downs") == 1);
  // In the host's clip, past the game's box.
  s.down(100, 275);
  s.up(100, 275);
  CHECK(s.count("downs") == 1);
}

// The cover hosts draw an opaque Cover at (200,100)..(300,200) above the
// game, which they nest at (50,50)..(250,250). DeepCoverHost nests it one
// artboard deeper.
void checkKeepsPressUnderCover(const char* host)
{
  TouchScene s(host);
  s.down(150, 150);
  s.move(225, 150);
  s.move(275, 150);
  const int movesUnderCover = s.count("moves");
  s.move(350, 150);
  s.move(500, 150);
  s.up(500, 150);
  s.advance();
  CHECK(s.flag("covered"));
  CHECK(s.count("downs") == 1);
  CHECK(s.count("moves") == movesUnderCover + 2);
  CHECK(s.count("ups") == 1);
  CHECK(s.count("exits") == 0);
  CHECK_FALSE(s.flag("over"));
}

void checkLiftUnderCoverEndsPress(const char* host)
{
  TouchScene s(host);
  s.down(150, 150);
  s.move(225, 150);
  const int moves = s.count("moves");
  s.up(225, 150);
  s.move(500, 150);
  s.advance();
  CHECK(s.flag("covered"));
  CHECK(s.count("moves") == moves);
  CHECK(s.count("exits") == 1);
  CHECK(s.flag("over"));
}

void checkSurfaceExit(const char* host)
{
  TouchScene s(host);
  s.move(150, 150);
  s.down(150, 150);
  s.machine->pointerExit(Vec2D(150, 150));
  s.move(500, 150);
  s.advance();
  CHECK(s.count("moves") == 1);
  CHECK(s.count("exits") == 1);
  CHECK(s.flag("over"));
}

void checkNoExitOwed(const char* host)
{
  TouchScene s(host);
  s.move(500, 150);
  s.machine->pointerExit(Vec2D(500, 150));
  CHECK(s.count("exits") == 0);
  s.move(150, 150);
  s.move(225, 150);
  const int moves = s.count("moves");
  s.machine->pointerExit(Vec2D(225, 150));
  s.advance();
  CHECK(s.count("moves") == moves);
  CHECK(s.count("exits") == 0);
}

// The transformed hosts clip ClipGame's script and its Target shape to
// (50,50)..(200,200).
void checkHearsWhatShows(
    const char* host,
    std::vector<Vec2D> shown,
    std::vector<Vec2D> hidden)
{
  TouchScene s(host);
  for (Vec2D at : shown)
  {
    INFO(at.x << "," << at.y);
    s.press(at);
    CHECK(s.count("downs") == 1);
    CHECK(s.flag("hit"));
  }
  for (Vec2D at : hidden)
  {
    INFO(at.x << "," << at.y);
    s.press(at);
    CHECK(s.count("downs") == 0);
    CHECK_FALSE(s.flag("hit"));
  }
}

void checkTwoButtons(const char* host, PointerButton first, PointerButton last)
{
  TouchScene s(host);
  s.down(150, 150, PointerButton::primary);
  s.down(150, 150, PointerButton::secondary);
  s.move(500, 150);
  s.up(500, 150, first);
  s.move(510, 150);
  CHECK(s.count("moves") == 2);
  s.up(510, 150, last);
  s.advance();
  CHECK(s.count("downs") == 2);
  CHECK(s.count("ups") == 2);
  CHECK(s.count("exits") == 0);
  s.move(520, 150);
  CHECK(s.count("exits") == 1);
}

void checkButtonPressedOutside(const char* host)
{
  TouchScene s(host);
  s.down(150, 150, PointerButton::primary);
  s.move(500, 150);
  s.down(500, 150, PointerButton::secondary);
  s.up(500, 150, PointerButton::secondary);
  s.move(510, 150);
  s.up(510, 150, PointerButton::primary);
  s.advance();
  CHECK(s.count("downs") == 2);
  CHECK(s.count("moves") == 2);
  CHECK(s.count("ups") == 2);
  CHECK(s.count("exits") == 0);
}

void checkEndLetsGoOfAllButtons(const char* host)
{
  TouchScene s(host);
  s.down(150, 150, PointerButton::primary);
  s.down(150, 150, PointerButton::secondary);
  s.machine->pointerExit(Vec2D(150, 150));
  s.move(500, 150);
  s.up(500, 150, PointerButton::secondary);
  s.up(500, 150, PointerButton::primary);
  s.advance();
  CHECK(s.count("moves") == 0);
  CHECK(s.count("ups") == 0);
  CHECK(s.count("exits") == 1);
}

// A host that sends no button: a second down can't be told from the first.
void checkRepeatedDownIsOnePress(const char* host)
{
  TouchScene s(host);
  s.down(150, 150);
  s.down(150, 150);
  s.move(500, 150);
  s.up(500, 150);
  s.move(510, 150);
  s.up(510, 150);
  s.advance();
  CHECK(s.count("downs") == 2);
  CHECK(s.count("moves") == 1);
  CHECK(s.count("ups") == 1);
  CHECK(s.count("exits") == 1);
}

void checkHostDragEarnsNoExit(const char* host)
{
  TouchScene s(host);
  s.machine->dragStart(Vec2D(150, 150), 0.0f, false);
  s.move(500, 150);
  s.advance();
  CHECK(s.count("moves") == 0);
  CHECK(s.count("exits") == 0);
}
} // namespace

TEST_CASE(
    "a host gets a press far from a nested artboard with a claiming script",
    "[scripting][hittest]")
{
  checkHostGetsPressFar("Host");
}

TEST_CASE(
    "a host gets a hover far from a nested artboard with a claiming script",
    "[scripting][hittest]")
{
  checkHostGetsHoverFar("Host");
}

TEST_CASE(
    "a nested script node still takes a press inside its artboard",
    "[scripting][hittest]")
{
  checkTakesPressInside("Host");
}

TEST_CASE(
    "a nested script node keeps a drag that leaves its artboard",
    "[scripting][hittest]")
{
  checkKeepsDrag("Host");
}

TEST_CASE(
    "a nested script node gets one exit when the pointer leaves its artboard",
    "[scripting][hittest]")
{
  checkOneExit("Host");
}

TEST_CASE(
    "a script node on a root artboard still hears a press anywhere",
    "[scripting][hittest]")
{
  checkRootHearsAnywhere("Game");
}

TEST_CASE(
    "a nested script node takes a press only where its artboard shows",
    "[scripting][hittest]")
{
  checkPressOnlyWhereShown("Overflow");
}

TEST_CASE(
    "a host gets a press far from a nested artboard with a claiming layout",
    "[scripting][hittest]")
{
  checkHostGetsPressFar("LayoutHost");
}

TEST_CASE(
    "a host gets a hover far from a nested artboard with a claiming layout",
    "[scripting][hittest]")
{
  checkHostGetsHoverFar("LayoutHost");
}

TEST_CASE(
    "a nested scripted layout still takes a press inside its artboard",
    "[scripting][hittest]")
{
  checkTakesPressInside("LayoutHost");
}

TEST_CASE(
    "a nested scripted layout keeps a drag that leaves its artboard",
    "[scripting][hittest]")
{
  checkKeepsDrag("LayoutHost");
}

TEST_CASE(
    "a nested scripted layout gets one exit when the pointer leaves its "
    "artboard",
    "[scripting][hittest]")
{
  checkOneExit("LayoutHost");
}

TEST_CASE(
    "a scripted layout on a root artboard still hears a press anywhere",
    "[scripting][hittest]")
{
  checkRootHearsAnywhere("LayoutGame");
  CHECK(TouchScene("LayoutGame").layoutSize() == Vec2D(200, 200));
}

TEST_CASE(
    "a nested scripted layout takes a press only where its artboard shows",
    "[scripting][hittest]")
{
  checkPressOnlyWhereShown("LayoutOverflow");
}

// SmallLayoutGame's layout is a 100 x 100 box at (50,50) in its 200 x 200
// artboard; SmallLayoutHost nests that artboard at (50,50).
TEST_CASE(
    "a host gets a press far from a nested small claiming layout",
    "[scripting][hittest]")
{
  checkHostGetsPressFar("SmallLayoutHost");
}

TEST_CASE(
    "a nested scripted layout hears its whole artboard, not only its box",
    "[scripting][hittest]")
{
  TouchScene s("SmallLayoutHost");
  s.down(225, 225);
  s.up(225, 225);
  s.down(75, 75);
  s.up(75, 75);
  CHECK(s.count("downs") == 2);
}

TEST_CASE(
    "a small scripted layout on a root artboard still hears a press anywhere",
    "[scripting][hittest]")
{
  checkRootHearsAnywhere("SmallLayoutGame");
  TouchScene s("SmallLayoutGame");
  CHECK(s.layoutSize() == Vec2D(100, 100));
  s.down(175, 175);
  s.up(175, 175);
  CHECK(s.count("downs") == 1);
}

TEST_CASE(
    "a nested script node keeps a press its drag carries under a host shape",
    "[scripting][hittest]")
{
  checkKeepsPressUnderCover("CoverHost");
}

TEST_CASE(
    "a nested scripted layout keeps a press its drag carries under a host "
    "shape",
    "[scripting][hittest]")
{
  checkKeepsPressUnderCover("CoverLayoutHost");
}

TEST_CASE(
    "a nested script node loses a press lifted under a host shape",
    "[scripting][hittest]")
{
  checkLiftUnderCoverEndsPress("CoverHost");
}

TEST_CASE(
    "a nested scripted layout loses a press lifted under a host shape",
    "[scripting][hittest]")
{
  checkLiftUnderCoverEndsPress("CoverLayoutHost");
}

TEST_CASE(
    "a nested script node gets one exit when the pointer leaves the host",
    "[scripting][hittest]")
{
  checkSurfaceExit("Host");
}

TEST_CASE(
    "a nested scripted layout gets one exit when the pointer leaves the host",
    "[scripting][hittest]")
{
  checkSurfaceExit("LayoutHost");
}

TEST_CASE(
    "a nested script node gets no exit from the host for a pointer it let go",
    "[scripting][hittest]")
{
  checkNoExitOwed("CoverHost");
}

TEST_CASE(
    "a nested scripted layout gets no exit from the host for a pointer it let "
    "go",
    "[scripting][hittest]")
{
  checkNoExitOwed("CoverLayoutHost");
}

TEST_CASE(
    "a script node two artboards deep keeps a press its drag carries under a "
    "host shape",
    "[scripting][hittest]")
{
  checkKeepsPressUnderCover("DeepCoverHost");
}

TEST_CASE(
    "a script node two artboards deep loses a press lifted under a host shape",
    "[scripting][hittest]")
{
  checkLiftUnderCoverEndsPress("DeepCoverHost");
}

// The middle artboard has to keep the press through the cover's hover exit,
// or the lift never reaches the paused Game.
TEST_CASE(
    "a script node two artboards deep loses a press lifted while it is paused",
    "[scripting][hittest]")
{
  TouchScene s("DeepCoverHost");
  auto* game = s.artboard->find<NestedArtboard>("Slot")
                   ->artboardInstance()
                   ->find<NestedArtboard>("Game");
  REQUIRE(game != nullptr);
  s.down(150, 150);
  s.move(225, 150);
  game->isPaused(true);
  s.up(225, 150);
  game->isPaused(false);
  const int moves = s.count("moves");
  s.move(500, 150);
  s.advance();
  CHECK(s.count("moves") == moves);
  CHECK(s.flag("over"));
}

TEST_CASE(
    "a nested script node gets no exit for a host drag it never heard",
    "[scripting][hittest]")
{
  checkHostDragEarnsNoExit("Host");
}

TEST_CASE(
    "a nested scripted layout gets no exit for a host drag it never heard",
    "[scripting][hittest]")
{
  checkHostDragEarnsNoExit("LayoutHost");
}

TEST_CASE(
    "a nested script keeps a press until both held buttons let go",
    "[scripting][hittest][pointer_button]")
{
  for (const char* host : {"Host", "LayoutHost"})
  {
    INFO(host);
    checkTwoButtons(host, PointerButton::secondary, PointerButton::primary);
    checkTwoButtons(host, PointerButton::primary, PointerButton::secondary);
  }
}

TEST_CASE(
    "a nested script keeps a press through a button pressed outside it",
    "[scripting][hittest][pointer_button]")
{
  checkButtonPressedOutside("Host");
  checkButtonPressedOutside("LayoutHost");
}

TEST_CASE(
    "a nested script lets go of every held button when its pointer ends",
    "[scripting][hittest][pointer_button]")
{
  checkEndLetsGoOfAllButtons("Host");
  checkEndLetsGoOfAllButtons("LayoutHost");
}

TEST_CASE(
    "a nested script takes a repeated down of one button as one press",
    "[scripting][hittest][pointer_button]")
{
  checkRepeatedDownIsOnePress("Host");
  checkRepeatedDownIsOnePress("LayoutHost");
}

// HoverGame's script has a move handler that never claims, and no down
// handler.
TEST_CASE(
    "a nested script node hears a press that started inside its artboard",
    "[scripting][hittest]")
{
  TouchScene s("HoverHost");
  s.down(150, 150);
  s.move(300, 150);
  s.move(500, 150);
  s.up(500, 150);
  s.advance();
  CHECK(s.count("moves") == 2);
  CHECK(s.count("exits") == 0);
  CHECK(s.flag("over"));
}

TEST_CASE(
    "a nested script hears only what shows when its mount scales it",
    "[scripting][hittest]")
{
  checkHearsWhatShows(
      "ScaledMountHost",
      {Vec2D(100, 100), Vec2D(195, 195)},
      {Vec2D(300, 100), Vec2D(100, 250), Vec2D(240, 100)});
}

TEST_CASE(
    "a nested script hears only what shows when its mount turns it",
    "[scripting][hittest]")
{
  checkHearsWhatShows(
      "RotatedMountHost",
      {Vec2D(100, 100), Vec2D(60, 190), Vec2D(195, 60)},
      {Vec2D(100, 225), Vec2D(25, 150)});
}

TEST_CASE(
    "a nested script hears only what shows when a fit scales it",
    "[scripting][hittest]")
{
  checkHearsWhatShows(
      "FitHost",
      {Vec2D(100, 100), Vec2D(195, 195)},
      {Vec2D(300, 100), Vec2D(100, 250), Vec2D(240, 100)});
}
#endif
