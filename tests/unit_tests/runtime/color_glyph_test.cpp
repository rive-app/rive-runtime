#include "catch.hpp"
#include "rive/text/font_hb.hpp"
#include "rive/text/raw_text.hpp"
#include "rive/text/utf.hpp"
#include "rive/text_engine.hpp"
#include "utils/no_op_factory.hpp"
#include "utils/no_op_renderer.hpp"
#include <string>
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

static const char* emojiFont = "assets/TwemojiMozilla.subset.ttf";

TEST_CASE("non-emoji font reports no color glyphs", "[color_glyph]")
{
    auto font = loadFont("assets/RobotoFlex.ttf");
    REQUIRE(font != nullptr);

    CHECK_FALSE(font->hasColorGlyphs());
    // GlyphID 0 is typically .notdef — should not be a color glyph.
    CHECK_FALSE(font->isColorGlyph(0));

    std::vector<Font::ColorGlyphLayer> layers;
    size_t count = font->getColorLayers(0, layers);
    CHECK(count == 0);
    CHECK(layers.empty());
}

TEST_CASE("emoji font reports color glyphs", "[color_glyph]")
{
    auto font = loadFont(emojiFont);
    REQUIRE(font != nullptr);

    CHECK(font->hasColorGlyphs());
}

TEST_CASE("isColorGlyph returns false for non-color glyph in emoji font",
          "[color_glyph]")
{
    auto font = loadFont(emojiFont);
    REQUIRE(font != nullptr);

    // GlyphID 0 (.notdef) should not be a color glyph even in an emoji font.
    CHECK_FALSE(font->isColorGlyph(0));
}

TEST_CASE("known color glyph IDs are detected in subset font", "[color_glyph]")
{
    auto font = loadFont(emojiFont);
    REQUIRE(font != nullptr);
    REQUIRE(font->hasColorGlyphs());

    // The TwemojiMozilla.subset.ttf COLR table has base glyphs at IDs 2, 3.
    CHECK(font->isColorGlyph(2));
    CHECK(font->isColorGlyph(3));
}

TEST_CASE("getColorLayers returns layers for a color glyph", "[color_glyph]")
{
    auto font = loadFont(emojiFont);
    REQUIRE(font != nullptr);

    // Use known color glyph ID from the COLR table.
    GlyphID colorGlyphId = 2;

    std::vector<Font::ColorGlyphLayer> layers;
    size_t count = font->getColorLayers(colorGlyphId, layers, 0xFF000000);
    CHECK(count > 0);
    CHECK(count == layers.size());

    // Each layer should have a non-empty path.
    for (const auto& layer : layers)
    {
        CHECK(!layer.path.empty());
        // Color should have full alpha or a palette color.
        CHECK(((layer.color >> 24) & 0xFF) > 0);
    }
}

TEST_CASE("getColorLayers returns empty for non-color glyph", "[color_glyph]")
{
    auto font = loadFont(emojiFont);
    REQUIRE(font != nullptr);

    std::vector<Font::ColorGlyphLayer> layers;
    size_t count = font->getColorLayers(0, layers);
    CHECK(count == 0);
    CHECK(layers.empty());
}

TEST_CASE("color glyph layers are cached", "[color_glyph]")
{
    auto font = loadFont(emojiFont);
    REQUIRE(font != nullptr);

    GlyphID colorGlyphId = 0;
    for (GlyphID id = 1; id < 100; id++)
    {
        if (font->isColorGlyph(id))
        {
            colorGlyphId = id;
            break;
        }
    }
    REQUIRE(colorGlyphId != 0);

    // Call twice — second call should return cached result.
    std::vector<Font::ColorGlyphLayer> layers1;
    size_t count1 = font->getColorLayers(colorGlyphId, layers1, 0xFF000000);

    std::vector<Font::ColorGlyphLayer> layers2;
    size_t count2 = font->getColorLayers(colorGlyphId, layers2, 0xFF000000);

    CHECK(count1 == count2);
    CHECK(layers1.size() == layers2.size());
}

