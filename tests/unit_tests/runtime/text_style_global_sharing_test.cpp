#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include "rive/nested_artboard.hpp"
#include "rive/shapes/paint/shape_paint.hpp"
#include "rive/shapes/paint/solid_color.hpp"
#include "rive/text/text.hpp"
#include "rive/text/text_style_global.hpp"
#include "rive/text/text_style_paint.hpp"
#include "rive/text/text_style_share_cache.hpp"
#include "rive/text/text_value_run.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "rive_file_reader.hpp"
#include "utils/no_op_renderer.hpp"
#include "catch.hpp"

using namespace rive;

// text_style_global_sharing.riv (written by rive_core's
// global_text_style_sharing_test.dart). Every style binds its font size to its
// own view model's "size" unless noted:
//   Static (30), Plain (no binds), Inert (22, a default state machine with
//   only Entry/Any/Exit): shareable.
//   Exposed (20, "size" exposed), ReadsHost (font size bound to the host's
//   "Main VM".size), Animated (24, an animation state), WithBackground (26),
//   WithDash (28): per text.
// "Main" (Main VM: size 25, exposedSize 50) has Text A ("A") and Text B ("BB")
// on Static, Text C on ReadsHost, Text D on Exposed (its exposed size bound to
// exposedSize), Text E on Plain, Text F on WithBackground, Text G on Inert,
// Text H on Animated, Text I on WithDash, and a nested "Card" with Card Static
// and Card ReadsHost. "Quiet" has a single Text on Static.
namespace
{
TextStyleGlobal* handleOf(Text* text)
{
  REQUIRE(text != nullptr);
  for (auto child : text->children())
  {
    if (child->is<TextStyleGlobal>())
    {
      return child->as<TextStyleGlobal>();
    }
  }
  return nullptr;
}

TextStyleGlobal* handle(ArtboardInstance* artboard, const std::string& name)
{
  return handleOf(artboard->find<Text>(name));
}

// The font size the first run of [name] shaped with.
float fontSize(ArtboardInstance* artboard, const std::string& name)
{
  auto text = artboard->find<Text>(name);
  REQUIRE(text != nullptr);
  REQUIRE(!text->runs().empty());
  auto paint = text->runs().front()->style();
  REQUIRE(paint != nullptr);
  return paint->fontSize();
}

void frame(ArtboardInstance* artboard)
{
  NoOpRenderer renderer;
  artboard->advance(0.0f);
  artboard->draw(&renderer);
}

struct MainScene
{
  std::unique_ptr<ArtboardInstance> artboard;
  rcp<ViewModelInstance> viewModelInstance;

  MainScene(File* file)
  {
    artboard = file->artboard("Main")->instance();
    viewModelInstance = file->createDefaultViewModelInstance(artboard.get());
    REQUIRE(viewModelInstance != nullptr);
    artboard->bindViewModelInstance(viewModelInstance);
    frame(artboard.get());
  }

  ViewModelInstanceNumber* number(const char* name)
  {
    auto value = viewModelInstance->propertyValue(name);
    REQUIRE(value != nullptr);
    return value->as<ViewModelInstanceNumber>();
  }

  ArtboardInstance* card()
  {
    auto nested = artboard->find<NestedArtboard>("Card");
    REQUIRE(nested != nullptr);
    return nested->artboardInstance();
  }
};
} // namespace

TEST_CASE(
    "Text styles are shared only when nothing can differ per text",
    "[textStyleGlobal]")
{
  auto file = ReadRiveFile("assets/text_style_global_sharing.riv");
  const std::pair<const char*, bool> expected[] = {
      {"Static", true},
      {"Plain", true},
      {"Inert", true},
      {"Exposed", false},
      {"ReadsHost", false},
      {"Animated", false},
      {"WithBackground", false},
      {"WithDash", false},
  };
  for (auto [name, shareable] : expected)
  {
    auto source = file->artboard(name);
    REQUIRE(source != nullptr);
    INFO(name);
    CHECK(
        TextStyleShareCache::analyze(source, file.get()).shareable ==
        shareable);
    CHECK(file->textStyleShares().info(source).shareable == shareable);
  }
  CHECK(TextStyleShareCache::analyze(file->artboard("ReadsHost"), file.get())
            .readsOutsideOwnViewModel);
  CHECK(!TextStyleShareCache::analyze(file->artboard("Exposed"), file.get())
             .readsOutsideOwnViewModel);
}

