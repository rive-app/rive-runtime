/*
 * Copyright 2022 Rive
 */

#include <rive/simple_array.hpp>
#include <catch.hpp>
#include <rive/text_engine.hpp>
#include <rive/text/font_hb.hpp>
#include "rive/text/utf.hpp"

using namespace rive;

static rive::TextRun append(std::vector<rive::Unichar>* unichars,
                            rive::rcp<rive::Font> font,
                            float size,
                            const char text[])
{
    const uint8_t* ptr = (const uint8_t*)text;
    uint32_t n = 0;
    while (*ptr)
    {
        unichars->push_back(rive::UTF::NextUTF8(&ptr));
        n += 1;
    }
    return {std::move(font), size, -1.0f, 0.0f, n, 0};
}

static rcp<Font> loadFont(const char* filename)
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

TEST_CASE("line breaker separates words", "[line break]")
{
    auto font = loadFont("assets/RobotoFlex.ttf");
    REQUIRE(font != nullptr);

    // one two⏎ three
    std::vector<rive::TextRun> truns;
    std::vector<rive::Unichar> unichars;
    truns.push_back(append(&unichars, font, 32.0f, "one two three"));

    auto paragraphs = font->shapeText(unichars, truns);
    REQUIRE(paragraphs.size() == 1);
    const auto& paragraph = paragraphs.front();
    REQUIRE(paragraph.runs.size() == 1);
    const auto& run = paragraph.runs.front();
    REQUIRE(run.breaks.size() == 6);
    REQUIRE(run.breaks[0] == 0);
    REQUIRE(run.breaks[1] == 3);
    REQUIRE(run.breaks[2] == 4);
    REQUIRE(run.breaks[3] == 7);
    REQUIRE(run.breaks[4] == 8);
    REQUIRE(run.breaks[5] == 13);
}

TEST_CASE("line breaker handles multiple runs", "[line break]")
{
    auto font = loadFont("assets/RobotoFlex.ttf");
    REQUIRE(font != nullptr);

    std::vector<rive::TextRun> truns;
    std::vector<rive::Unichar> unichars;
    truns.push_back(append(&unichars, font, 32.0f, "one two thr"));
    truns.push_back(append(&unichars, font, 60.0f, "ee four"));

    auto paragraphs = font->shapeText(unichars, truns);
    REQUIRE(paragraphs.size() == 1);
    const auto& paragraph = paragraphs.front();

    REQUIRE(paragraph.runs.size() == 2);
    {
        const auto& run = paragraph.runs.front();
        REQUIRE(run.breaks.size() == 5);
        REQUIRE(run.breaks[0] == 0);
        REQUIRE(run.breaks[1] == 3);
        REQUIRE(run.breaks[2] == 4);
        REQUIRE(run.breaks[3] == 7);
        REQUIRE(run.breaks[4] == 8);
    }
    {
        const auto& run = paragraph.runs.back();
        REQUIRE(run.breaks.size() == 3);
        REQUIRE(run.breaks[0] == 2);
        REQUIRE(run.breaks[1] == 3);
        REQUIRE(run.breaks[2] == 7);
    }
}

TEST_CASE("line breaker handles returns", "[line break]")
{
    auto font = loadFont("assets/RobotoFlex.ttf");
    REQUIRE(font != nullptr);

    std::vector<rive::TextRun> truns;
    std::vector<rive::Unichar> unichars;
    truns.push_back(append(&unichars, font, 32.0f, "one two thr"));
    truns.push_back(append(&unichars,
                           font,
                           60.0f,
                           reinterpret_cast<const char*>(u8"ee\u2028 four")));

    auto paragraphs = font->shapeText(unichars, truns);
    const auto& paragraph = paragraphs.front();
    REQUIRE(paragraph.runs.size() == 2);
    {
        const auto& run = paragraph.runs.front();
        REQUIRE(run.breaks.size() == 5);
        REQUIRE(run.breaks[0] == 0);
        REQUIRE(run.breaks[1] == 3);
        REQUIRE(run.breaks[2] == 4);
        REQUIRE(run.breaks[3] == 7);
        REQUIRE(run.breaks[4] == 8);
    }
    {
        const auto& run = paragraph.runs.back();
        REQUIRE(run.breaks.size() == 5);
        REQUIRE(run.breaks[0] == 2);
        REQUIRE(run.breaks[1] == 2);
        REQUIRE(run.breaks[2] == 2);
        REQUIRE(run.breaks[3] == 4);
        REQUIRE(run.breaks[4] == 8);
    }
}

