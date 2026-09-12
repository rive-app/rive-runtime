/*
 * Copyright 2026 Rive
 */

#include <rive/simple_array.hpp>
#include <catch.hpp>
#include <rive/text_engine.hpp>
#include <rive/text/font_hb.hpp>
#include "rive/text/utf.hpp"
#include <utility>
#include <vector>

using namespace rive;

using Words = std::vector<std::pair<uint32_t, uint32_t>>;

static rcp<Font> loadFont()
{
    FILE* fp = fopen("assets/RobotoFlex.ttf", "rb");
    REQUIRE(fp != nullptr);
    fseek(fp, 0, SEEK_END);
    const size_t length = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    std::vector<uint8_t> bytes(length);
    REQUIRE(fread(bytes.data(), 1, length, fp) == length);
    fclose(fp);
    return HBFont::Decode(bytes);
}

// MSVC rejects \u escapes in narrow literals, so tests pass u8 literals.
#define U8(literal) reinterpret_cast<const char*>(u8##literal)

// Shapes utf8 as one styled run and returns each paragraph's (start, end)
// word pairs with paragraph wide glyph indices. Runs split by script, and the
// pair stream is flat across them, so they are merged back here.
static std::vector<Words> shapeWords(const char* utf8)
{
    auto font = loadFont();
    REQUIRE(font != nullptr);
    std::vector<Unichar> unichars;
    const uint8_t* ptr = (const uint8_t*)utf8;
    while (*ptr)
    {
        unichars.push_back(UTF::NextUTF8(&ptr));
    }
    TextRun run = {font, 32.0f, -1.0f, 0.0f, (uint32_t)unichars.size(), 0};
    auto paragraphs = font->shapeText(unichars, Span<const TextRun>(&run, 1));
    std::vector<Words> result;
    for (const Paragraph& paragraph : paragraphs)
    {
        std::vector<uint32_t> stream;
        uint32_t base = 0;
        for (const GlyphRun& gr : paragraph.runs)
        {
            for (uint32_t index : gr.breaks)
            {
                stream.push_back(base + index);
            }
            base += (uint32_t)gr.glyphs.size();
        }
        Words words;
        for (size_t i = 0; i + 1 < stream.size(); i += 2)
        {
            words.push_back({stream[i], stream[i + 1]});
        }
        result.push_back(words);
    }
    return result;
}

TEST_CASE("line breaker allows breaks after hyphens, dashes and slashes",
          "[line break]")
{
    auto words = shapeWords(U8("state-of-the-art x\u2014y a/b"));
    REQUIRE(words.size() == 1);
    Words expected = {
        {0, 6},   // state-
        {6, 9},   // of-
        {9, 13},  // the-
        {13, 16}, // art
        {17, 18}, // x
        {18, 19}, // the em dash breaks on both sides
        {19, 20}, // y
        {21, 23}, // a/
        {23, 24}, // b
    };
    REQUIRE(words[0] == expected);
}

TEST_CASE("line breaker keeps no-break space, soft hyphen and word joiner "
          "glued",
          "[line break]")
{
    auto words = shapeWords(U8("100\u00A0km su\u00ADper 5\u2060x"));
    REQUIRE(words.size() == 1);
    Words expected = {{0, 6}, {7, 13}, {14, 17}};
    REQUIRE(words[0] == expected);
}

TEST_CASE("line breaker breaks CJK per character with kinsoku", "[line break]")
{
    // Glyphs are .notdef in this font, the break structure only needs text.
    // 日本、「語」。x, an ideographic space, y
    auto words =
        shapeWords(U8("\u65E5\u672C\u3001\u300C\u8A9E\u300D\u3002x\u3000y"));
    REQUIRE(words.size() == 1);
    Words expected = {
        {0, 1},  // 日
        {1, 3},  // 本、
        {3, 7},  // 「語」。
        {7, 8},  // x, the ideographic space after it is trimmed
        {9, 10}, // y
    };
    REQUIRE(words[0] == expected);
}

TEST_CASE("line breaker treats every hard break as a forced break",
          "[line break]")
{
    // SheenBidi splits paragraphs at CR LF, NEL and PS.
    auto words = shapeWords(U8("a\r\nb\u0085c\u2029d"));
    REQUIRE(words.size() == 4);
    REQUIRE(words[0] == Words{{0, 1}, {2, 2}});
    REQUIRE(words[1] == Words{{0, 1}, {1, 1}});
    REQUIRE(words[2] == Words{{0, 1}, {1, 1}});
    REQUIRE(words[3] == Words{{0, 1}});

    // VT and LS stay inside a paragraph and still force a break.
    words = shapeWords(U8("d e\vf\u2028g"));
    REQUIRE(words.size() == 1);
    REQUIRE(words[0] == Words{{0, 1}, {2, 3}, {3, 3}, {4, 5}, {5, 5}, {6, 7}});
}

TEST_CASE("line breaker never splits a glyph cluster when a word overflows",
          "[line break]")
{
    // Six glyphs in four clusters: [0] [1 2] [3] [4 5], 10 units each.
    GlyphRun run(6);
    uint32_t textIndices[] = {0, 1, 1, 2, 3, 3};
    for (uint32_t i = 0; i < 6; i++)
    {
        run.glyphs[i] = (GlyphID)(i + 1);
        run.textIndices[i] = textIndices[i];
        run.advances[i] = 10.0f;
        run.xpos[i] = 10.0f * i;
    }
    run.xpos[6] = 60.0f;
    uint32_t breaks[] = {0, 6};
    run.breaks = SimpleArray<uint32_t>(breaks, 2);
    SimpleArray<GlyphRun> runs(1);
    runs[0] = std::move(run);

    auto lines = GlyphLine::BreakLines(runs, 25.0f);
    Words expected = {{0, 1}, {1, 3}, {3, 4}, {4, 6}};
    Words actual;
    for (auto& line : lines)
    {
        actual.push_back({line.startGlyphIndex, line.endGlyphIndex});
    }
    REQUIRE(actual == expected);
}
