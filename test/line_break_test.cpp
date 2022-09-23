/*
 * Copyright 2022 Rive
 */

#include <rive/simple_array.hpp>
#include <catch.hpp>
#include <rive/render_text.hpp>
#include <rive/text/renderfont_hb.hpp>
#include <rive/text/line_breaker.hpp>
#include "utils/rive_utf.hpp"

using namespace rive;

static rive::RenderTextRun append(std::vector<rive::Unichar>* unichars,
                                  rive::rcp<rive::RenderFont> font,
                                  float size,
                                  const char text[]) {
    const uint8_t* ptr = (const uint8_t*)text;
    uint32_t n = 0;
    while (*ptr) {
        unichars->push_back(rive::UTF::NextUTF8(&ptr));
        n += 1;
    }
    return {std::move(font), size, n};
}

static rcp<RenderFont> loadFont(const char* filename) {
    FILE* fp = fopen("../../test/assets/RobotoFlex.ttf", "rb");
    REQUIRE(fp != nullptr);

    fseek(fp, 0, SEEK_END);
    const size_t length = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    std::vector<uint8_t> bytes(length);
    REQUIRE(fread(bytes.data(), 1, length, fp) == length);
    fclose(fp);

    return HBRenderFont::Decode(bytes);
}

TEST_CASE("line breaker separates words", "[line break]") {
    auto font = loadFont("../../test/assets/RobotoFlex.ttf");
    REQUIRE(font != nullptr);

    // one two⏎ three
    std::vector<rive::RenderTextRun> truns;
    std::vector<rive::Unichar> unichars;
    truns.push_back(append(&unichars, font, 32.0f, "one two three"));

    auto shape = font->shapeText(unichars, truns);
    REQUIRE(shape.size() == 1);
    auto run = shape.front();
    REQUIRE(run.breaks.size() == 6);
    REQUIRE(run.breaks[0] == 0);
    REQUIRE(run.breaks[1] == 3);
    REQUIRE(run.breaks[2] == 4);
    REQUIRE(run.breaks[3] == 7);
    REQUIRE(run.breaks[4] == 8);
    REQUIRE(run.breaks[5] == 13);
}

TEST_CASE("line breaker handles multiple runs", "[line break]") {
    auto font = loadFont("../../test/assets/RobotoFlex.ttf");
    REQUIRE(font != nullptr);

    std::vector<rive::RenderTextRun> truns;
    std::vector<rive::Unichar> unichars;
    truns.push_back(append(&unichars, font, 32.0f, "one two thr"));
    truns.push_back(append(&unichars, font, 60.0f, "ee four"));

    auto shape = font->shapeText(unichars, truns);
    REQUIRE(shape.size() == 2);
    {
        auto run = shape.front();
        REQUIRE(run.breaks.size() == 5);
        REQUIRE(run.breaks[0] == 0);
        REQUIRE(run.breaks[1] == 3);
        REQUIRE(run.breaks[2] == 4);
        REQUIRE(run.breaks[3] == 7);
        REQUIRE(run.breaks[4] == 8);
    }
    {
        auto run = shape.back();
        REQUIRE(run.breaks.size() == 3);
        REQUIRE(run.breaks[0] == 2);
        REQUIRE(run.breaks[1] == 3);
        REQUIRE(run.breaks[2] == 7);
    }
}

TEST_CASE("line breaker handles returns", "[line break]") {
    auto font = loadFont("../../test/assets/RobotoFlex.ttf");
    REQUIRE(font != nullptr);

    std::vector<rive::RenderTextRun> truns;
    std::vector<rive::Unichar> unichars;
    truns.push_back(append(&unichars, font, 32.0f, "one two thr"));
    truns.push_back(append(&unichars, font, 60.0f, "ee\n four"));

    auto shape = font->shapeText(unichars, truns);
    REQUIRE(shape.size() == 2);
    {
        auto run = shape.front();
        REQUIRE(run.breaks.size() == 5);
        REQUIRE(run.breaks[0] == 0);
        REQUIRE(run.breaks[1] == 3);
        REQUIRE(run.breaks[2] == 4);
        REQUIRE(run.breaks[3] == 7);
        REQUIRE(run.breaks[4] == 8);
    }
    {
        auto run = shape.back();
        REQUIRE(run.breaks.size() == 5);
        REQUIRE(run.breaks[0] == 2);
        REQUIRE(run.breaks[1] == 2);
        REQUIRE(run.breaks[2] == 2);
        REQUIRE(run.breaks[3] == 4);
        REQUIRE(run.breaks[4] == 8);
    }
}

