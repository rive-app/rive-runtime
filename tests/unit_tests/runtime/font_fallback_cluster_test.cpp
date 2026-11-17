#include "catch.hpp"
#include "rive/text/font_hb.hpp"
#include "rive/text_engine.hpp"
#include <cstdio>
#include <vector>

using namespace rive;

// U+0312 COMBINING TURNED COMMA ABOVE: RobotoFlex, IBM Plex Sans Arabic and
// Inter have no glyph for it, Montserrat does. All four have "a". After "a"
// it forms one cluster, the way a keycap's U+20E3 does after its "1".
static constexpr Unichar kMark = 0x0312;
// U+0313 COMBINING COMMA ABOVE: Inter has it; RobotoFlex and Montserrat
// don't.
static constexpr Unichar kMark2 = 0x0313;

static std::vector<uint8_t> readFile(const char* filename)
{
  FILE* fp = fopen(filename, "rb");
  REQUIRE(fp != nullptr);
  fseek(fp, 0, SEEK_END);
  const size_t length = ftell(fp);
  fseek(fp, 0, SEEK_SET);
  std::vector<uint8_t> bytes(length);
  REQUIRE(fread(bytes.data(), 1, length, fp) == length);
  fclose(fp);
  return bytes;
}

static rcp<Font> loadFont(const char* filename)
{
  auto font = HBFont::Decode(readFile(filename));
  REQUIRE(font != nullptr);
  return font;
}

// What the procs below hand out, and what they were asked for.
static std::vector<rcp<Font>> procFonts;
static std::vector<uint8_t> procFontBytes;
static std::vector<Unichar> procMissing;

// procFonts[fallbackIndex], then nothing.
static rcp<Font> pickByIndex(
    Unichar missing,
    uint32_t fallbackIndex,
    const Font*)
{
  procMissing.push_back(missing);
  return fallbackIndex < procFonts.size() ? procFonts[fallbackIndex] : nullptr;
}

// procFonts[0], procFonts[1], procFonts[0], ... forever.
static rcp<Font> pickAlternating(
    Unichar missing,
    uint32_t fallbackIndex,
    const Font*)
{
  procMissing.push_back(missing);
  return procFonts[fallbackIndex % 2];
}

// The same font, decoded afresh on every call.
static rcp<Font> pickFreshDecode(Unichar missing, uint32_t, const Font*)
{
  procMissing.push_back(missing);
  return HBFont::Decode(procFontBytes);
}

// What the request procs below were handed.
static std::vector<std::vector<Unichar>> procClusters;
static std::vector<Unichar> procRequestMissing;
static std::vector<const Font*> procFrom;

// procFonts[fallbackIndex], then nothing.
static rcp<Font> pickRequestByIndex(const Font::FallbackRequest& request)
{
  procClusters.emplace_back(request.cluster.begin(), request.cluster.end());
  procRequestMissing.push_back(request.missing);
  procFrom.push_back(request.font);
  return request.fallbackIndex < procFonts.size()
             ? procFonts[request.fallbackIndex]
             : nullptr;
}

// The fallbackIndex each coverage proc below was asked with.
static std::vector<uint32_t> procIndices;

// The first of procFonts that has [missing], as a host picks.
static rcp<Font> pickByCoverage(
    Unichar missing,
    uint32_t fallbackIndex,
    const Font* from)
{
  procMissing.push_back(missing);
  procIndices.push_back(fallbackIndex);
  procFrom.push_back(from);
  for (const auto& font : procFonts)
  {
    if (font->hasGlyph(missing))
    {
      return font;
    }
  }
  return nullptr;
}

// The first of procFonts that has every codepoint in the request's cluster,
// as a host picks.
static rcp<Font> pickRequestByCoverage(const Font::FallbackRequest& request)
{
  procClusters.emplace_back(request.cluster.begin(), request.cluster.end());
  for (const auto& font : procFonts)
  {
    bool covers = true;
    for (auto c : request.cluster)
    {
      covers = covers && font->hasGlyph(c);
    }
    if (covers)
    {
      return font;
    }
  }
  return nullptr;
}

// Installs a proc for one test, and clears it and what it saw however the
// test ends.
struct ProcScope
{
  explicit ProcScope(Font::FallbackProc proc) { Font::gFallbackProc = proc; }
  explicit ProcScope(Font::FallbackRequestProc proc)
  {
    Font::gFallbackRequestProc = proc;
  }
  ~ProcScope()
  {
    Font::gFallbackProc = nullptr;
    Font::gFallbackRequestProc = nullptr;
    procFonts.clear();
    procFontBytes.clear();
    procMissing.clear();
    procIndices.clear();
    procClusters.clear();
    procRequestMissing.clear();
    procFrom.clear();
  }
};