TEST_CASE("line breaker builds lines", "[line break]")
{
    auto font = loadFont("assets/RobotoFlex.ttf");
    REQUIRE(font != nullptr);

    // one two⏎ three
    std::vector<rive::TextRun> truns;
    std::vector<rive::Unichar> unichars;
    truns.push_back(append(&unichars, font, 32.0f, "one two three"));

    auto paragraphs = font->shapeText(unichars, truns);
    REQUIRE(paragraphs.size() == 1);
    const auto& paragraph = paragraphs.front();
    REQUIRE(paragraph.runs.size() == 1);

    // at 194 everything fits in one line
    {
        auto lines = GlyphLine::BreakLines(paragraph.runs, 194.0f);
        REQUIRE(lines.size() == 1);
        auto line = lines.back();
        REQUIRE(line.startRunIndex == 0);
        REQUIRE(line.startGlyphIndex == 0);
        REQUIRE(line.endRunIndex == 0);
        REQUIRE(line.endGlyphIndex == 13);
    }
    // at 191 "three" should pop to second line
    {
        auto lines = GlyphLine::BreakLines(paragraph.runs, 191.0f);
        REQUIRE(lines.size() == 2);
        {
            auto line = lines.front();
            REQUIRE(line.startRunIndex == 0);
            REQUIRE(line.startGlyphIndex == 0);
            REQUIRE(line.endRunIndex == 0);
            REQUIRE(line.endGlyphIndex == 7);
        }

        {
            auto line = lines.back();
            REQUIRE(line.startRunIndex == 0);
            REQUIRE(line.startGlyphIndex == 8);
            REQUIRE(line.endRunIndex == 0);
            REQUIRE(line.endGlyphIndex == 13);
        }
    }
}

TEST_CASE("line breaker deals with extremes", "[line break]")
{
    auto font = loadFont("assets/RobotoFlex.ttf");
    REQUIRE(font != nullptr);

    // one two⏎ three
    std::vector<rive::TextRun> truns;
    std::vector<rive::Unichar> unichars;
    truns.push_back(append(&unichars, font, 32.0f, "ab"));

    auto paragraphs = font->shapeText(unichars, truns);
    REQUIRE(paragraphs.size() == 1);
    const auto& paragraph = paragraphs.front();
    REQUIRE(paragraph.runs.size() == 1);

    {
        auto lines = GlyphLine::BreakLines(paragraph.runs, 17.0f);
        REQUIRE(lines.size() == 2);
        {
            auto line = lines.front();
            REQUIRE(line.startRunIndex == 0);
            REQUIRE(line.startGlyphIndex == 0);
            REQUIRE(line.endRunIndex == 0);
            REQUIRE(line.endGlyphIndex == 1);
        }

        {
            auto line = lines.back();
            REQUIRE(line.startRunIndex == 0);
            REQUIRE(line.startGlyphIndex == 1);
            REQUIRE(line.endRunIndex == 0);
            REQUIRE(line.endGlyphIndex == 2);
        }
    }
    // Test that it also handles 0 width.
    {
        auto lines = GlyphLine::BreakLines(paragraph.runs, 0.0f);
        REQUIRE(lines.size() == 2);
        {
            auto line = lines.front();
            REQUIRE(line.startRunIndex == 0);
            REQUIRE(line.startGlyphIndex == 0);
            REQUIRE(line.endRunIndex == 0);
            REQUIRE(line.endGlyphIndex == 1);
        }

        {
            auto line = lines.back();
            REQUIRE(line.startRunIndex == 0);
            REQUIRE(line.startGlyphIndex == 1);
            REQUIRE(line.endRunIndex == 0);
            REQUIRE(line.endGlyphIndex == 2);
        }
    }
}

