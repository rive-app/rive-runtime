#include "catch.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/data_bind/data_context.hpp"
#include "rive/layout_component.hpp"
#include "rive/scripted/decoded_file.hpp"
#include "rive/shapes/paint/solid_color.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/viewmodel/viewmodel_instance_color.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "rive/viewmodel/viewmodel_instance_trigger.hpp"
#include "rive_file_reader.hpp"
#include "utils/no_op_renderer.hpp"

#include <algorithm>
#include <type_traits>
#include <vector>

using namespace rive;

static_assert(std::is_move_constructible<DecodedRoot>::value, "");
static_assert(!std::is_move_assignable<DecodedRoot>::value, "");
static_assert(!std::is_copy_constructible<DecodedRoot>::value, "");

namespace
{
// Built from assets/rml/decoded_game_guest.rml.
constexpr const char* kGuest = "assets/decoded_game_guest.riv";
constexpr uint32_t kGreen = 0xFF00FF00;
constexpr uint32_t kRed = 0xFFFF0000;

uint32_t fillOf(ArtboardInstance* artboard, const char* name)
{
  for (auto* object : artboard->objects())
  {
    if (object == nullptr || !object->is<SolidColor>())
    {
      continue;
    }
    auto* color = object->as<SolidColor>();
    for (auto* parent = color->parent(); parent != nullptr;
         parent = parent->parent())
    {
      if (parent->name() == name)
      {
        return color->colorValue();
      }
    }
  }
  return 0;
}

// Paths are per instance, so they tell a watermark's draw from its artboard's.
class PathRecordingRenderer : public NoOpRenderer
{
public:
  std::vector<RenderPath*> paths;
  void drawPath(RenderPath* path, RenderPaint*) override
  {
    paths.push_back(path);
  }
};

std::vector<RenderPath*> drawnBy(Artboard* artboard, bool internal)
{
  PathRecordingRenderer renderer;
  if (internal)
  {
    artboard->drawInternal(&renderer);
  }
  else
  {
    artboard->draw(&renderer);
  }
  return renderer.paths;
}

ViewModelInstanceColor* accentOf(const rcp<ViewModelInstance>& theme)
{
  return theme->propertyValue("accent")->as<ViewModelInstanceColor>();
}
} // namespace

TEST_CASE(
    "a decoded root binds its main and its file's globals",
    "[decoded_root]")
{
  auto file = ReadRiveFile(kGuest);
  auto root = decodedRoot(*file, nullptr);
  REQUIRE(root.artboard != nullptr);
  CHECK(root.artboard->name() == "Game");
  REQUIRE(root.stateMachine != nullptr);
  REQUIRE(root.viewModel != nullptr);
  CHECK(root.viewModel->viewModel() == file->viewModel("Game"));
  CHECK(
      root.viewModel->propertyValue("score")
          ->as<ViewModelInstanceNumber>()
          ->propertyValue() == 7.0f);

  auto theme = root.stateMachine->globalViewModelInstance("Theme");
  REQUIRE(theme != nullptr);
  CHECK(theme->viewModel() == file->viewModel("Theme"));
  CHECK(accentOf(theme)->propertyValue() == kGreen);

  REQUIRE(root.artboard->dataContext() != nullptr);
  CHECK(root.artboard->dataContext()->parent() == nullptr);

  root.stateMachine->advanceAndApply(0.0f);
  CHECK(fillOf(root.artboard.get(), "Target") == kGreen);
}

TEST_CASE(
    "a decoded root without a state machine still binds its globals",
    "[decoded_root]")
{
  auto file = ReadRiveFile(kGuest);
  auto root = decodedRoot(*file, "Other");
  REQUIRE(root.artboard != nullptr);
  CHECK(root.artboard->name() == "Other");
  CHECK(root.artboard->width() == 300.0f);
  CHECK(root.stateMachine == nullptr);
  CHECK(root.viewModel == nullptr);
  REQUIRE(root.artboard->globalViewModelInstance("Theme") != nullptr);
  root.artboard->advance(0.0f);
  CHECK(fillOf(root.artboard.get(), "Swatch") == kGreen);
}

TEST_CASE(
    "decodedRoot answers empty for an artboard the file lacks",
    "[decoded_root]")
{
  auto file = ReadRiveFile(kGuest);
  auto root = decodedRoot(*file, "Nope");
  CHECK(root.artboard == nullptr);
  CHECK(root.stateMachine == nullptr);
  CHECK(root.viewModel == nullptr);
}

TEST_CASE(
    "a decoded root lays out first at the size set before its first "
    "advance",
    "[decoded_root]")
{
  auto file = ReadRiveFile(kGuest);
  auto root = decodedRoot(*file, nullptr);
  REQUIRE(root.artboard != nullptr);
  auto* target = root.artboard->find<LayoutComponent>("Target");
  REQUIRE(target != nullptr);
  // No layout yet at the file's 600x600, which would put Target at 200.
  CHECK(target->worldTransform().tx() != 200.0f);
  root.artboard->width(1000.0f);
  root.artboard->height(620.0f);
  root.stateMachine->advanceAndApply(0.0f);
  CHECK(target->worldTransform().tx() == 400.0f);
  CHECK(target->worldTransform().ty() == 210.0f);
}

