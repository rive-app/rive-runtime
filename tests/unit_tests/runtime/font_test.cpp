#include "rive/simple_array.hpp"
#include "catch.hpp"
#include "rive/text_engine.hpp"
#include "rive/text/font_hb.hpp"
#include "rive/text/utf.hpp"
#include "hb.h"
#include "hb-ot.h"
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
static rive::rcp<rive::Font> pickFallbackFont(const rive::Unichar missing,
                                              const uint32_t fallbackIndex,
                                              const rive::Font*)
{
    if (fallbackIndex > 0)
    {
        return nullptr;
    }
    size_t length = fallbackFonts.size();
    for (size_t i = fallbackIndex; i < length; i++)
    {
        HBFont* font = static_cast<HBFont*>(fallbackFonts[i].get());
        if (font->hasGlyph(missing))
        {
            return fallbackFonts[i];
        }
    }
    return nullptr;
}

TEST_CASE("Inspect Font Styles", "[text_styles]")
{
    struct TestCaseData
    {
        const char* fontPath;
        uint16_t expectedWeight;
        bool expectedItalic;
    };

    std::vector<TestCaseData> testCases = {
        {"assets/fonts/AdventPro-VariableFont_wdth,wght.ttf", 400, false},
        {"assets/fonts/Inter_18pt-Regular.ttf", 400, false},
        {"assets/fonts/Inter_28pt-Bold.ttf", 700, false},
        {"assets/fonts/OpenSans-Italic.ttf", 400, true},
        {"assets/fonts/OpenSans-ExtraBoldItalic.ttf", 800, true},
    };

    for (const auto& testCase : testCases)
    {
        SECTION(testCase.fontPath)
        {
            rive::rcp<Font> font = loadFont(testCase.fontPath);
            HBFont* hbFont = static_cast<HBFont*>(font.get());

            REQUIRE(hbFont->getWeight() == testCase.expectedWeight);
            REQUIRE(hbFont->isItalic() == testCase.expectedItalic);
        }
    }
}

TEST_CASE("font exposes cap and x height for vertical trim", "[text_styles]")
{
    // Latin text fonts have an 'H' and 'x'; their tops must sit between the
    // baseline and the ascent, with the x-height below the cap-height. All of
    // capHeight/xHeight/ascent are stored negative (up is -Y).
    std::vector<const char*> fontPaths = {
        "assets/fonts/Inter_18pt-Regular.ttf",
        "assets/Montserrat.ttf",
    };
    for (const char* path : fontPaths)
    {
        SECTION(path)
        {
            rive::rcp<Font> font = loadFont(path);
            const Font::LineMetrics& metrics = font->lineMetrics();

            REQUIRE(metrics.capHeight < 0.0f);
            REQUIRE(metrics.capHeight >= metrics.ascent);
            // x-height is below the cap-height (less far above the baseline).
            REQUIRE(metrics.xHeight > metrics.capHeight);
            REQUIRE(metrics.xHeight < 0.0f);

            // The size-scaled accessors scale linearly.
            REQUIRE(font->capHeight(20.0f) ==
                    Approx(metrics.capHeight * 20.0f));
            REQUIRE(font->xHeight(20.0f) == Approx(metrics.xHeight * 20.0f));
        }
    }
}

TEST_CASE("fallback glyphs are found", "[text_fallback]")
{
    REQUIRE(fallbackFonts.empty());
    auto font = loadFont("assets/RobotoFlex.ttf");
    REQUIRE(font != nullptr);
    auto fallbackFont = loadFont("assets/IBMPlexSansArabic-Regular.ttf");
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
    auto font = loadFont("assets/RobotoFlex.ttf");
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
    rive::rcp<rive::Font> vfont =
        font->makeAtCoords(rive::Span<HBFont::Coord>(&coord, 1));
    REQUIRE(vfont->getAxisValue(2003265652) == 800.0f);

    rive::Font::Coord coord2 = {2003072104, 122.0f};
    rive::rcp<rive::Font> vfont2 =
        vfont->makeAtCoords(rive::Span<HBFont::Coord>(&coord2, 1));
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
    return std::find(std::begin(featureStrings),
                     std::end(featureStrings),
                     tag) != std::end(featureStrings);
}