TEST_CASE("line breaker breaks return characters", "[line break]")
{
    auto font = loadFont("assets/RobotoFlex.ttf");
    REQUIRE(font != nullptr);

    // one two⏎ three
    std::vector<rive::TextRun> truns;
    std::vector<rive::Unichar> unichars;
    truns.push_back(
        append(&unichars,
               font,
               32.0f,
               reinterpret_cast<const char*>(u8"hello look\u2028here")));

    auto paragraphs = font->shapeText(unichars, truns);
    REQUIRE(paragraphs.size() == 1);
    const auto& paragraph = paragraphs.front();
    REQUIRE(paragraph.runs.size() == 1);
    {
        auto lines = GlyphLine::BreakLines(paragraph.runs, 300.0f);
        REQUIRE(lines.size() == 2);
    }
}

TEST_CASE("shaper separates paragraphs", "[shaper]")
{
    auto font = loadFont("assets/RobotoFlex.ttf");
    REQUIRE(font != nullptr);

    // one two⏎ three
    std::vector<rive::TextRun> truns;
    std::vector<rive::Unichar> unichars;
    truns.push_back(append(&unichars,
                           font,
                           32.0f,
                           reinterpret_cast<const char*>(
                               u8"hello look\u2028here\nsecond paragraph")));

    auto paragraphs = font->shapeText(unichars, truns);
    REQUIRE(paragraphs.size() == 2);
    {
        const auto& paragraph = paragraphs.front();
        REQUIRE(paragraph.runs.size() == 1);
        REQUIRE(paragraph.baseDirection() == rive::TextDirection::ltr);

        auto lines = GlyphLine::BreakLines(paragraph.runs, 300.0f);
        REQUIRE(lines.size() == 2);
    }
    {
        const auto& paragraph = paragraphs.back();
        REQUIRE(paragraph.runs.size() == 1);
        REQUIRE(paragraph.baseDirection() == rive::TextDirection::ltr);

        auto lines = GlyphLine::BreakLines(paragraph.runs, 300.0f);
        REQUIRE(lines.size() == 1);
    }
}

TEST_CASE("shaper handles RTL", "[shaper]")
{
    auto font = loadFont("assets/IBMPlexSansArabic-Regular.ttf");
    REQUIRE(font != nullptr);

    // one two⏎ three
    std::vector<rive::TextRun> truns;
    std::vector<rive::Unichar> unichars;
    truns.push_back(append(&unichars, font, 32.0f, "لمفاتيح ABC DEF"));

    auto paragraphs = font->shapeText(unichars, truns);
    REQUIRE(paragraphs.size() == 1);
    const auto& paragraph = paragraphs.front();
    REQUIRE(paragraph.baseDirection() == rive::TextDirection::rtl);
    {
        auto lines = GlyphLine::BreakLines(paragraph.runs, 300.0f);
        REQUIRE(lines.size() == 1);
    }
    {
        auto lines = GlyphLine::BreakLines(paragraph.runs, 196.0f);
        REQUIRE(lines.size() == 2);

        // The second line should start with DEF as it's the first word to wrap.
        const auto& line = lines.back();
        const auto& run = paragraph.runs[line.startRunIndex];
        auto index = run.textIndices[line.startGlyphIndex];
        REQUIRE(unichars[index] == 'D');
        REQUIRE(unichars[index + 1] == 'E');
        REQUIRE(unichars[index + 2] == 'F');
    }
}