static SimpleArray<GlyphRun> shapeRuns(
    const rcp<Font>& font,
    const std::vector<Unichar>& text)
{
  std::vector<TextRun> truns = {
      {font, 32.0f, -1.0f, 0.0f, (uint32_t)text.size(), 0, 0, 0}};
  auto paragraphs = font->shapeText(text, truns);
  REQUIRE(paragraphs.size() == 1);
  return std::move(paragraphs[0].runs);
}

static size_t glyphCount(const SimpleArray<GlyphRun>& runs)
{
  size_t count = 0;
  for (const auto& run : runs)
  {
    count += run.glyphs.size();
  }
  return count;
}

TEST_CASE("a cluster missing a mark falls back whole", "[text_fallback]")
{
  auto roboto = loadFont("assets/RobotoFlex.ttf");
  auto montserrat = loadFont("assets/Montserrat.ttf");
  procFonts = {montserrat};
  ProcScope scope(pickByIndex);

  auto runs = shapeRuns(roboto, {'x', 'a', kMark, 'y'});

  // The proc is asked for the mark, not the "a" its cluster starts with.
  REQUIRE(procMissing == std::vector<Unichar>{kMark});
  REQUIRE(runs.size() == 3);
  CHECK(runs[0].font == roboto);
  CHECK(runs[0].glyphs.size() == 1);
  // Montserrat draws the whole cluster, "a" included, and Roboto draws no
  // "a" of its own next to it.
  CHECK(runs[1].font == montserrat);
  CHECK(runs[1].glyphs.size() == 2);
  CHECK(runs[1].textIndices[0] == 1);
  CHECK(runs[1].textIndices[1] == 1);
  CHECK(runs[2].font == roboto);
  CHECK(runs[2].glyphs.size() == 1);
  CHECK(runs[2].textIndices[0] == 3);
  for (const auto& run : runs)
  {
    for (auto glyph : run.glyphs)
    {
      CHECK(glyph != 0);
    }
  }
}

TEST_CASE("the proc isn't asked for a default ignorable", "[text_fallback]")
{
  auto roboto = loadFont("assets/RobotoFlex.ttf");
  auto montserrat = loadFont("assets/Montserrat.ttf");
  procFonts = {montserrat};
  ProcScope scope(pickByIndex);

  // Roboto has no glyph for either, but nothing draws them.
  Unichar ignorable = 0;
  SECTION("a variation selector") { ignorable = 0xFE0E; }
  SECTION("a combining grapheme joiner") { ignorable = 0x034F; }
  REQUIRE_FALSE(roboto->hasGlyph(ignorable));
  auto runs = shapeRuns(roboto, {'a', ignorable, kMark});

  REQUIRE(procMissing == std::vector<Unichar>{kMark});
  REQUIRE(runs.size() == 1);
  CHECK(runs[0].font == montserrat);
}

TEST_CASE("a fallback chain draws a cluster once", "[text_fallback]")
{
  auto roboto = loadFont("assets/RobotoFlex.ttf");
  auto plex = loadFont("assets/IBMPlexSansArabic-Regular.ttf");
  auto montserrat = loadFont("assets/Montserrat.ttf");
  // Plex has the "a" but not the mark, so the chain goes on to Montserrat.
  procFonts = {plex, montserrat};
  ProcScope scope(pickByIndex);

  auto runs = shapeRuns(roboto, {'a', kMark});

  REQUIRE(procMissing == std::vector<Unichar>{kMark, kMark});
  REQUIRE(runs.size() == 1);
  CHECK(runs[0].font == montserrat);
  CHECK(runs[0].glyphs.size() == 2);
}

TEST_CASE("a cluster no font draws is drawn once", "[text_fallback]")
{
  auto roboto = loadFont("assets/RobotoFlex.ttf");
  auto plex = loadFont("assets/IBMPlexSansArabic-Regular.ttf");
  procFonts = {plex};
  ProcScope scope(pickByIndex);

  auto runs = shapeRuns(roboto, {'a', kMark});

  // The last font tried keeps the cluster: one "a" and its missing mark.
  REQUIRE(runs.size() == 1);
  CHECK(runs[0].font == plex);
  CHECK(runs[0].glyphs.size() == 2);
  CHECK(runs[0].glyphs[0] != 0);
  CHECK(runs[0].glyphs[1] == 0);
}

TEST_CASE(
    "a proc handing back the font itself ends fallback",
    "[text_fallback]")
{
  auto roboto = loadFont("assets/RobotoFlex.ttf");
  procFonts = {roboto};
  ProcScope scope(pickByIndex);

  auto runs = shapeRuns(roboto, {'a', kMark});

  CHECK(procMissing.size() == 1);
  REQUIRE(runs.size() == 1);
  CHECK(runs[0].font == roboto);
  CHECK(runs[0].glyphs.size() == 2);
}