TEST_CASE(
    "Every text using a shareable style draws one instance of it",
    "[textStyleGlobal]")
{
  auto file = ReadRiveFile("assets/text_style_global_sharing.riv");
  {
    MainScene first(file.get());
    MainScene second(file.get());
    auto a = handle(first.artboard.get(), "Text A");
    auto b = handle(first.artboard.get(), "Text B");
    auto otherA = handle(second.artboard.get(), "Text A");
    auto cardStatic = handle(first.card(), "Card Static");
    REQUIRE(a != nullptr);
    REQUIRE(cardStatic != nullptr);
    CHECK(a->isShared());
    CHECK(a->resolvedPaint() == b->resolvedPaint());
    CHECK(a->resolvedPaint() == otherA->resolvedPaint());
    CHECK(a->resolvedPaint() == cardStatic->resolvedPaint());
    CHECK(a->artboardInstance() == cardStatic->artboardInstance());
    CHECK(handle(first.artboard.get(), "Text E")->isShared());
    CHECK(handle(first.artboard.get(), "Text G")->isShared());

    // Static, Plain and Inert, however many hosts use them.
    CHECK(file->textStyleShares().liveCount() == 3);

    for (auto name : {"Text C", "Text D", "Text F", "Text H", "Text I"})
    {
      INFO(name);
      auto own = handle(first.artboard.get(), name);
      CHECK(!own->isShared());
      CHECK(own->artboardInstance() != nullptr);
      CHECK(
          own->resolvedPaint() !=
          handle(second.artboard.get(), name)->resolvedPaint());
    }

    CHECK(fontSize(first.artboard.get(), "Text A") == 30.0f);
    CHECK(fontSize(first.artboard.get(), "Text G") == 22.0f);
    CHECK(fontSize(first.artboard.get(), "Text H") == 24.0f);
    CHECK(fontSize(first.artboard.get(), "Text F") == 26.0f);
    CHECK(fontSize(first.artboard.get(), "Text I") == 28.0f);
  }
  CHECK(file->textStyleShares().liveCount() == 0);
}

TEST_CASE(
    "Texts sharing a style paint keep their own glyphs",
    "[textStyleGlobal]")
{
  auto file = ReadRiveFile("assets/text_style_global_sharing.riv");
  MainScene scene(file.get());
  auto textA = scene.artboard->find<Text>("Text A");
  auto paint = handleOf(textA)->resolvedPaint();
  REQUIRE(paint != nullptr);
  CHECK(paint->isShared());
  CHECK(paint->localPath()->empty());

  // A translucent text never fades the paint every other text draws.
  REQUIRE(!paint->shapePaints().empty());
  auto shapePaint = paint->shapePaints().front();
  SolidColor* color = nullptr;
  for (auto child : shapePaint->children())
  {
    if (child->is<SolidColor>())
    {
      color = child->as<SolidColor>();
    }
  }
  REQUIRE(color != nullptr);
  const auto colorValue = color->colorValue();
  textA->opacity(0.5f);
  frame(scene.artboard.get());
  CHECK(shapePaint->renderOpacity() == 1.0f);
  CHECK(color->colorValue() == colorValue);
  CHECK(paint->localPath()->empty());
}

TEST_CASE(
    "Text style binds read the hosting artboard's data",
    "[textStyleGlobal]")
{
  auto file = ReadRiveFile("assets/text_style_global_sharing.riv");
  MainScene scene(file.get());
  CHECK(fontSize(scene.artboard.get(), "Text C") == 25.0f);
  CHECK(fontSize(scene.card(), "Card ReadsHost") == 25.0f);
  // The host's exposedSize drives Text D's exposed size.
  CHECK(fontSize(scene.artboard.get(), "Text D") == 50.0f);

  // A host value reshapes the texts reading it in the same frame.
  scene.number("size")->propertyValue(35.0f);
  scene.number("exposedSize")->propertyValue(60.0f);
  frame(scene.artboard.get());
  CHECK(fontSize(scene.artboard.get(), "Text C") == 35.0f);
  CHECK(fontSize(scene.card(), "Card ReadsHost") == 35.0f);
  CHECK(fontSize(scene.artboard.get(), "Text D") == 60.0f);

  // Another host keeps its own values.
  MainScene other(file.get());
  CHECK(fontSize(other.artboard.get(), "Text C") == 25.0f);
  CHECK(fontSize(other.artboard.get(), "Text D") == 50.0f);

  // Bound to another instance: the styles follow it.
  auto instance = file->createDefaultViewModelInstance(scene.artboard.get());
  instance->propertyValue("size")->as<ViewModelInstanceNumber>()->propertyValue(
      18.0f);
  scene.artboard->bindViewModelInstance(instance);
  frame(scene.artboard.get());
  CHECK(fontSize(scene.artboard.get(), "Text C") == 18.0f);
  CHECK(fontSize(scene.card(), "Card ReadsHost") == 18.0f);
}

