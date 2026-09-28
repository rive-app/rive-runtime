#include "catch.hpp"
#include "rive/text/raw_text.hpp"
#include "rive/text/font_hb.hpp"
#include "utils/no_op_factory.hpp"
#include <cstdio>
#include <vector>

using namespace rive;

namespace
{
rcp<Font> loadFont(const char* filename)
{
    FILE* fp = fopen(filename, "rb");
    REQUIRE(fp != nullptr);
    fseek(fp, 0, SEEK_END);
    std::vector<uint8_t> bytes(ftell(fp));
    fseek(fp, 0, SEEK_SET);
    REQUIRE(fread(bytes.data(), 1, bytes.size(), fp) == bytes.size());
    fclose(fp);
    return HBFont::Decode(bytes);
}
} // namespace

TEST_CASE("RawText keeps one style per paint and foreground color", "[rawtext]")
{
    NoOpFactory factory;
    RawText text(&factory);
    auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");
    text.append("red", nullptr, font, 16.0f, -1.0f, 0.0f, 0xFFFF0000);
    text.append("blue", nullptr, font, 16.0f, -1.0f, 0.0f, 0xFF0000FF);
    text.append("red again", nullptr, font, 16.0f, -1.0f, 0.0f, 0xFFFF0000);
    REQUIRE(text.styleCount() == 2);
    CHECK(text.styleForegroundColor(0) == 0xFFFF0000);
    CHECK(text.styleForegroundColor(1) == 0xFF0000FF);
}

TEST_CASE("RawText clear drops the styles with the runs", "[rawtext]")
{
    NoOpFactory factory;
    Factory* paints = &factory;
    RawText text(&factory);
    auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");
    for (int i = 0; i < 1000; i++)
    {
        text.clear();
        text.append("frame", paints->makeRenderPaint(), font);
        text.bounds();
    }
    CHECK(text.styleCount() == 1);
}

TEST_CASE("RawText ignores layout ordinals out of range", "[rawtext]")
{
    NoOpFactory factory;
    RawText text(&factory);
    auto font = loadFont("assets/fonts/Inter_18pt-Regular.ttf");
    text.append("kept", nullptr, font);
    text.sizingIndex((int)TextSizing::fixed);
    text.overflowIndex((int)TextOverflow::ellipsis);
    text.alignIndex(4);
    text.directionIndex(2);
    text.maxWidth(40.0f);
    text.maxHeight(10.0f);
    AABB before = text.bounds();

    text.sizingIndex(5);
    text.overflowIndex(9);
    text.alignIndex(7);
    text.directionIndex(-1);
    text.wrapIndex(3);
    text.wordBreakIndex(-2);
    text.originIndex(2);
    CHECK(text.sizing() == TextSizing::fixed);
    CHECK(text.overflow() == TextOverflow::ellipsis);
    CHECK(text.alignIndex() == 4);
    CHECK(text.directionIndex() == 2);
    CHECK(text.wrap() == TextWrap::wrap);
    CHECK(text.wordBreak() == TextWordBreak::breakWord);
    CHECK(text.origin() == TextOrigin::top);
    AABB after = text.bounds();
    CHECK(after.minX == before.minX);
    CHECK(after.maxY == before.maxY);
}