TEST_CASE("decoded roots are independent of each other", "[decoded_root]")
{
  auto file = ReadRiveFile(kGuest);
  auto a = decodedRoot(*file, nullptr);
  auto b = decodedRoot(*file, nullptr);
  REQUIRE(a.artboard != nullptr);
  REQUIRE(b.artboard != nullptr);
  CHECK(a.viewModel != b.viewModel);
  auto themeA = a.stateMachine->globalViewModelInstance("Theme");
  auto themeB = b.stateMachine->globalViewModelInstance("Theme");
  REQUIRE(themeA != nullptr);
  REQUIRE(themeB != nullptr);
  CHECK(themeA != themeB);

  accentOf(themeA)->propertyValue(kRed);
  a.viewModel->propertyValue("score")
      ->as<ViewModelInstanceNumber>()
      ->propertyValue(1.0f);
  a.stateMachine->advanceAndApply(0.0f);
  b.stateMachine->advanceAndApply(0.0f);
  CHECK(fillOf(a.artboard.get(), "Target") == kRed);
  CHECK(fillOf(b.artboard.get(), "Target") == kGreen);
  CHECK(
      b.viewModel->propertyValue("score")
          ->as<ViewModelInstanceNumber>()
          ->propertyValue() == 7.0f);
}

TEST_CASE("a decoded root's globals are its own file's", "[decoded_root]")
{
  // Two loads of one file stand in for a host and the file it decodes.
  auto host = ReadRiveFile(kGuest);
  auto guest = ReadRiveFile(kGuest);
  auto hostRoot = decodedRoot(*host, nullptr);
  auto guestRoot = decodedRoot(*guest, nullptr);
  auto hostTheme = hostRoot.stateMachine->globalViewModelInstance("Theme");
  auto guestTheme = guestRoot.stateMachine->globalViewModelInstance("Theme");
  REQUIRE(hostTheme != nullptr);
  REQUIRE(guestTheme != nullptr);
  CHECK(guestTheme->viewModel() == guest->viewModel("Theme"));
  CHECK(guestTheme->viewModel() != host->viewModel("Theme"));
  accentOf(hostTheme)->propertyValue(kRed);
  hostRoot.stateMachine->advanceAndApply(0.0f);
  guestRoot.stateMachine->advanceAndApply(0.0f);
  CHECK(fillOf(guestRoot.artboard.get(), "Target") == kGreen);
}

TEST_CASE(
    "decodedRootLike binds only its own file's view model instances",
    "[decoded_root]")
{
  auto file = ReadRiveFile(kGuest);
  auto other = ReadRiveFile(kGuest);
  auto first = decodedRoot(*file, "Game");
  REQUIRE(first.artboard != nullptr);

  // One of the file's own: shared on purpose, globals still separate.
  auto shared = decodedRootLike(*file, *first.artboard, first.viewModel);
  REQUIRE(shared.artboard != nullptr);
  CHECK(shared.artboard->name() == "Game");
  CHECK(shared.viewModel == first.viewModel);
  CHECK(
      shared.stateMachine->globalViewModelInstance("Theme") !=
      first.stateMachine->globalViewModelInstance("Theme"));

  // Another file's instance gives way to this file's default.
  auto foreign = other->createDefaultViewModelInstance(
      other->viewModel("Game"));
  REQUIRE(foreign != nullptr);
  auto refused = decodedRootLike(*file, *first.artboard, foreign);
  REQUIRE(refused.artboard != nullptr);
  CHECK(refused.viewModel != foreign);
  CHECK(refused.viewModel->viewModel() == file->viewModel("Game"));

  // An instance of another file's artboard has no source in this one.
  auto stranger = decodedRoot(*other, "Game");
  CHECK(
      decodedRootLike(*file, *stranger.artboard, nullptr).artboard == nullptr);
}

TEST_CASE("a decoded root keeps its file alive", "[decoded_root]")
{
  auto file = ReadRiveFile(kGuest);
  auto root = decodedRoot(*file, nullptr);
  REQUIRE(root.artboard != nullptr);
  file = nullptr;
  REQUIRE(root.artboard->file() != nullptr);
  NoOpRenderer renderer;
  for (int frame = 0; frame < 3; frame++)
  {
    root.stateMachine->advanceAndApply(1.0f / 60.0f);
    root.artboard->draw(&renderer);
  }
  CHECK(fillOf(root.artboard.get(), "Target") == kGreen);
}

