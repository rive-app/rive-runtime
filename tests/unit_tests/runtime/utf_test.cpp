/*
 * Copyright 2026 Rive
 */

#include <catch.hpp>
#include "rive/text/utf.hpp"
#include <vector>

using namespace rive;

// Decode a surrogate pair back to the code point it stands for.
static Unichar fromSurrogatePair(uint16_t hi, uint16_t lo)
{
  return 0x10000 + ((Unichar(hi) - 0xD800) << 10) + (Unichar(lo) - 0xDC00);
}

TEST_CASE("BMP code points encode to a single UTF-16 unit", "[utf]")
{
  uint16_t utf16[2];
  for (Unichar uni :
       {Unichar(0),
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

// Decodes bytes the way a guest hands them over, with nothing past the end.
static std::vector<Unichar> decodeUTF8(const std::vector<uint8_t>& bytes)
{
  std::vector<Unichar> unichars;
  const uint8_t* ptr = bytes.data();
  const uint8_t* end = ptr + bytes.size();
  while (ptr < end)
  {
    unichars.push_back(UTF::NextUTF8(&ptr, end));
  }
  CHECK(ptr == end);
  return unichars;
}

TEST_CASE("UTF-8 decodes every sequence length", "[utf]")
{
  CHECK(
      decodeUTF8({'a', 0xC3, 0xA9, 0xE2, 0x82, 0xAC, 0xF0, 0x9F, 0x98, 0x80}) ==
      std::vector<Unichar>{'a', 0xE9, 0x20AC, 0x1F600});
}

TEST_CASE("UTF-8 cut short decodes as replacements inside its buffer", "[utf]")
{
  CHECK(decodeUTF8({0xF0}) == std::vector<Unichar>{0xFFFD});
  CHECK(
      decodeUTF8({'a', 0xF0, 0x9F, 0x98}) == std::vector<Unichar>{'a', 0xFFFD});
  // The byte that cuts a sequence short starts the next one.
  CHECK(
      decodeUTF8({0xE2, 'b', 0xC3, 0xE2, 0x82, 0xAC}) ==
      std::vector<Unichar>{0xFFFD, 'b', 0xFFFD, 0x20AC});
  CHECK(
      decodeUTF8({0xC3, 0x00, 'c'}) == std::vector<Unichar>{0xFFFD, 0x00, 'c'});
  // Stray continuations and leads past 0xF7 start no sequence.
  CHECK(
      decodeUTF8({0x80, 0xBF, 0xF8, 0xFF, 'z'}) ==
      std::vector<Unichar>{0xFFFD, 0xFFFD, 0xFFFD, 0xFFFD, 'z'});
}