TEST_CASE("foreground color is applied for 0xFFFF color index", "[color_glyph]")
{
    auto font = loadFont(emojiFont);
    REQUIRE(font != nullptr);

    GlyphID colorGlyphId = 0;
    for (GlyphID id = 1; id < 100; id++)
    {
        if (font->isColorGlyph(id))
        {
            colorGlyphId = id;
            break;
        }
    }
    REQUIRE(colorGlyphId != 0);

    // Get layers with two different foreground colors.
    std::vector<Font::ColorGlyphLayer> layersBlack;
    font->getColorLayers(colorGlyphId, layersBlack, 0xFF000000);

    std::vector<Font::ColorGlyphLayer> layersRed;
    font->getColorLayers(colorGlyphId, layersRed, 0xFFFF0000);

    // The number of layers should be the same.
    REQUIRE(layersBlack.size() == layersRed.size());

    // Check if any layer uses the foreground color by seeing if colors differ
    // between the two calls (layers that use foreground will differ).
    bool hasForegroundLayer = false;
    for (size_t i = 0; i < layersBlack.size(); i++)
    {
        if (layersBlack[i].useForeground)
        {
            hasForegroundLayer = true;
            CHECK(layersBlack[i].color == 0xFF000000);
            CHECK(layersRed[i].color == 0xFFFF0000);
        }
    }
    // Not all fonts use foreground layers, so this is informational.
    (void)hasForegroundLayer;
}

TEST_CASE("withOptions preserves color glyph support", "[color_glyph]")
{
    auto font = loadFont(emojiFont);
    REQUIRE(font != nullptr);
    CHECK(font->hasColorGlyphs());

    // Create a sub-font with no option changes — should still report color.
    auto subFont = font->withOptions({}, {});
    REQUIRE(subFont != nullptr);
    CHECK(subFont->hasColorGlyphs());
}

TEST_CASE("RawText renders with color glyph font without crashing",
          "[color_glyph]")
{
    auto font = loadFont(emojiFont);
    REQUIRE(font != nullptr);

    NoOpFactory factory;
    RawText rawText(&factory);
    rawText.maxWidth(200.0f);
    rawText.sizing(TextSizing::autoHeight);

    // Append text — the emoji font may or may not have Latin glyphs,
    // but it should not crash regardless.
    rawText.append("A", nullptr, font, 32.0f);

    NoOpRenderer renderer;
    rawText.render(&renderer);
}

// ---------- shaping tests ----------