TEST_CASE("a proc cycling between fonts ends fallback", "[text_fallback]")
{
  auto roboto = loadFont("assets/RobotoFlex.ttf");
  auto plex = loadFont("assets/IBMPlexSansArabic-Regular.ttf");
  auto inter = loadFont("assets/fonts/Inter_18pt-Regular.ttf");
  procFonts = {plex, inter};
  ProcScope scope(pickAlternating);

  auto runs = shapeRuns(roboto, {'a', kMark});

  // Roboto -> Plex -> Inter, then Plex again, which it already fell back
  // from.
  CHECK(procMissing.size() == 3);
  REQUIRE(runs.size() == 1);
  CHECK(runs[0].font == inter);
  CHECK(runs[0].glyphs.size() == 2);
}

TEST_CASE("fallback stops after kMaxFallbacks fonts", "[text_fallback]")
{
  auto roboto = loadFont("assets/RobotoFlex.ttf");
  // A new object every call, so no font repeats.
  procFontBytes = readFile("assets/IBMPlexSansArabic-Regular.ttf");
  ProcScope scope(pickFreshDecode);

  auto runs = shapeRuns(roboto, {'a', kMark, 'b'});

  CHECK(procMissing.size() == Font::kMaxFallbacks);
  REQUIRE(runs.size() == 2);
  CHECK(runs[0].glyphs.size() == 2);
  CHECK(runs[0].glyphs[1] == 0);
  CHECK(runs[1].font == roboto);
  CHECK(glyphCount(runs) == 3);
}

TEST_CASE("a fallback request carries the whole cluster", "[text_fallback]")
{
  auto roboto = loadFont("assets/RobotoFlex.ttf");
  auto montserrat = loadFont("assets/Montserrat.ttf");
  procFonts = {montserrat};
  ProcScope pointScope(pickByIndex);
  ProcScope requestScope(pickRequestByIndex);

  auto runs = shapeRuns(roboto, {'x', 'a', 0xFE0E, kMark, 'y'});

  REQUIRE(
      procClusters == std::vector<std::vector<Unichar>>{{'a', 0xFE0E, kMark}});
  CHECK(procRequestMissing == std::vector<Unichar>{kMark});
  CHECK(procFrom == std::vector<const Font*>{roboto.get()});
  // The request proc is preferred over gFallbackProc.
  CHECK(procMissing.empty());
  REQUIRE(runs.size() == 3);
  CHECK(runs[1].font == montserrat);
  CHECK(runs[1].textIndices[0] == 1);
}

TEST_CASE("a keycap reaches the request whole", "[text_fallback]")
{
  auto roboto = loadFont("assets/RobotoFlex.ttf");
  ProcScope scope(pickRequestByIndex);

  shapeRuns(roboto, {'A', '1', 0xFE0F, 0x20E3, 'B'});

  CHECK(
      procClusters == std::vector<std::vector<Unichar>>{{'1', 0xFE0F, 0x20E3}});
  CHECK(procRequestMissing == std::vector<Unichar>{0x20E3});
}

TEST_CASE("a ZWJ sequence reaches the request whole", "[text_fallback]")
{
  auto roboto = loadFont("assets/RobotoFlex.ttf");
  ProcScope scope(pickRequestByIndex);

  // Family: man, woman, girl.
  const std::vector<Unichar> family =
      {0x1F468, 0x200D, 0x1F469, 0x200D, 0x1F467};
  shapeRuns(roboto, family);

  CHECK(procClusters == std::vector<std::vector<Unichar>>{family});
}

TEST_CASE("each font in a chain asks with the cluster", "[text_fallback]")
{
  auto roboto = loadFont("assets/RobotoFlex.ttf");
  auto plex = loadFont("assets/IBMPlexSansArabic-Regular.ttf");
  auto montserrat = loadFont("assets/Montserrat.ttf");
  procFonts = {plex, montserrat};
  ProcScope scope(pickRequestByIndex);

  auto runs = shapeRuns(roboto, {'a', kMark});

  CHECK(
      procClusters ==
      std::vector<std::vector<Unichar>>{{'a', kMark}, {'a', kMark}});
  CHECK(procFrom == std::vector<const Font*>{roboto.get(), plex.get()});
  REQUIRE(runs.size() == 1);
  CHECK(runs[0].font == montserrat);
  CHECK(runs[0].glyphs.size() == 2);
}

