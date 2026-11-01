#include "rive/artboard.hpp"
#include "rive/artboard_component_list.hpp"
#include "rive/file.hpp"
#include "rive/text/font_hb.hpp"
#include "rive/text/text.hpp"
#include "rive/text/text_style.hpp"
#include "rive/text/text_value_run.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <atomic>
#include <set>
#include <thread>
#include <vector>

using namespace rive;

static rcp<Font> loadFont(const char* filename)
{
  FILE* fp = fopen(filename, "rb");
  REQUIRE(fp != nullptr);
  fseek(fp, 0, SEEK_END);
  const size_t length = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  std::vector<uint8_t> bytes(length);
  REQUIRE(fread(bytes.data(), 1, length, fp) == length);
  fclose(fp);
  return HBFont::Decode(bytes);
}

static constexpr uint32_t kWght = 2003265652;
static constexpr uint32_t kWdth = 2003072104;
static constexpr uint32_t kLiga = 1818847073;
static constexpr uint32_t kSlnt = 1936486004;

TEST_CASE("Fonts made with the same options are one font", "[text]")
{
  auto font = loadFont("assets/RobotoFlex.ttf");
  const uint64_t made = HBFont::sm_variantsMade.load();

  Font::Coord bold = {kWght, 700.0f};
  auto first = font->sharedAtCoords(Span<Font::Coord>(&bold, 1));
  auto second = font->sharedAtCoords(Span<Font::Coord>(&bold, 1));
  CHECK(first.get() == second.get());
  CHECK(HBFont::sm_variantsMade.load() == made + 1);

  // The same options, reached in another order, are the same font.
  Font::Coord both[] = {{kWght, 700.0f}, {kWdth, 120.0f}};
  Font::Coord bothSwapped[] = {{kWdth, 120.0f}, {kWght, 700.0f}};
  auto wide = font->sharedAtCoords(Span<Font::Coord>(both, 2));
  CHECK(
      font->sharedAtCoords(Span<Font::Coord>(bothSwapped, 2)).get() ==
      wide.get());
  CHECK(wide.get() != first.get());

  // A zero axis value is the same option whatever its sign.
  Font::Coord upright = {kSlnt, 0.0f};
  Font::Coord negativeZero = {kSlnt, -0.0f};
  auto plain = font->sharedAtCoords(Span<Font::Coord>(&upright, 1));
  CHECK(
      font->sharedAtCoords(Span<Font::Coord>(&negativeZero, 1)).get() ==
      plain.get());

  // Other values, or a feature on top, are other fonts.
  Font::Coord light = {kWght, 300.0f};
  CHECK(
      font->sharedAtCoords(Span<Font::Coord>(&light, 1)).get() != first.get());
  Font::Feature noLigatures = {kLiga, 0};
  auto featured = font->sharedWithOptions(
      Span<Font::Coord>(&bold, 1),
      Span<Font::Feature>(&noLigatures, 1));
  CHECK(featured.get() != first.get());
  CHECK(featured->getFeatureValue(kLiga) == 0);
  CHECK(featured->getAxisValue(kWght) == 700.0f);

  // A variant of a variant keeps the first variant's options, and is shared
  // the same way.
  Font::Coord narrow = {kWdth, 80.0f};
  auto boldNarrow = first->sharedAtCoords(Span<Font::Coord>(&narrow, 1));
  CHECK(boldNarrow->getAxisValue(kWght) == 700.0f);
  CHECK(boldNarrow->getAxisValue(kWdth) == 80.0f);
  CHECK(
      first->sharedAtCoords(Span<Font::Coord>(&narrow, 1)).get() ==
      boldNarrow.get());
}

TEST_CASE("A variant nobody holds is made again, not kept", "[text]")
{
  auto font = loadFont("assets/RobotoFlex.ttf");
  Font::Coord bold = {kWght, 700.0f};
  const uint64_t made = HBFont::sm_variantsMade.load();
  {
    auto variant = font->sharedAtCoords(Span<Font::Coord>(&bold, 1));
    REQUIRE(variant != nullptr);
  }
  auto again = font->sharedAtCoords(Span<Font::Coord>(&bold, 1));
  CHECK(HBFont::sm_variantsMade.load() == made + 2);
  CHECK(again->getAxisValue(kWght) == 700.0f);

  // A variant keeps the font it was made from alive, and leaves its map
  // when it goes, after the caller dropped that font.
  font = nullptr;
  CHECK(again->getAxisValue(kWght) == 700.0f);
  again = nullptr;
}