TEST_CASE("line breaker builds lines", "[line break]") {
    auto font = loadFont("../../test/assets/RobotoFlex.ttf");
    REQUIRE(font != nullptr);

    // one two⏎ three
    std::vector<rive::RenderTextRun> truns;
    std::vector<rive::Unichar> unichars;
    truns.push_back(append(&unichars, font, 32.0f, "one two three"));

    auto shape = font->shapeText(unichars, truns);
    REQUIRE(shape.size() == 1);
    auto run = shape.front();

    // at 194 everything fits in one line
    {
        auto lines = RenderGlyphLine::BreakLines(shape, 194.0f, RenderTextAlign::left);
        REQUIRE(lines.size() == 1);
        auto line = lines.back();
        REQUIRE(line.startRun == 0);
        REQUIRE(line.startIndex == 0);
        REQUIRE(line.endRun == 0);
        REQUIRE(line.endIndex == 13);
    }
    // at 191 "three" should pop to second line
    {
        auto lines = RenderGlyphLine::BreakLines(shape, 191.0f, RenderTextAlign::left);
        REQUIRE(lines.size() == 2);
        {
            auto line = lines.front();
            REQUIRE(line.startRun == 0);
            REQUIRE(line.startIndex == 0);
            REQUIRE(line.endRun == 0);
            REQUIRE(line.endIndex == 7);
        }

        {
            auto line = lines.back();
            REQUIRE(line.startRun == 0);
            REQUIRE(line.startIndex == 8);
            REQUIRE(line.endRun == 0);
            REQUIRE(line.endIndex == 13);
        }
    }
}

TEST_CASE("line breaker deals with extremes", "[line break]") {
    auto font = loadFont("../../test/assets/RobotoFlex.ttf");
    REQUIRE(font != nullptr);

    // one two⏎ three
    std::vector<rive::RenderTextRun> truns;
    std::vector<rive::Unichar> unichars;
    truns.push_back(append(&unichars, font, 32.0f, "ab"));

    auto shape = font->shapeText(unichars, truns);
    REQUIRE(shape.size() == 1);
    auto run = shape.front();

    {
        auto lines = RenderGlyphLine::BreakLines(shape, 17.0f, RenderTextAlign::left);
        REQUIRE(lines.size() == 2);
        {
            auto line = lines.front();
            REQUIRE(line.startRun == 0);
            REQUIRE(line.startIndex == 0);
            REQUIRE(line.endRun == 0);
            REQUIRE(line.endIndex == 1);
        }

        {
            auto line = lines.back();
            REQUIRE(line.startRun == 0);
            REQUIRE(line.startIndex == 1);
            REQUIRE(line.endRun == 0);
            REQUIRE(line.endIndex == 2);
        }
    }
    // Test that it also handles 0 width.
    {
        auto lines = RenderGlyphLine::BreakLines(shape, 0.0f, RenderTextAlign::left);
        REQUIRE(lines.size() == 2);
        {
            auto line = lines.front();
            REQUIRE(line.startRun == 0);
            REQUIRE(line.startIndex == 0);
            REQUIRE(line.endRun == 0);
            REQUIRE(line.endIndex == 1);
        }

        {
            auto line = lines.back();
            REQUIRE(line.startRun == 0);
            REQUIRE(line.startIndex == 1);
            REQUIRE(line.endRun == 0);
            REQUIRE(line.endIndex == 2);
        }
    }
}

TEST_CASE("line breaker breaks return characters", "[line break]") {
    auto font = loadFont("../../test/assets/RobotoFlex.ttf");
    REQUIRE(font != nullptr);

    // one two⏎ three
    std::vector<rive::RenderTextRun> truns;
    std::vector<rive::Unichar> unichars;
    truns.push_back(append(&unichars, font, 32.0f, "hello look\nhere"));

    auto shape = font->shapeText(unichars, truns);
    REQUIRE(shape.size() == 1);
    auto run = shape.front();

    {
        auto lines = RenderGlyphLine::BreakLines(shape, 300.0f, RenderTextAlign::left);
        REQUIRE(lines.size() == 2);
    }
}
