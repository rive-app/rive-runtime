/*
 * Copyright 2026 Rive
 */

#include <catch.hpp>
#include "rive/text/utf.hpp"

using namespace rive;

// Decode a surrogate pair back to the code point it stands for.
static Unichar fromSurrogatePair(uint16_t hi, uint16_t lo)
{
    return 0x10000 + ((Unichar(hi) - 0xD800) << 10) + (Unichar(lo) - 0xDC00);
}

TEST_CASE("BMP code points encode to a single UTF-16 unit", "[utf]")
{
    uint16_t utf16[2];
    for (Unichar uni : {Unichar(0),
                        Unichar('A'),
                        Unichar(0xD7FF),
                        Unichar(0xE000),
                        Unichar(0xFFFF)})
    {
        REQUIRE(UTF::ToUTF16(uni, utf16) == 1);
        CHECK(utf16[0] == uni);
    }
}

TEST_CASE("supplementary code points encode to a valid surrogate pair", "[utf]")
{
    // 0xD835/0xDC0C is MATHEMATICAL BOLD CAPITAL M; the old bit-or shortcut
    // produced 0xD7F5 for its lead unit, which is not a surrogate at all.
    struct
    {
        Unichar uni;
        uint16_t hi, lo;
    } cases[] = {
        {0x10000, 0xD800, 0xDC00},
        {0x1D40C, 0xD835, 0xDC0C},
        {0x1F600, 0xD83D, 0xDE00},
        {0x10FFFF, 0xDBFF, 0xDFFF},
    };

    uint16_t utf16[2];
    for (auto& c : cases)
    {
        REQUIRE(UTF::ToUTF16(c.uni, utf16) == 2);
        CHECK(utf16[0] == c.hi);
        CHECK(utf16[1] == c.lo);
    }
}

TEST_CASE("every supplementary code point round trips through UTF-16", "[utf]")
{
    uint16_t utf16[2];
    for (Unichar uni = 0x10000; uni <= 0x10FFFF; ++uni)
    {
        REQUIRE(UTF::ToUTF16(uni, utf16) == 2);
        REQUIRE(utf16[0] >= 0xD800);
        REQUIRE(utf16[0] <= 0xDBFF);
        REQUIRE(utf16[1] >= 0xDC00);
        REQUIRE(utf16[1] <= 0xDFFF);
        REQUIRE(fromSurrogatePair(utf16[0], utf16[1]) == uni);
    }
}