TEST_CASE("shaper handles empty space", "[shaper]")
{
    auto font = loadFont("assets/IBMPlexSansArabic-Regular.ttf");
    REQUIRE(font != nullptr);

    std::vector<rive::TextRun> truns;
    std::vector<rive::Unichar> unichars;
    truns.push_back(append(&unichars, font, 32.0f, " "));

    auto paragraphs = font->shapeText(unichars, truns);
    REQUIRE(paragraphs.size() == 1);
    const auto& paragraph = paragraphs.front();
    REQUIRE(paragraph.baseDirection() == rive::TextDirection::ltr);
    {
        auto lines = GlyphLine::BreakLines(paragraph.runs, 300.0f);
        REQUIRE(lines.size() == 1);
    }
}

TEST_CASE("line breaker deals with empty paragraphs", "[line break]")
{
    auto font = loadFont("assets/IBMPlexSansArabic-Regular.ttf");
    REQUIRE(font != nullptr);

    std::vector<rive::TextRun> truns;
    std::vector<rive::Unichar> unichars;
    truns.push_back(append(&unichars, font, 32.0f, "hi\n "));

    auto paragraphs = font->shapeText(unichars, truns);
    REQUIRE(paragraphs.size() == 2);
    {
        const auto& paragraph = paragraphs.front();
        REQUIRE(paragraph.baseDirection() == rive::TextDirection::ltr);
        {
            auto lines = GlyphLine::BreakLines(paragraph.runs, -1.0f);
            REQUIRE(lines.size() == 1);
        }
    }
    {
        const auto& paragraph = paragraphs.back();
        REQUIRE(paragraph.baseDirection() == rive::TextDirection::ltr);
        {
            auto lines = GlyphLine::BreakLines(paragraph.runs, -1.0f);
            REQUIRE(lines.size() == 1);
            auto line = lines.front();
            REQUIRE(line.startRunIndex == 0);
            REQUIRE(paragraph.runs[line.startRunIndex].glyphs.size() == 1);
            REQUIRE(paragraph.runs[line.startRunIndex].textIndices.size() == 1);
            REQUIRE(paragraph.runs[line.startRunIndex].textIndices[0] == 3);
        }
    }
}

TEST_CASE("line breaker deals with space only lines", "[line break]")
{
    auto font = loadFont("assets/IBMPlexSansArabic-Regular.ttf");
    REQUIRE(font != nullptr);

    std::vector<rive::TextRun> truns;
    std::vector<rive::Unichar> unichars;
    truns.push_back(append(&unichars,
                           font,
                           32.0f,
                           reinterpret_cast<const char*>(u8"hi\u2028 ")));

    auto paragraphs = font->shapeText(unichars, truns);
    REQUIRE(paragraphs.size() == 1);
    {
        const auto& paragraph = paragraphs.front();
        REQUIRE(paragraph.baseDirection() == rive::TextDirection::ltr);
        {
            auto lines = GlyphLine::BreakLines(paragraph.runs, -1.0f);
            REQUIRE(lines.size() == 2);
        }
    }
}