TEST_CASE("the emoji presentation check sends the cluster", "[text_fallback]")
{
  auto roboto = loadFont("assets/RobotoFlex.ttf");
  auto emoji = loadFont("assets/TwemojiMozilla.subset.ttf");
  REQUIRE(emoji->hasColorGlyphs());
  procFonts = {emoji};
  ProcScope scope(pickRequestByIndex);

  // Roboto draws "a", but U+FE0F asks for it as an emoji.
  REQUIRE(roboto->hasGlyph('a'));
  auto runs = shapeRuns(roboto, {'a', 0xFE0F, 'b'});

  REQUIRE(procClusters.size() >= 2);
  // Checking for a color font, then falling back to it.
  CHECK(procClusters[0] == std::vector<Unichar>{'a', 0xFE0F});
  CHECK(procClusters[1] == std::vector<Unichar>{'a', 0xFE0F});
  CHECK(procRequestMissing[0] == 'a');
  REQUIRE(runs.size() == 2);
  CHECK(runs[0].font == emoji);
  CHECK(runs[1].font == roboto);
}

TEST_CASE("fallback looks at the run's own font", "[text_fallback]")
{
  auto roboto = loadFont("assets/RobotoFlex.ttf");
  auto montserrat = loadFont("assets/Montserrat.ttf");
  procFonts = {montserrat};
  ProcScope scope(pickByCoverage);

  // Shaped through Montserrat, but the cluster missing a mark is in a
  // Roboto run.
  std::vector<Unichar> text = {'x', 'a', kMark};
  std::vector<TextRun> truns = {
      {montserrat, 32.0f, -1.0f, 0.0f, 1, 0, 0, 0},
      {roboto, 32.0f, -1.0f, 0.0f, 2, 0, 0, 0}};
  auto paragraphs = montserrat->shapeText(text, truns);
  REQUIRE(paragraphs.size() == 1);
  const auto& runs = paragraphs[0].runs;

  CHECK(procMissing == std::vector<Unichar>{kMark});
  CHECK(procFrom == std::vector<const Font*>{roboto.get()});
  // Montserrat isn't the font that failed, so it's a fine fallback.
  REQUIRE(runs.size() == 2);
  CHECK(runs[1].font == montserrat);
  CHECK(runs[1].glyphs.size() == 2);
  CHECK(runs[1].glyphs[1] != 0);
}

TEST_CASE("each missing cluster is asked for on its own", "[text_fallback]")
{
  auto roboto = loadFont("assets/RobotoFlex.ttf");
  auto montserrat = loadFont("assets/Montserrat.ttf");
  auto inter = loadFont("assets/fonts/Inter_18pt-Regular.ttf");
  REQUIRE_FALSE(roboto->hasGlyph(kMark2));
  REQUIRE_FALSE(montserrat->hasGlyph(kMark2));
  REQUIRE(inter->hasGlyph(kMark2));
  procFonts = {montserrat, inter};
  ProcScope scope(pickByCoverage);

  auto runs = shapeRuns(roboto, {'a', kMark, 'x', 'b', kMark2});

  // Both from Roboto, each first in its own chain, rather than the second
  // asked from the font the first picked.
  CHECK(procMissing == std::vector<Unichar>{kMark, kMark2});
  CHECK(procIndices == std::vector<uint32_t>{0, 0});
  CHECK(procFrom == std::vector<const Font*>{roboto.get(), roboto.get()});
  REQUIRE(runs.size() == 3);
  CHECK(runs[0].font == montserrat);
  CHECK(runs[1].font == roboto);
  CHECK(runs[2].font == inter);
  CHECK(runs[2].glyphs.size() == 2);
}

TEST_CASE("a cluster no font draws doesn't stop the next", "[text_fallback]")
{
  auto roboto = loadFont("assets/RobotoFlex.ttf");
  auto inter = loadFont("assets/fonts/Inter_18pt-Regular.ttf");
  procFonts = {inter};
  ProcScope scope(pickRequestByCoverage);

  // Inter has no U+0312, so the first cluster stays missing in Roboto.
  auto runs = shapeRuns(roboto, {'a', kMark, 'x', 'b', kMark2});

  CHECK(
      procClusters ==
      std::vector<std::vector<Unichar>>{{'a', kMark}, {'b', kMark2}});
  REQUIRE(runs.size() == 2);
  CHECK(runs[0].font == roboto);
  CHECK(runs[0].glyphs.size() == 3);
  CHECK(runs[0].glyphs[1] == 0);
  CHECK(runs[1].font == inter);
  CHECK(runs[1].glyphs.size() == 2);
}

TEST_CASE(
    "neighbors given the same font are shaped together",
    "[text_fallback]")
{
  auto roboto = loadFont("assets/RobotoFlex.ttf");
  auto montserrat = loadFont("assets/Montserrat.ttf");
  procFonts = {montserrat};
  ProcScope scope(pickByCoverage);

  auto runs = shapeRuns(roboto, {'a', kMark, 'b', kMark});

  CHECK(procMissing == std::vector<Unichar>{kMark, kMark});
  REQUIRE(runs.size() == 1);
  CHECK(runs[0].font == montserrat);
  CHECK(runs[0].glyphs.size() == 4);
}