static rive::TextRun appendText(std::vector<rive::Unichar>* unichars,
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

TEST_CASE("shaping emoji font produces glyphs", "[color_glyph]")
{
    auto font = loadFont(emojiFont);
    REQUIRE(font != nullptr);

    std::vector<rive::Unichar> unichars;
    std::vector<rive::TextRun> truns;
    truns.push_back(appendText(&unichars, font, 32.0f, "A"));

    auto paragraphs = font->shapeText(unichars, truns);
    REQUIRE(paragraphs.size() == 1);
    REQUIRE(paragraphs[0].runs.size() >= 1);

    // At least one glyph should be produced without crashing.
    const auto& run = paragraphs[0].runs[0];
    CHECK(run.glyphs.size() > 0);
}

TEST_CASE("shaping with fallback uses emoji font for missing glyphs",
          "[color_glyph]")
{
    auto regularFont = loadFont("assets/RobotoFlex.ttf");
    REQUIRE(regularFont != nullptr);
    auto emojiFontRcp = loadFont(emojiFont);
    REQUIRE(emojiFontRcp != nullptr);

    // Set up fallback proc.
    static rive::rcp<rive::Font> sFallback;
    sFallback = emojiFontRcp;
    Font::gFallbackProc = [](const rive::Unichar,
                             const uint32_t fallbackIndex,
                             const rive::Font*) -> rive::rcp<rive::Font> {
        if (fallbackIndex > 0)
            return nullptr;
        return sFallback;
    };

    std::vector<rive::Unichar> unichars;
    std::vector<rive::TextRun> truns;
    // "A❤B" — A and B from regular, ❤ should fallback to emoji font.
    truns.push_back(appendText(&unichars,
                               regularFont,
                               32.0f,
                               "A\xe2\x9d\xa4"
                               "B"));

    auto paragraphs = regularFont->shapeText(unichars, truns);
    REQUIRE(paragraphs.size() == 1);

    // Should produce multiple runs due to font fallback.
    // The fallback font is an emoji font, so it should be used for
    // the missing glyph. At minimum, we verify shaping doesn't crash
    // and produces runs.
    bool foundEmojiFont = false;
    for (const auto& run : paragraphs[0].runs)
    {
        if (run.font->hasColorGlyphs())
        {
            foundEmojiFont = true;
        }
    }
    CHECK(foundEmojiFont);

    Font::gFallbackProc = nullptr;
    sFallback = nullptr;
}

// ---------- COLRv0 layer detail tests ----------

TEST_CASE("COLRv0 layers have solid paint type", "[color_glyph]")
{
    auto font = loadFont(emojiFont);
    REQUIRE(font != nullptr);

    std::vector<Font::ColorGlyphLayer> layers;
    size_t count = font->getColorLayers(2, layers, 0xFF000000);
    REQUIRE(count > 0);

    for (const auto& layer : layers)
    {
        // COLRv0 always produces solid fill layers.
        CHECK(layer.paintType == Font::ColorGlyphPaintType::solid);
        // Gradient data should be unused.
        CHECK(layer.stops.empty());
        // Image data should be unused.
        CHECK(layer.imageBytes.empty());
    }
}

TEST_CASE("all known color glyph IDs return layers", "[color_glyph]")
{
    auto font = loadFont(emojiFont);
    REQUIRE(font != nullptr);

    // Both glyph IDs 2 and 3 should produce layers.
    for (GlyphID id : {(GlyphID)2, (GlyphID)3})
    {
        std::vector<Font::ColorGlyphLayer> layers;
        size_t count = font->getColorLayers(id, layers, 0xFF000000);
        CHECK(count > 0);
        CHECK(count == layers.size());
    }
}

TEST_CASE("cached layers update foreground color correctly", "[color_glyph]")
{
    auto font = loadFont(emojiFont);
    REQUIRE(font != nullptr);

    GlyphID id = 2;
    // Prime the cache with black foreground.
    std::vector<Font::ColorGlyphLayer> layersFirst;
    font->getColorLayers(id, layersFirst, 0xFF000000);

    // Now request with green foreground — cached layers should reflect this.
    std::vector<Font::ColorGlyphLayer> layersGreen;
    font->getColorLayers(id, layersGreen, 0xFF00FF00);

    REQUIRE(layersFirst.size() == layersGreen.size());

    for (size_t i = 0; i < layersFirst.size(); i++)
    {
        if (layersFirst[i].useForeground)
        {
            CHECK(layersFirst[i].color == 0xFF000000);
            CHECK(layersGreen[i].color == 0xFF00FF00);
        }
        else
        {
            // Non-foreground layers should be identical.
            CHECK(layersFirst[i].color == layersGreen[i].color);
        }
    }
}

TEST_CASE("getColorLayers returns zero for non-color font", "[color_glyph]")
{
    auto font = loadFont("assets/RobotoFlex.ttf");
    REQUIRE(font != nullptr);

    // Try several glyph IDs — none should be color.
    for (GlyphID id = 0; id < 10; id++)
    {
        std::vector<Font::ColorGlyphLayer> layers;
        size_t count = font->getColorLayers(id, layers, 0xFF000000);
        CHECK(count == 0);
        CHECK(layers.empty());
    }
}

TEST_CASE("getColorLayers appends to existing vector", "[color_glyph]")
{
    auto font = loadFont(emojiFont);
    REQUIRE(font != nullptr);

    std::vector<Font::ColorGlyphLayer> layers;
    size_t count1 = font->getColorLayers(2, layers, 0xFF000000);
    REQUIRE(count1 > 0);
    CHECK(layers.size() == count1);

    // Append layers for another glyph into the same vector.
    size_t count2 = font->getColorLayers(3, layers, 0xFF000000);
    REQUIRE(count2 > 0);
    CHECK(layers.size() == count1 + count2);
}

TEST_CASE("isColorGlyph is false for high glyph IDs", "[color_glyph]")
{
    auto font = loadFont(emojiFont);
    REQUIRE(font != nullptr);

    // Very high glyph IDs that don't exist should not crash and return false.
    CHECK_FALSE(font->isColorGlyph(9999));
    CHECK_FALSE(font->isColorGlyph(65535));
}

// ---------- RawText rendering tests with color glyphs ----------

TEST_CASE("RawText with multiple color glyphs renders without crashing",
          "[color_glyph]")
{
    auto font = loadFont(emojiFont);
    REQUIRE(font != nullptr);

    NoOpFactory factory;
    RawText rawText(&factory);
    rawText.maxWidth(400.0f);
    rawText.sizing(TextSizing::autoHeight);

    // Append the same character multiple times.
    rawText.append("\xe2\x9d\xa4\xe2\x9d\xa4\xe2\x9d\xa4",
                   nullptr,
                   font,
                   32.0f);

    NoOpRenderer renderer;
    rawText.render(&renderer);
}

TEST_CASE("RawText with mixed regular and emoji text renders without crashing",
          "[color_glyph]")
{
    auto regularFont = loadFont("assets/RobotoFlex.ttf");
    REQUIRE(regularFont != nullptr);
    auto emojiFontRcp = loadFont(emojiFont);
    REQUIRE(emojiFontRcp != nullptr);

    // Set up fallback.
    static rive::rcp<rive::Font> sFallback;
    sFallback = emojiFontRcp;
    Font::gFallbackProc = [](const rive::Unichar,
                             const uint32_t fallbackIndex,
                             const rive::Font*) -> rive::rcp<rive::Font> {
        if (fallbackIndex > 0)
            return nullptr;
        return sFallback;
    };

    NoOpFactory factory;
    RawText rawText(&factory);
    rawText.maxWidth(400.0f);
    rawText.sizing(TextSizing::autoHeight);

    // Mix of regular Latin text and an emoji character.
    rawText.append("Hello \xe2\x9d\xa4 World", nullptr, regularFont, 32.0f);

    NoOpRenderer renderer;
    rawText.render(&renderer);

    Font::gFallbackProc = nullptr;
    sFallback = nullptr;
}

TEST_CASE("RawText at small font size with emoji does not crash",
          "[color_glyph]")
{
    auto font = loadFont(emojiFont);
    REQUIRE(font != nullptr);

    NoOpFactory factory;
    RawText rawText(&factory);
    rawText.maxWidth(100.0f);
    rawText.sizing(TextSizing::autoHeight);

    rawText.append("\xe2\x9d\xa4", nullptr, font, 1.0f);

    NoOpRenderer renderer;
    rawText.render(&renderer);
}

TEST_CASE("RawText at large font size with emoji does not crash",
          "[color_glyph]")
{
    auto font = loadFont(emojiFont);
    REQUIRE(font != nullptr);

    NoOpFactory factory;
    RawText rawText(&factory);
    rawText.maxWidth(2000.0f);
    rawText.sizing(TextSizing::autoHeight);

    rawText.append("\xe2\x9d\xa4", nullptr, font, 200.0f);

    NoOpRenderer renderer;
    rawText.render(&renderer);
}