TEST_CASE("font features load as expected", "[text]")
{
    REQUIRE(fallbackFonts.empty());
    auto font = loadFont("assets/RobotoFlex.ttf");
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

// DecodeFile maps the file instead of copying it. The mapping is invisible to
// callers, so these assert that it is equivalent to Decode and that every
// failure path degrades to null rather than crashing (callers fall back).
TEST_CASE("mapped font decodes equivalently to a copied one", "[text]")
{
    const char* path = "assets/fonts/Inter_18pt-Regular.ttf";
#ifndef RIVE_HB_FILE_MAPPING
    // No file mapping here (Windows): callers fall back to Decode.
    REQUIRE(HBFont::DecodeFile(path) == nullptr);
    return;
#else
    auto copied = loadFont(path);
    auto mapped = HBFont::DecodeFile(path);
    REQUIRE(copied != nullptr);
    REQUIRE(mapped != nullptr);

    // Same metrics.
    REQUIRE(mapped->getWeight() == copied->getWeight());
    REQUIRE(mapped->isItalic() == copied->isItalic());
    REQUIRE(mapped->getAxisCount() == copied->getAxisCount());

    // Same shaping: identical glyphs and advances for the same text.
    std::vector<rive::Unichar> mappedText, copiedText;
    std::vector<rive::TextRun> mappedRuns, copiedRuns;
    copiedRuns.push_back(append(&copiedText, copied, 32.0f, "Shaping parity"));
    mappedRuns.push_back(append(&mappedText, mapped, 32.0f, "Shaping parity"));

    auto copiedShape = copied->shapeText(copiedText, copiedRuns);
    auto mappedShape = mapped->shapeText(mappedText, mappedRuns);
    REQUIRE(mappedShape.size() == copiedShape.size());
    for (size_t p = 0; p < copiedShape.size(); p++)
    {
        const auto& a = copiedShape[p];
        const auto& b = mappedShape[p];
        REQUIRE(b.runs.size() == a.runs.size());
        for (size_t r = 0; r < a.runs.size(); r++)
        {
            REQUIRE(b.runs[r].glyphs.size() == a.runs[r].glyphs.size());
            for (size_t g = 0; g < a.runs[r].glyphs.size(); g++)
            {
                REQUIRE(b.runs[r].glyphs[g] == a.runs[r].glyphs[g]);
                REQUIRE(b.runs[r].advances[g] == a.runs[r].advances[g]);
            }
        }
    }
#endif
}

TEST_CASE("mapped font outlives the call that created it", "[text]")
{
    const char* path = "assets/fonts/Inter_18pt-Regular.ttf";
#ifndef RIVE_HB_FILE_MAPPING
    REQUIRE(HBFont::DecodeFile(path) == nullptr);
    return;
#else
    // The mapping is owned by harfbuzz's blob, not by any local: glyph data
    // must still be readable once DecodeFile's scope is long gone.
    rcp<Font> font;
    {
        font = HBFont::DecodeFile(path);
    }
    REQUIRE(font != nullptr);
    REQUIRE(font->hasGlyph('A'));

    // Shaping reads the mapped tables, so this would fault or return nothing
    // if the mapping had been released with the local scope.
    std::vector<rive::Unichar> text;
    std::vector<rive::TextRun> runs;
    runs.push_back(append(&text, font, 32.0f, "A"));
    auto shape = font->shapeText(text, runs);
    REQUIRE(shape.size() == 1);
    REQUIRE(shape[0].runs.size() == 1);
    REQUIRE(shape[0].runs[0].glyphs.size() == 1);
    REQUIRE(font->getPath(shape[0].runs[0].glyphs[0]).verbs().size() > 0);
#endif
}

TEST_CASE("DecodeFile returns null rather than failing hard", "[text]")
{
    // Callers treat null as "use Decode instead", so every failure this call
    // can detect must return null rather than throw or crash. Note these are
    // only the failures detectable here: arbitrary non-font data is NOT one of
    // them, since hb_face_create_or_fail does not validate that a non-empty
    // blob is a font -- the byte-based Decode has the same behaviour.
    REQUIRE(HBFont::DecodeFile(nullptr) == nullptr);
    REQUIRE(HBFont::DecodeFile("assets/does_not_exist.ttf") == nullptr);

    // An empty file: rejected on size before anything is mapped.
    const char* emptyPath = "assets/empty_font_test_file.tmp";
    FILE* fp = fopen(emptyPath, "wb");
    REQUIRE(fp != nullptr);
    fclose(fp);
    REQUIRE(HBFont::DecodeFile(emptyPath) == nullptr);
    remove(emptyPath);
}
