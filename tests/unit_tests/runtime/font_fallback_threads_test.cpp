#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include "rive/text/font_hb.hpp"
#include "rive/text/text.hpp"
#include "rive/text/text_style.hpp"
#include "rive/text/text_value_run.hpp"
#include "rive/text/utf.hpp"
#include "rive_file_reader.hpp"
#include "hb.h"
#include <catch.hpp>
#include <algorithm>
#include <atomic>
#include <set>
#include <string>
#include <thread>
#include <vector>

using namespace rive;

static std::vector<uint8_t> readBytes(const char* filename)
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
  return HBFont::Decode(readBytes(filename));
}

// Hosts hand every thread the same fallback fonts: Android keeps the ones
// its proc returns for the life of the process, and each command queue
// shapes and draws text with them on a thread of its own.
static rcp<Font> s_fallback;
static rcp<Font> pickSharedFallback(
    const Unichar,
    const uint32_t fallbackIndex,
    const Font*)
{
  return fallbackIndex == 0 ? s_fallback : nullptr;
}

// Makes [font] every thread's fallback font while in scope.
class SharedFallback
{
public:
  SharedFallback(rcp<Font> font) : m_previous(Font::gFallbackProc)
  {
    s_fallback = std::move(font);
    Font::gFallbackProc = pickSharedFallback;
  }
  ~SharedFallback()
  {
    Font::gFallbackProc = m_previous;
    s_fallback = nullptr;
  }

private:
  Font::FallbackProc m_previous;
};

static std::string toUTF8(const std::vector<Unichar>& text)
{
  std::string utf8;
  uint8_t bytes[4];
  for (Unichar unichar : text)
  {
    utf8.append((const char*)bytes, UTF::Encode(bytes, unichar));
  }
  return utf8;
}

static SimpleArray<Paragraph> shapeWith(
    const rcp<Font>& font,
    const std::vector<Unichar>& text)
{
  TextRun run = {font, 32.0f, -1.0f, 0.0f, (uint32_t)text.size(), 0, 0};
  return font->shapeText(
      Span<const Unichar>(text.data(), text.size()),
      Span<const TextRun>(&run, 1));
}

// The one font [shape]'s runs draw with, or null if they draw with several.
static rcp<Font> drawnWith(const SimpleArray<Paragraph>& shape)
{
  rcp<Font> font;
  for (const Paragraph& paragraph : shape)
  {
    for (const GlyphRun& run : paragraph.runs)
    {
      if (font != nullptr && font != run.font)
      {
        return nullptr;
      }
      font = run.font;
    }
  }
  return font;
}

static std::vector<GlyphID> glyphsOf(const SimpleArray<Paragraph>& shape)
{
  std::vector<GlyphID> glyphs;
  for (const Paragraph& paragraph : shape)
  {
    for (const GlyphRun& run : paragraph.runs)
    {
      glyphs.insert(glyphs.end(), run.glyphs.begin(), run.glyphs.end());
    }
  }
  return glyphs;
}

// The characters [fallback] has that [font] shapes to missing glyphs alone,
// so all of them fall back. Lacking one isn't enough: a default ignorable,
// such as a joiner, takes [font]'s space glyph.
static std::vector<Unichar> fallbackOnly(
    const rcp<Font>& font,
    const Font* fallback)
{
  SharedFallback none(nullptr);
  std::vector<Unichar> text;
  for (Unichar unichar : fallback->codepoints())
  {
    std::vector<GlyphID> glyphs = glyphsOf(shapeWith(font, {unichar}));
    if (!glyphs.empty() &&
        std::all_of(glyphs.begin(), glyphs.end(), [](GlyphID glyph) {
          return glyph == 0;
        }))
    {
      text.push_back(unichar);
    }
  }
  return text;
}

static std::vector<float> advancesOf(const SimpleArray<Paragraph>& shape)
{
  std::vector<float> advances;
  for (const Paragraph& paragraph : shape)
  {
    for (const GlyphRun& run : paragraph.runs)
    {
      advances.insert(advances.end(), run.advances.begin(), run.advances.end());
    }
  }
  return advances;
}