// A variant's outline and color layer caches aren't locked, so a variant is
// only shared on the thread that made it: one held on a thread is not handed
// to a caller on another, which gets its own font, as every caller used to.
TEST_CASE("A variant is shared only on the thread that made it", "[text]")
{
  auto font = loadFont("assets/RobotoFlex.ttf");
  Font::Coord bold = {kWght, 700.0f};
  auto here = font->sharedAtCoords(Span<Font::Coord>(&bold, 1));
  // Compared on the other thread, while both of its fonts are alive; Catch
  // checks stay on this one.
  bool otherThreadGotItsOwn = false;
  bool otherThreadShared = false;
  std::thread([&]() {
    auto made = font->sharedAtCoords(Span<Font::Coord>(&bold, 1));
    auto again = font->sharedAtCoords(Span<Font::Coord>(&bold, 1));
    otherThreadGotItsOwn = made.get() != here.get();
    otherThreadShared = again.get() == made.get();
  }).join();
  CHECK(otherThreadGotItsOwn);
  CHECK(otherThreadShared);
  CHECK(font->sharedAtCoords(Span<Font::Coord>(&bold, 1)).get() == here.get());
}

// A font from withOptions is its caller's, who may hand it to another thread
// and ask again for the same options while that thread still uses it. So no
// later request gets it back, shared or not, and withOptions never gives out
// a shared one.
TEST_CASE("A font from withOptions is never handed out again", "[text]")
{
  auto font = loadFont("assets/RobotoFlex.ttf");
  Font::Coord bold = {kWght, 700.0f};
  const uint64_t made = HBFont::sm_variantsMade.load();
  auto handedOff = font->makeAtCoords(Span<Font::Coord>(&bold, 1));
  auto again = font->makeAtCoords(Span<Font::Coord>(&bold, 1));
  CHECK(again.get() != handedOff.get());
  auto shared = font->sharedAtCoords(Span<Font::Coord>(&bold, 1));
  CHECK(shared.get() != handedOff.get());
  CHECK(shared.get() != again.get());
  CHECK(font->makeAtCoords(Span<Font::Coord>(&bold, 1)).get() != shared.get());
  CHECK(HBFont::sm_variantsMade.load() == made + 4);
  CHECK(
      font->sharedAtCoords(Span<Font::Coord>(&bold, 1)).get() == shared.get());
}

// A variation modifier's falloff keeps a variant alive per coverage level,
// thousands of the same font on a long text. Each request compares against
// the variants with its own key, not against every live one.
TEST_CASE("Thousands of live variants are looked up by their options", "[text]")
{
  auto font = loadFont("assets/RobotoFlex.ttf");
  constexpr int kCount = 2048;
  std::vector<rcp<Font>> variants;
  const uint64_t compared = HBFont::sm_variantsCompared.load();
  const uint64_t made = HBFont::sm_variantsMade.load();
  for (int i = 0; i < kCount; i++)
  {
    Font::Coord coord = {kWght, 100.0f + i * 0.25f};
    variants.push_back(font->sharedAtCoords(Span<Font::Coord>(&coord, 1)));
  }
  CHECK(HBFont::sm_variantsMade.load() == made + kCount);
  // None of them can share, and none was compared with the others.
  CHECK(HBFont::sm_variantsCompared.load() - compared < kCount / 16);

  bool allShared = true;
  for (int i = 0; i < kCount; i++)
  {
    Font::Coord coord = {kWght, 100.0f + i * 0.25f};
    auto again = font->sharedAtCoords(Span<Font::Coord>(&coord, 1));
    allShared = allShared && again.get() == variants[i].get();
  }
  CHECK(allShared);
  CHECK(HBFont::sm_variantsMade.load() == made + kCount);
  CHECK(HBFont::sm_variantsCompared.load() - compared < kCount + kCount / 16);
}

// A thread's id can be given to a thread started after it ends. A variant
// made on a thread that has ended, still held by what asked for it, must not
// be handed to a later thread as well.
TEST_CASE("A variant outlives its thread without being shared again", "[text]")
{
  auto font = loadFont("assets/RobotoFlex.ttf");
  Font::Coord bold = {kWght, 700.0f};
  rcp<Font> handedOff;
  std::thread([&]() {
    handedOff = font->sharedAtCoords(Span<Font::Coord>(&bold, 1));
  }).join();
  for (int i = 0; i < 8; i++)
  {
    bool gotItsOwn = false;
    std::thread([&]() {
      auto variant = font->sharedAtCoords(Span<Font::Coord>(&bold, 1));
      gotItsOwn = variant.get() != handedOff.get();
    }).join();
    CHECK(gotItsOwn);
  }
}

