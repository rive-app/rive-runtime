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
    FILE* fp = fopen("../../test/assets/RobotoFlex.ttf", "rb");
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
    auto font = loadFont("../../test/assets/RobotoFlex.ttf");
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
    auto font = loadFont("../../test/assets/RobotoFlex.ttf");
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
    auto font = loadFont("../../test/assets/RobotoFlex.ttf");
    REQUIRE(font != nullptr);

    std::vector<rive::TextRun> truns;
    std::vector<rive::Unichar> unichars;
    truns.push_back(append(&unichars, font, 32.0f, "one two thr"));
    truns.push_back(append(&unichars, font, 60.0f, u8"ee\u2028 four"));

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
    auto font = loadFont("../../test/assets/RobotoFlex.ttf");
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
    auto font = loadFont("../../test/assets/RobotoFlex.ttf");
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
    auto font = loadFont("../../test/assets/RobotoFlex.ttf");
    REQUIRE(font != nullptr);

    // one two⏎ three
    std::vector<rive::TextRun> truns;
    std::vector<rive::Unichar> unichars;
    truns.push_back(append(&unichars, font, 32.0f, u8"hello look\u2028here"));

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
    auto font = loadFont("../../test/assets/RobotoFlex.ttf");
    REQUIRE(font != nullptr);

    // one two⏎ three
    std::vector<rive::TextRun> truns;
    std::vector<rive::Unichar> unichars;
    truns.push_back(append(&unichars, font, 32.0f, u8"hello look\u2028here\nsecond paragraph"));

    auto paragraphs = font->shapeText(unichars, truns);
    REQUIRE(paragraphs.size() == 2);
    {
        const auto& paragraph = paragraphs.front();
        REQUIRE(paragraph.runs.size() == 1);
        REQUIRE(paragraph.baseDirection == rive::TextDirection::ltr);

        auto lines = GlyphLine::BreakLines(paragraph.runs, 300.0f);
        REQUIRE(lines.size() == 2);
    }
    {
        const auto& paragraph = paragraphs.back();
        REQUIRE(paragraph.runs.size() == 1);
        REQUIRE(paragraph.baseDirection == rive::TextDirection::ltr);

        auto lines = GlyphLine::BreakLines(paragraph.runs, 300.0f);
        REQUIRE(lines.size() == 1);
    }
}

TEST_CASE("shaper handles RTL", "[shaper]")
{
    auto font = loadFont("../../test/assets/IBMPlexSansArabic-Regular.ttf");
    REQUIRE(font != nullptr);

    // one two⏎ three
    std::vector<rive::TextRun> truns;
    std::vector<rive::Unichar> unichars;
    truns.push_back(append(&unichars, font, 32.0f, "لمفاتيح ABC DEF"));

    auto paragraphs = font->shapeText(unichars, truns);
    REQUIRE(paragraphs.size() == 1);
    const auto& paragraph = paragraphs.front();
    REQUIRE(paragraph.baseDirection == rive::TextDirection::rtl);
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
    auto font = loadFont("../../test/assets/IBMPlexSansArabic-Regular.ttf");
    REQUIRE(font != nullptr);

    std::vector<rive::TextRun> truns;
    std::vector<rive::Unichar> unichars;
    truns.push_back(append(&unichars, font, 32.0f, " "));

    auto paragraphs = font->shapeText(unichars, truns);
    REQUIRE(paragraphs.size() == 1);
    const auto& paragraph = paragraphs.front();
    REQUIRE(paragraph.baseDirection == rive::TextDirection::ltr);
    {
        auto lines = GlyphLine::BreakLines(paragraph.runs, 300.0f);
        REQUIRE(lines.size() == 1);
    }
}

TEST_CASE("line breaker deals with empty paragraphs", "[line break]")
{
    auto font = loadFont("../../test/assets/IBMPlexSansArabic-Regular.ttf");
    REQUIRE(font != nullptr);

    std::vector<rive::TextRun> truns;
    std::vector<rive::Unichar> unichars;
    truns.push_back(append(&unichars, font, 32.0f, "hi\n "));

    auto paragraphs = font->shapeText(unichars, truns);
    REQUIRE(paragraphs.size() == 2);
    {
        const auto& paragraph = paragraphs.front();
        REQUIRE(paragraph.baseDirection == rive::TextDirection::ltr);
        {
            auto lines = GlyphLine::BreakLines(paragraph.runs, -1.0f);
            REQUIRE(lines.size() == 1);
        }
    }
    {
        const auto& paragraph = paragraphs.back();
        REQUIRE(paragraph.baseDirection == rive::TextDirection::ltr);
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
    auto font = loadFont("../../test/assets/IBMPlexSansArabic-Regular.ttf");
    REQUIRE(font != nullptr);

    std::vector<rive::TextRun> truns;
    std::vector<rive::Unichar> unichars;
    truns.push_back(append(&unichars, font, 32.0f, u8"hi\u2028 "));

    auto paragraphs = font->shapeText(unichars, truns);
    REQUIRE(paragraphs.size() == 1);
    {
        const auto& paragraph = paragraphs.front();
        REQUIRE(paragraph.baseDirection == rive::TextDirection::ltr);
        {
            auto lines = GlyphLine::BreakLines(paragraph.runs, -1.0f);
            REQUIRE(lines.size() == 2);
        }
    }
}

TEST_CASE("line breaker deals with empty lines", "[line break]")
{
    auto font = loadFont("../../test/assets/IBMPlexSansArabic-Regular.ttf");
    REQUIRE(font != nullptr);

    std::vector<rive::TextRun> truns;
    std::vector<rive::Unichar> unichars;
    truns.push_back(append(&unichars, font, 32.0f, "hi\n"));

    auto paragraphs = font->shapeText(unichars, truns);
    REQUIRE(paragraphs.size() == 1);
    {
        const auto& paragraph = paragraphs.front();
        REQUIRE(paragraph.baseDirection == rive::TextDirection::ltr);
        {
            auto lines = GlyphLine::BreakLines(paragraph.runs, -1.0f);
            REQUIRE(lines.size() == 1);
            {
                auto line = lines.front();
                REQUIRE(line.startRunIndex == 0);
                REQUIRE(line.startGlyphIndex == 0);
                REQUIRE(paragraph.runs[line.startRunIndex].glyphs.size() == 3);
                REQUIRE(paragraph.runs[line.startRunIndex].textIndices.size() == 3);
                REQUIRE(paragraph.runs[line.startRunIndex].textIndices[0] == 0);
            }
        }
    }
}