static bool sameMetrics(const Font* a, const Font* b)
{
  const Font::LineMetrics& x = a->lineMetrics();
  const Font::LineMetrics& y = b->lineMetrics();
  return x.ascent == y.ascent && x.descent == y.descent &&
         x.capHeight == y.capHeight && x.xHeight == y.xHeight;
}

// RIVE CmdServer crashes on Android: two command queues advanced files on
// their own threads, and texts in both fell back to the same font. A font's
// outline cache isn't locked, so both threads drew into one, and a lookup
// on one read buckets the other had just freed (HBFont::getPath, from
// Text::buildRenderStyles). Here each thread advances its own file, and
// switches its text between two strings of more outlines than a font
// caches (512), so the cache keeps starting over. Drawing one font from
// every thread raced under TSan, and crashed in the cache's lookup.
TEST_CASE(
    "Texts on several threads draw a shared fallback font",
    "[text_fallback]")
{
  constexpr int kThreads = 4;
  std::vector<rcp<File>> files;
  std::vector<std::unique_ptr<ArtboardInstance>> artboards;
  for (int i = 0; i < kThreads; i++)
  {
    files.push_back(ReadRiveFile("assets/hello_world.riv"));
    artboards.push_back(files.back()->artboardDefault());
    REQUIRE(artboards.back() != nullptr);
  }
  rcp<Font> fileFont = artboards[0]->find<TextStyle>()[0]->font();
  REQUIRE(fileFont != nullptr);
  auto fallback = loadFont("assets/IBMPlexSansArabic-Regular.ttf");
  std::vector<Unichar> text = fallbackOnly(fileFont, fallback.get());
  const std::string forward = toUTF8(text);
  std::reverse(text.begin(), text.end());
  const std::string backward = toUTF8(text);

  SharedFallback shared(fallback);
  std::vector<std::thread> threads;
  for (auto& artboard : artboards)
  {
    ArtboardInstance* instance = artboard.get();
    threads.emplace_back([instance, &forward, &backward]() {
      TextValueRun* run = instance->find<TextValueRun>()[0];
      for (int i = 0; i < 16; i++)
      {
        run->text(i % 2 == 0 ? forward : backward);
        instance->advance(0.0f);
      }
    });
  }
  for (auto& thread : threads)
  {
    thread.join();
  }

  for (auto& artboard : artboards)
  {
    const SimpleArray<Paragraph>& shape = artboard->find<Text>()[0]->shape();
    rcp<Font> font = drawnWith(shape);
    CHECK(font != nullptr);
    CHECK(font != fileFont);
    std::vector<GlyphID> glyphs = glyphsOf(shape);
    CHECK(std::set<GlyphID>(glyphs.begin(), glyphs.end()).size() > 512);
  }
}

// The first thread to shape with a fallback font keeps it, so a host that
// shapes on one thread draws the fonts its proc returns, as it always has.
// Another thread's runs get a copy of their own, which its shapes share
// while one holds it, and which draws and measures as the font does.
TEST_CASE(
    "A fallback font is copied for the threads after its first",
    "[text_fallback]")
{
  auto font = loadFont("assets/RobotoFlex.ttf");
  auto fallback = loadFont("assets/IBMPlexSansArabic-Regular.ttf");
  std::vector<Unichar> text = fallbackOnly(font, fallback.get());
  REQUIRE(!text.empty());
  SharedFallback shared(fallback);

  SimpleArray<Paragraph> here = shapeWith(font, text);
  CHECK(drawnWith(here) == fallback);

  // Compared on the other thread while its shapes are alive; Catch checks
  // stay on this one.
  SimpleArray<Paragraph> there;
  bool sharedOnItsThread = false;
  std::thread([&]() {
    there = shapeWith(font, text);
    SimpleArray<Paragraph> again = shapeWith(font, text);
    sharedOnItsThread = drawnWith(again) == drawnWith(there);
  }).join();
  rcp<Font> copy = drawnWith(there);
  REQUIRE(copy != nullptr);
  CHECK(copy != fallback);
  CHECK(sharedOnItsThread);
  CHECK(drawnWith(shapeWith(font, text)) == fallback);

  // That thread is done with the copy, so this one can compare it.
  std::vector<GlyphID> glyphs = glyphsOf(there);
  CHECK(glyphs == glyphsOf(here));
  CHECK(advancesOf(there) == advancesOf(here));
  bool sameOutlines = true;
  for (GlyphID glyph : glyphs)
  {
    sameOutlines = sameOutlines &&
                   copy->getPath(glyph) == fallback->getPath(glyph);
  }
  CHECK(sameOutlines);
  CHECK(sameMetrics(copy.get(), fallback.get()));
}