// A variant can drop to zero on another thread (a host may destroy a file on
// a thread other than the one that advanced it) while the thread that made it
// asks for the same options. The lookup then meets it dying, still in the
// map, and must make a new font rather than revive it (RefCnt::tryRef). The
// other thread's destructor is held at that point.
static std::atomic<int> s_dyingStage{0};
TEST_CASE(
    "A variant released on another thread while its maker asks again",
    "[text]")
{
  auto font = loadFont("assets/RobotoFlex.ttf");
  Font::Coord bold = {kWght, 700.0f};
  rcp<Font> handedOff = font->sharedAtCoords(Span<Font::Coord>(&bold, 1));
  const Font* dying = handedOff.get();
  const uint64_t made = HBFont::sm_variantsMade.load();

  s_dyingStage.store(0);
  HBFont::sm_variantDying = []() {
    s_dyingStage.store(1);
    while (s_dyingStage.load() != 2)
    {
    }
  };
  std::thread releaser([&]() { handedOff = nullptr; });
  while (s_dyingStage.load() != 1)
  {
  }
  // The variant's count is zero and its destructor waits, still listed.
  auto again = font->sharedAtCoords(Span<Font::Coord>(&bold, 1));
  const bool madeAnother = again.get() != dying;
  s_dyingStage.store(2);
  releaser.join();
  HBFont::sm_variantDying = nullptr;

  CHECK(madeAnother);
  CHECK(HBFont::sm_variantsMade.load() == made + 1);
  CHECK(again->getAxisValue(kWght) == 700.0f);
}

// A font can be reached from several threads (one font can serve files that
// advance on different threads, and a host may destroy a file on another), so
// variants can be asked for and dropped on several at once.
TEST_CASE("Variants asked for and dropped on several threads", "[text]")
{
  auto font = loadFont("assets/RobotoFlex.ttf");
  std::vector<std::thread> threads;
  for (int t = 0; t < 4; t++)
  {
    threads.emplace_back([&font, t]() {
      for (int i = 0; i < 2000; i++)
      {
        Font::Coord coord = {kWght, (float)(100 + 100 * ((i + t) % 3))};
        auto variant = font->sharedAtCoords(Span<Font::Coord>(&coord, 1));
        if (variant->getAxisValue(kWght) != coord.value)
        {
          abort();
        }
      }
    });
  }
  for (auto& thread : threads)
  {
    thread.join();
  }
}

// component_list_1.riv's rows are an "Item" artboard whose text style sets
// two axes: every row used to build its own variable font and draw every
// glyph outline again.
TEST_CASE(
    "A list's rows share their styles' variable font and its outlines",
    "[text]")
{
  auto file = ReadRiveFile("assets/component_list_1.riv");
  auto artboard = file->artboard("Main")->instance();
  REQUIRE(artboard != nullptr);
  auto viewModelInstance = file->createDefaultViewModelInstance(artboard.get());
  REQUIRE(viewModelInstance != nullptr);
  artboard->bindViewModelInstance(viewModelInstance);
  auto list = artboard->find<ArtboardComponentList>("List");
  REQUIRE(list != nullptr);

  const uint64_t made = HBFont::sm_variantsMade.load();
  const uint64_t drawn = HBFont::sm_glyphPathsDrawn.load();
  artboard->advance(0.0f);
  REQUIRE(list->artboardCount() == 8);

  std::set<const Font*> fonts;
  for (int i = 0; i < (int)list->artboardCount(); i++)
  {
    for (auto style : list->artboardInstance(i)->find<TextStyle>())
    {
      fonts.insert(style->font().get());
    }
  }
  CHECK(fonts.size() == 1);
  CHECK(HBFont::sm_variantsMade.load() == made + 1);

  // Each distinct glyph drawn at most once, not once per row.
  std::set<GlyphID> glyphs;
  for (int i = 0; i < (int)list->artboardCount(); i++)
  {
    for (auto text : list->artboardInstance(i)->find<Text>())
    {
      for (const Paragraph& paragraph : text->shape())
      {
        for (const GlyphRun& run : paragraph.runs)
        {
          for (GlyphID glyph : run.glyphs)
          {
            REQUIRE(fonts.count(run.font.get()) == 1);
            glyphs.insert(glyph);
          }
        }
      }
    }
  }
  CHECK(HBFont::sm_glyphPathsDrawn.load() - drawn <= glyphs.size());
}

// text_variation_modifier_hug.riv's modifier group sets its text's width
// axis, so its glyphs are drawn in a variant the modifier asks the style's
// font for. A second instance of the artboard draws in the variant the first
// one made. (Styling may also make a variant it lets go of before the text is
// drawn, which nothing holds for the second instance to share.)
TEST_CASE("Instances of a variation-modified text share its variant", "[text]")
{
  auto file = ReadRiveFile("assets/text_variation_modifier_hug.riv");
  auto fontsDrawn = [](Artboard* artboard) {
    std::set<const Font*> fonts;
    for (auto text : artboard->find<Text>())
    {
      for (const auto& line : text->orderedLines())
      {
        for (auto [glyphRun, glyphIndex] : line)
        {
          fonts.insert(glyphRun->font.get());
        }
      }
    }
    return fonts;
  };
  auto first = file->artboardDefault();
  REQUIRE(first != nullptr);
  first->advance(0.0f);
  auto fonts = fontsDrawn(first.get());
  REQUIRE(fonts.size() == 1);
  for (auto style : first->find<TextStyle>())
  {
    CHECK(fonts.count(style->font().get()) == 0);
  }

  auto second = file->artboardDefault();
  second->advance(0.0f);
  CHECK(fontsDrawn(second.get()) == fonts);
}