TEST_CASE(
    "An artboard drawing only shared styles has nothing to advance",
    "[textStyleGlobal]")
{
  auto file = ReadRiveFile("assets/text_style_global_sharing.riv");
  REQUIRE(file->artboard("Quiet") != nullptr);
  auto artboard = file->artboard("Quiet")->instance();
  frame(artboard.get());
  auto shared = handle(artboard.get(), "Quiet Static");
  REQUIRE(shared != nullptr);
  CHECK(shared->isShared());
  CHECK(shared->quietState() == AdvancingComponent::QuietState::quiet);
  CHECK(!shared->advanceComponent(0.016f, AdvanceFlags::AdvanceNested));
  CHECK(fontSize(artboard.get(), "Quiet Static") == 30.0f);
}

TEST_CASE("Text style instances tear down in any order", "[textStyleGlobal]")
{
  auto file = ReadRiveFile("assets/text_style_global_sharing.riv");
  auto first = std::make_unique<MainScene>(file.get());
  auto second = std::make_unique<MainScene>(file.get());
  REQUIRE(file->artboard("Quiet") != nullptr);
  auto quiet = file->artboard("Quiet")->instance();
  frame(quiet.get());
  CHECK(file->textStyleShares().liveCount() == 3);

  // The first host to use a shared style goes first.
  first.reset();
  frame(second->artboard.get());
  CHECK(file->textStyleShares().liveCount() == 3);
  CHECK(fontSize(second->artboard.get(), "Text A") == 30.0f);
  second.reset();
  CHECK(file->textStyleShares().liveCount() == 1);
  frame(quiet.get());
  quiet.reset();
  CHECK(file->textStyleShares().liveCount() == 0);
}

// A style's metrics changing on its own (a keyframe sets the value; no other
// dirt follows) re-shapes the texts drawing it, shared or per text.
TEST_CASE(
    "A style's metrics re-shape the texts drawing it",
    "[textStyleGlobal]")
{
  auto file = ReadRiveFile("assets/text_style_global_sharing.riv");
  MainScene scene(file.get());
  // Shared (Plain) and per text (Animated: a state machine).
  for (auto name : {"Text E", "Text H"})
  {
    auto text = scene.artboard->find<Text>(name);
    REQUIRE(text != nullptr);
    REQUIRE(!text->runs().empty());
    auto paint = text->runs().front()->style();
    REQUIRE(paint != nullptr);
    float width = text->localBounds().width();
    REQUIRE(width > 0.0f);

    paint->fontSize(paint->fontSize() * 2.0f);
    frame(scene.artboard.get());
    CHECK(text->localBounds().width() > width);

    width = text->localBounds().width();
    paint->letterSpacing(paint->letterSpacing() + 10.0f);
    frame(scene.artboard.get());
    CHECK(text->localBounds().width() > width);

    float height = text->localBounds().height();
    paint->lineHeight(paint->fontSize() * 3.0f);
    frame(scene.artboard.get());
    CHECK(text->localBounds().height() > height);
  }
}

// An instance the File makes holds a reference to it, released before the
// artboard's components (and the leases they hold on shared styles) are
// deleted. Holding the last reference, the artboard outlives its File.
TEST_CASE(
    "An artboard can outlive its file's shared text styles",
    "[textStyleGlobal]")
{
  auto file = ReadRiveFile("assets/text_style_global_sharing.riv");
  auto artboard = file->artboardNamed("Main");
  REQUIRE(artboard != nullptr);
  auto viewModelInstance = file->createDefaultViewModelInstance(artboard.get());
  REQUIRE(viewModelInstance != nullptr);
  artboard->bindViewModelInstance(viewModelInstance);
  frame(artboard.get());
  // Shared by the main artboard's texts and its nested Card's.
  REQUIRE(file->textStyleShares().liveCount() > 0);

  file = nullptr;
  artboard = nullptr;
  viewModelInstance = nullptr;
}
