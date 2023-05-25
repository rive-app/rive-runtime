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
    return {std::move(font), size, n, 0};
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

static std::vector<rive::rcp<rive::Font>> fallbackFonts;
static rive::rcp<rive::Font> pickFallbackFont(rive::Span<const rive::Unichar> missing)
{
    size_t length = fallbackFonts.size();
    for (size_t i = 0; i < length; i++)
    {
        HBFont* font = static_cast<HBFont*>(fallbackFonts[i].get());
        if (font->hasGlyph(missing))
        {
            return fallbackFonts[i];
        }
    }
    return nullptr;
}

TEST_CASE("fallback glyphs are found", "[text]")
{
    REQUIRE(fallbackFonts.empty());
    auto font = loadFont("../../test/assets/RobotoFlex.ttf");
    REQUIRE(font != nullptr);
    auto fallbackFont = loadFont("../../test/assets/IBMPlexSansArabic-Regular.ttf");
    REQUIRE(fallbackFont != nullptr);
    fallbackFonts.push_back(fallbackFont);

    HBFont::gFallbackProc = pickFallbackFont;

    std::vector<rive::TextRun> truns;
    std::vector<rive::Unichar> unichars;
    truns.push_back(append(&unichars, font, 32.0f, "لمفاتيح ABC DEF"));

    auto paragraphs = font->shapeText(unichars, truns);
    REQUIRE(paragraphs.size() == 1);
    fallbackFonts.clear();
    HBFont::gFallbackProc = nullptr;
}

TEST_CASE("variable axis values can be read", "[text]")
{
    REQUIRE(fallbackFonts.empty());
    auto font = loadFont("../../test/assets/RobotoFlex.ttf");
    REQUIRE(font != nullptr);

    std::vector<rive::Font::Axis> axes = font->getAxes();

    bool hasWeight = false;
    for (rive::Font::Axis axis : axes)
    {
        if (axis.tag == 2003265652)
        {
            REQUIRE(axis.def == 400.0f);
            hasWeight = true;
            break;
        }
    }

    REQUIRE(hasWeight);

    float value = font->getAxisValue(2003265652);
    REQUIRE(value == 400.0f);

    rive::Font::Coord coord = {2003265652, 800.0f};
    rive::rcp<rive::Font> vfont = font->makeAtCoords(rive::Span<HBFont::Coord>(&coord, 1));
    REQUIRE(vfont->getAxisValue(2003265652) == 800.0f);

    rive::Font::Coord coord2 = {2003265652, 822.0f};
    rive::rcp<rive::Font> vfont2 = vfont->makeAtCoords(rive::Span<HBFont::Coord>(&coord2, 1));
    REQUIRE(vfont2->getAxisValue(2003265652) == 822.0f);
}