TEST_CASE(
    "a decoded root fires its finished trigger on its main instance",
    "[decoded_root]")
{
  auto file = ReadRiveFile(kGuest);
  auto root = decodedRoot(*file, nullptr);
  REQUIRE(root.artboard != nullptr);
  auto* finished = root.viewModel->propertyValue("finished")
                       ->as<ViewModelInstanceTrigger>();
  root.stateMachine->advanceAndApply(0.0f);
  uint32_t before = finished->propertyValue();
  root.stateMachine->pointerDown(Vec2D(20.0f, 20.0f));
  root.stateMachine->pointerUp(Vec2D(20.0f, 20.0f));
  root.stateMachine->advanceAndApply(0.016f);
  CHECK(finished->propertyValue() == before);
  root.stateMachine->pointerDown(Vec2D(300.0f, 300.0f));
  root.stateMachine->pointerUp(Vec2D(300.0f, 300.0f));
  root.stateMachine->advanceAndApply(0.016f);
  CHECK(finished->propertyValue() == before + 1);
}

TEST_CASE(
    "a decoded root runs no machine its file does not name as default",
    "[decoded_root]")
{
  auto file = ReadRiveFile(kGuest);
  // As the CLI player and Artboard inputs: no fallback to the first machine.
  file->artboard("Game")->defaultStateMachineId(kEmptyId);
  auto root = decodedRoot(*file, "Game");
  REQUIRE(root.artboard != nullptr);
  CHECK(root.artboard->stateMachineCount() == 1);
  CHECK(root.stateMachine == nullptr);
  REQUIRE(root.viewModel != nullptr);
  REQUIRE(root.artboard->globalViewModelInstance("Theme") != nullptr);
  root.artboard->advance(0.0f);
  CHECK(fillOf(root.artboard.get(), "Target") == kGreen);
}

TEST_CASE(
    "a decoded root of a watermarked file plays from its first frame",
    "[decoded_root]")
{
  struct Deterministic
  {
    bool was = File::deterministicMode;
    Deterministic() { File::deterministicMode = true; }
    ~Deterministic() { File::deterministicMode = was; }
  } deterministic;
  auto file = ReadRiveFile("assets/watermark_playback_test.riv");
  REQUIRE(file->manifest() != nullptr);
  REQUIRE(file->manifest()->hasWatermark());

  auto standalone = file->artboardDefault();
  REQUIRE(standalone->watermark() != nullptr);
  auto machine = standalone->stateMachineAt(0);
  REQUIRE(machine != nullptr);
  machine->advanceAndApply(0.016f);
  REQUIRE(standalone->watermark()->isPlaying());
  CHECK(drawnBy(standalone.get(), false) != drawnBy(standalone.get(), true));

  auto first = decodedRoot(*file, nullptr);
  REQUIRE(first.artboard != nullptr);
  auto copy = decodedRootLike(*file, *first.artboard, nullptr);
  REQUIRE(copy.artboard != nullptr);
  for (auto* root : {&first, &copy})
  {
    CHECK(root->artboard->watermark() == nullptr);
    if (root->stateMachine != nullptr)
    {
      root->stateMachine->advanceAndApply(0.016f);
    }
    else
    {
      root->artboard->advance(0.016f);
    }
    auto own = drawnBy(root->artboard.get(), true);
    REQUIRE(!own.empty());
    CHECK(drawnBy(root->artboard.get(), false) == own);
  }
}

TEST_CASE("decodedRoot matches an artboard's whole name", "[decoded_root]")
{
  auto file = ReadRiveFile(kGuest);
  // A script's string may hold a NUL; the name is all of its bytes.
  const char name[] = "Game\0Other";
  CHECK(decodedRoot(*file, name, sizeof(name) - 1).artboard == nullptr);
  auto game = decodedRoot(*file, name, 4);
  REQUIRE(game.artboard != nullptr);
  CHECK(game.artboard->name() == "Game");
}

TEST_CASE(
    "decodedRootLike decodes none of the artboards before its own",
    "[decoded_root]")
{
  // Deferred bodies decode from these bytes later, so they outlive import.
  auto bytes = ReadFile(kGuest);
  FileImportOptions options;
  options.deferDecoding = true;
  // Game, the default artboard, otherwise decodes at import.
  options.testing_deferDefaultArtboard = true;
  auto file = File::import(
      bytes,
      &gNoOpFactory,
      nullptr,
      (FileAssetLoader*)nullptr,
      nullptr,
      false,
      options);
  REQUIRE(file != nullptr);
  REQUIRE(file->artboardHeader(0)->name() == "Game");
  REQUIRE(file->artboardHeader(0)->hasDeferredBody());

  auto other = decodedRoot(*file, "Other");
  REQUIRE(other.artboard != nullptr);
  auto copy = decodedRootLike(*file, *other.artboard, nullptr);
  REQUIRE(copy.artboard != nullptr);
  CHECK(copy.artboard->name() == "Other");
  CHECK(file->artboardHeader(0)->hasDeferredBody());
}
