#include "rive/simple_array.hpp"
#include "catch.hpp"
#include "rive/text_engine.hpp"
#include "rive/text/font_hb.hpp"
#include "rive/text/utf.hpp"
#include <string>

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

    Font::gFallbackProc = pickFallbackFont;

    std::vector<rive::TextRun> truns;
    std::vector<rive::Unichar> unichars;
    truns.push_back(append(&unichars, font, 32.0f, "لمفاتيح ABC DEF"));

    auto paragraphs = font->shapeText(unichars, truns);
    REQUIRE(paragraphs.size() == 1);
    paragraphs = SimpleArray<Paragraph>();
    REQUIRE(paragraphs.size() == 0);
    fallbackFonts.clear();
    Font::gFallbackProc = nullptr;
}

TEST_CASE("variable axis values can be read", "[text]")
{
    REQUIRE(fallbackFonts.empty());
    auto font = loadFont("../../test/assets/RobotoFlex.ttf");
    REQUIRE(font != nullptr);

    auto count = font->getAxisCount();

    bool hasWeight = false;
    for (uint16_t i = 0; i < count; i++)
    {
        auto axis = font->getAxis(i);
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

    REQUIRE(font->getAxisValue(2003072104) == 100.0f);

    rive::Font::Coord coord = {2003265652, 800.0f};
    rive::rcp<rive::Font> vfont = font->makeAtCoords(rive::Span<HBFont::Coord>(&coord, 1));
    REQUIRE(vfont->getAxisValue(2003265652) == 800.0f);

    rive::Font::Coord coord2 = {2003072104, 122.0f};
    rive::rcp<rive::Font> vfont2 = vfont->makeAtCoords(rive::Span<HBFont::Coord>(&coord2, 1));
    REQUIRE(vfont2->getAxisValue(2003072104) == 122.0f);
    // Should also still have the first axis value we set.
    REQUIRE(vfont2->getAxisValue(2003265652) == 800.0f);
}

static std::string tagToString(uint32_t tag)
{
    std::string tag_name;
    tag_name += ((char)((tag & 0xff000000) >> 24));
    tag_name += ((char)((tag & 0x00ff0000) >> 16));
    tag_name += ((char)((tag & 0x0000ff00) >> 8));
    tag_name += ((char)((tag & 0x000000ff)));
    return tag_name;
}

static bool hasTag(std::vector<std::string> featureStrings, std::string tag)
{
    return std::find(std::begin(featureStrings), std::end(featureStrings), tag) !=
           std::end(featureStrings);
}

TEST_CASE("font features load as expected", "[text]")
{
    REQUIRE(fallbackFonts.empty());
    auto font = loadFont("../../test/assets/RobotoFlex.ttf");
    REQUIRE(font != nullptr);

    rive::SimpleArray<uint32_t> features = font->features();
    std::vector<std::string> featureStrings;
    for (auto feature : features)
    {
        featureStrings.push_back(tagToString(feature));
    }
    REQUIRE(features.size() == 7);

    REQUIRE(hasTag(featureStrings, "mkmk"));
    REQUIRE(hasTag(featureStrings, "kern"));
    REQUIRE(hasTag(featureStrings, "rvrn"));
    REQUIRE(hasTag(featureStrings, "mark"));
    REQUIRE(hasTag(featureStrings, "locl"));
    REQUIRE(hasTag(featureStrings, "pnum"));
    REQUIRE(hasTag(featureStrings, "liga"));
}