TEST_CASE("line breaker deals with empty lines", "[line break]")
{
    auto font = loadFont("assets/IBMPlexSansArabic-Regular.ttf");
    REQUIRE(font != nullptr);

    std::vector<rive::TextRun> truns;
    std::vector<rive::Unichar> unichars;
    truns.push_back(append(&unichars, font, 32.0f, "hi\n"));

    auto paragraphs = font->shapeText(unichars, truns);
    REQUIRE(paragraphs.size() == 1);
    {
        const auto& paragraph = paragraphs.front();
        REQUIRE(paragraph.baseDirection() == rive::TextDirection::ltr);
        {
            auto lines = GlyphLine::BreakLines(paragraph.runs, -1.0f);
            REQUIRE(lines.size() == 1);
            {
                auto line = lines.front();
                REQUIRE(line.startRunIndex == 0);
                REQUIRE(line.startGlyphIndex == 0);
                REQUIRE(paragraph.runs[line.startRunIndex].glyphs.size() == 3);
                REQUIRE(paragraph.runs[line.startRunIndex].textIndices.size() ==
                        3);
                REQUIRE(paragraph.runs[line.startRunIndex].textIndices[0] == 0);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// TextWordBreak. breakWord is the default and is covered by every case above;
// these pin the two new modes and the boundaries between them.
// ---------------------------------------------------------------------------

// Shapes a single 32pt run and hands back the paragraph's runs.
static SimpleArray<Paragraph> shapeOneRun(const rcp<Font>& font,
                                          std::vector<Unichar>* unichars,
                                          std::vector<TextRun>* truns,
                                          const char text[])
{
    truns->push_back(append(unichars, font, 32.0f, text));
    return font->shapeText(*unichars, *truns);
}

TEST_CASE("word break normal never splits a word", "[line break]")
{
    auto font = loadFont("assets/RobotoFlex.ttf");
    REQUIRE(font != nullptr);

    std::vector<rive::TextRun> truns;
    std::vector<rive::Unichar> unichars;
    auto paragraphs = shapeOneRun(font, &unichars, &truns, "ab");
    REQUIRE(paragraphs.size() == 1);
    const auto& runs = paragraphs.front().runs;

    // "line breaker deals with extremes" pins breakWord splitting this into
    // two lines at these widths. normal has to keep it whole and overflow
    // instead -- and, crucially, still terminate: routing this through the
    // knock-to-a-new-line branch would spin forever.
    for (float width : {17.0f, 1.0f, 0.0f})
    {
        auto lines = GlyphLine::BreakLines(runs, width, TextWordBreak::normal);
        REQUIRE(lines.size() == 1);
        REQUIRE(lines[0].startRunIndex == 0);
        REQUIRE(lines[0].startGlyphIndex == 0);
        REQUIRE(lines[0].endRunIndex == 0);
        REQUIRE(lines[0].endGlyphIndex == 2);
    }
}

TEST_CASE("word break normal still wraps between words", "[line break]")
{
    auto font = loadFont("assets/RobotoFlex.ttf");
    REQUIRE(font != nullptr);

    std::vector<rive::TextRun> truns;
    std::vector<rive::Unichar> unichars;
    auto paragraphs = shapeOneRun(font, &unichars, &truns, "one two three");
    REQUIRE(paragraphs.size() == 1);
    const auto& runs = paragraphs.front().runs;
    const auto& run = runs.front();

    // Wide enough for "one" but not for "three", so every word lands on its
    // own line and only the last one overflows.
    float width = (run.xpos[3] + run.xpos[4]) / 2.0f;

    auto lines = GlyphLine::BreakLines(runs, width, TextWordBreak::normal);
    REQUIRE(lines.size() == 3);
    REQUIRE(lines[0].startGlyphIndex == 0);
    REQUIRE(lines[0].endGlyphIndex == 3);
    REQUIRE(lines[1].startGlyphIndex == 4);
    REQUIRE(lines[1].endGlyphIndex == 7);
    // "three" is kept whole even though it doesn't fit.
    REQUIRE(lines[2].startGlyphIndex == 8);
    REQUIRE(lines[2].endGlyphIndex == 13);

    // breakWord would chop that last word up instead.
    auto broken = GlyphLine::BreakLines(runs, width, TextWordBreak::breakWord);
    REQUIRE(broken.size() > lines.size());
}

TEST_CASE("word break all fills the line it is already on", "[line break]")
{
    auto font = loadFont("assets/RobotoFlex.ttf");
    REQUIRE(font != nullptr);

    std::vector<rive::TextRun> truns;
    std::vector<rive::Unichar> unichars;
    auto paragraphs = shapeOneRun(font, &unichars, &truns, "one two three");
    REQUIRE(paragraphs.size() == 1);
    const auto& runs = paragraphs.front().runs;
    const auto& run = runs.front();

    // Room for "one two t" but not "one two th".
    float width = (run.xpos[9] + run.xpos[10]) / 2.0f;

    auto lines = GlyphLine::BreakLines(runs, width, TextWordBreak::breakAll);
    REQUIRE(lines.size() >= 2);
    // The first line is packed into the middle of "three" rather than ending
    // after "two".
    REQUIRE(lines[0].startGlyphIndex == 0);
    REQUIRE(lines[0].endGlyphIndex == 9);
    REQUIRE(lines[1].startGlyphIndex == 9);

    // breakWord leaves "three" intact on the next line at this width.
    auto broken = GlyphLine::BreakLines(runs, width, TextWordBreak::breakWord);
    REQUIRE(broken[0].endGlyphIndex == 7);
}

TEST_CASE("word break all never cuts inside the space before a word",
          "[line break]")
{
    auto font = loadFont("assets/RobotoFlex.ttf");
    REQUIRE(font != nullptr);

    std::vector<rive::TextRun> truns;
    std::vector<rive::Unichar> unichars;
    auto paragraphs = shapeOneRun(font, &unichars, &truns, "one two three");
    REQUIRE(paragraphs.size() == 1);
    const auto& runs = paragraphs.front().runs;
    const auto& run = runs.front();

    // Glyph 7 is the space before "three", which starts at glyph 8. Both of
    // these widths land the "last position that fits" inside that space. The
    // cut has to be rejected: line 0 must end after "two" with the space
    // excluded (a trailing space would inflate the width ComputeMaxWidth and
    // ComputeLineSpacing derive from the line), and line 1 must start at the
    // word, not at the space.
    for (float width : {(run.xpos[8] + run.xpos[9]) / 2.0f,
                        (run.xpos[7] + run.xpos[8]) / 2.0f})
    {
        auto lines =
            GlyphLine::BreakLines(runs, width, TextWordBreak::breakAll);
        REQUIRE(lines.size() >= 2);
        REQUIRE(lines[0].endGlyphIndex == 7);
        REQUIRE(lines[1].startGlyphIndex == 8);
    }
}

TEST_CASE("word break all matches break word for a lone word", "[line break]")
{
    auto font = loadFont("assets/RobotoFlex.ttf");
    REQUIRE(font != nullptr);

    std::vector<rive::TextRun> truns;
    std::vector<rive::Unichar> unichars;
    auto paragraphs = shapeOneRun(font, &unichars, &truns, "ab");
    REQUIRE(paragraphs.size() == 1);
    const auto& runs = paragraphs.front().runs;

    // Nothing precedes the word, so both modes take the same path -- including
    // at width 0, where the minimum-one-cluster guard has to keep it moving.
    for (float width : {17.0f, 1.0f, 0.0f})
    {
        auto all = GlyphLine::BreakLines(runs, width, TextWordBreak::breakAll);
        auto word =
            GlyphLine::BreakLines(runs, width, TextWordBreak::breakWord);
        REQUIRE(all.size() == word.size());
        for (size_t i = 0; i < all.size(); i++)
        {
            REQUIRE(all[i] == word[i]);
        }
    }
}

TEST_CASE("word break all honors word joiners", "[line break]")
{
    auto font = loadFont("assets/RobotoFlex.ttf");
    REQUIRE(font != nullptr);

    std::vector<rive::TextRun> truns;
    std::vector<rive::Unichar> unichars;
    // U+2060 word joiner between "abc" and "def".
    auto paragraphs = shapeOneRun(font,
                                  &unichars,
                                  &truns,
                                  "xx abc\xE2\x81\xA0"
                                  "def");
    REQUIRE(paragraphs.size() == 1);
    const auto& runs = paragraphs.front().runs;
    const auto& run = runs.front();

    // Sweep widths that all land inside the joined "abc⁠def" run. No
    // line may start or end at the joiner itself.
    for (uint32_t i = 3; i < run.glyphs.size(); i++)
    {
        auto lines = GlyphLine::BreakLines(runs,
                                           (run.xpos[i] + run.xpos[i + 1]) / 2,
                                           TextWordBreak::breakAll);
        for (const auto& line : lines)
        {
            for (uint32_t joiner : run.joiners)
            {
                if (line.startGlyphIndex < run.textIndices.size())
                {
                    REQUIRE(run.textIndices[line.startGlyphIndex] != joiner);
                }
                if (line.endGlyphIndex < run.textIndices.size())
                {
                    REQUIRE(run.textIndices[line.endGlyphIndex] != joiner);
                }
            }
        }
    }
}

TEST_CASE("word break modes hold the line invariants", "[line break]")
{
    auto font = loadFont("assets/RobotoFlex.ttf");
    REQUIRE(font != nullptr);

    const char* samples[] = {
        "one two three",
        "  hello world  ",
        "supercalifragilisticexpialidocious",
        "foo-bar-baz",
        "ab\xE2\x81\xA0"
        "cd ef",
        "auto\xC2\xAD"
        "mobile",
        "a",
        " ",
    };
    const float widths[] =
        {-1.f, 0.f, 1.f, 5.f, 17.f, 50.f, 97.f, 191.f, 194.f, 1000.f};
    const TextWordBreak modes[] = {TextWordBreak::breakWord,
                                   TextWordBreak::normal,
                                   TextWordBreak::breakAll};

    for (const char* sample : samples)
    {
        std::vector<rive::TextRun> truns;
        std::vector<rive::Unichar> unichars;
        auto paragraphs = shapeOneRun(font, &unichars, &truns, sample);
        for (const auto& paragraph : paragraphs)
        {
            size_t totalGlyphs = 0;
            for (const auto& run : paragraph.runs)
            {
                totalGlyphs += run.glyphs.size();
            }
            for (float width : widths)
            {
                for (TextWordBreak mode : modes)
                {
                    // Terminates, and can't emit more lines than there are
                    // places to break.
                    auto lines =
                        GlyphLine::BreakLines(paragraph.runs, width, mode);
                    REQUIRE(lines.size() <= totalGlyphs + 2);

                    const GlyphLine* previous = nullptr;
                    for (const auto& line : lines)
                    {
                        // Never inverted.
                        REQUIRE(line.endRunIndex >= line.startRunIndex);
                        const auto& startRun =
                            paragraph.runs[line.startRunIndex];
                        const auto& endRun = paragraph.runs[line.endRunIndex];
                        REQUIRE(line.startGlyphIndex <= startRun.xpos.size());
                        REQUIRE(line.endGlyphIndex <= endRun.xpos.size());
                        float from = startRun.xpos[line.startGlyphIndex];
                        float to = endRun.xpos[line.endGlyphIndex];
                        REQUIRE(to >= from);
                        // Lines never overlap and never run backwards.
                        if (previous != nullptr)
                        {
                            REQUIRE(line.startRunIndex >=
                                    previous->endRunIndex);
                            if (line.startRunIndex == previous->endRunIndex)
                            {
                                REQUIRE(line.startGlyphIndex >=
                                        previous->endGlyphIndex);
                            }
                        }
                        // Everything but normal has to respect the box.
                        if (width >= 0.0f && mode != TextWordBreak::normal &&
                            line.endGlyphIndex > line.startGlyphIndex + 1)
                        {
                            REQUIRE(to - from <= width);
                        }
                        previous = &line;
                    }
                }
            }
        }
    }
}

TEST_CASE("word break modes agree when there is nothing to break",
          "[line break]")
{
    auto font = loadFont("assets/RobotoFlex.ttf");
    REQUIRE(font != nullptr);

    std::vector<rive::TextRun> truns;
    std::vector<rive::Unichar> unichars;
    auto paragraphs = shapeOneRun(font, &unichars, &truns, "one two three");
    REQUIRE(paragraphs.size() == 1);
    const auto& runs = paragraphs.front().runs;

    // Autowidth (which is also how TextWrap::noWrap is implemented) never
    // overflows, so the mode is inert.
    for (float width : {-1.0f, 1000.0f})
    {
        auto word =
            GlyphLine::BreakLines(runs, width, TextWordBreak::breakWord);
        for (TextWordBreak mode :
             {TextWordBreak::normal, TextWordBreak::breakAll})
        {
            auto other = GlyphLine::BreakLines(runs, width, mode);
            REQUIRE(other.size() == word.size());
            for (size_t i = 0; i < word.size(); i++)
            {
                REQUIRE(other[i] == word[i]);
            }
        }
    }
}