// Sets its weight on its HarfBuzz font after HBFont measured it, and draws
// its own outlines, as CoreTextHBFont does for the system's fonts.
class SelfDrawnFont : public HBFont
{
public:
  SelfDrawnFont(hb_font_t* font, float weight) : HBFont(font)
  {
    hb_variation_t variation = {HB_TAG('w', 'g', 'h', 't'), weight};
    hb_font_set_variations(font, &variation, 1);
  }

  RawPath drawGlyphPath(GlyphID glyph) const override
  {
    drawn++;
    return HBFont::drawGlyphPath(glyph);
  }

  mutable std::atomic<uint32_t> drawn{0};
};

static rcp<SelfDrawnFont> loadSelfDrawn(const char* filename, float weight)
{
  std::vector<uint8_t> bytes = readBytes(filename);
  hb_blob_t* blob = hb_blob_create(
      (const char*)bytes.data(),
      (unsigned)bytes.size(),
      HB_MEMORY_MODE_DUPLICATE,
      nullptr,
      nullptr);
  hb_face_t* face = hb_face_create(blob, 0);
  hb_blob_destroy(blob);
  hb_font_t* font = hb_font_create(face);
  hb_face_destroy(face);
  return make_rcp<SelfDrawnFont>(font, weight);
}

// A copy draws its outlines through the font it copies, so a subclass's way
// of drawing them carries over, and starts from that font's coordinates,
// whether or not its options list them: it shapes (as an ellipsis does
// with a run's font) at the weight the subclass set, not at the default.
TEST_CASE(
    "A fallback font's copy draws through it at its coordinates",
    "[text_fallback]")
{
  auto font = loadFont("assets/IBMPlexSansArabic-Regular.ttf");
  rcp<SelfDrawnFont> heavy = loadSelfDrawn("assets/RobotoFlex.ttf", 900.0f);
  std::vector<Unichar> text = fallbackOnly(font, heavy.get());
  REQUIRE(!text.empty());
  SharedFallback shared(heavy);

  CHECK(drawnWith(shapeWith(font, text)) == heavy);
  SimpleArray<Paragraph> there;
  std::thread([&]() { there = shapeWith(font, text); }).join();
  rcp<Font> copy = drawnWith(there);
  REQUIRE(copy != nullptr);
  REQUIRE(copy != heavy);

  std::vector<GlyphID> glyphs = glyphsOf(there);
  std::set<GlyphID> distinct(glyphs.begin(), glyphs.end());
  const uint32_t drawn = heavy->drawn.load();
  for (GlyphID glyph : distinct)
  {
    copy->getPath(glyph);
  }
  CHECK(heavy->drawn.load() - drawn == distinct.size());
  bool sameOutlines = true;
  for (GlyphID glyph : distinct)
  {
    sameOutlines = sameOutlines &&
                   copy->getPath(glyph) == heavy->getPath(glyph);
  }
  CHECK(sameOutlines);
  CHECK(sameMetrics(copy.get(), heavy.get()));

  std::vector<Unichar> latin;
  for (const char* c = "Wide bold words"; *c != 0; c++)
  {
    latin.push_back((Unichar)*c);
  }
  std::vector<float> heavyAdvances = advancesOf(shapeWith(heavy, latin));
  CHECK(advancesOf(shapeWith(copy, latin)) == heavyAdvances);
  CHECK(
      advancesOf(shapeWith(loadFont("assets/RobotoFlex.ttf"), latin)) !=
      heavyAdvances);
}
