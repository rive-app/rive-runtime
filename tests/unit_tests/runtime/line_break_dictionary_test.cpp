/*
 * Copyright 2026 Rive
 */

#include "rive/text/line_break_dictionary.hpp"

#ifdef RIVE_LINE_BREAK_DICTIONARIES
#include "rive/text/utf.hpp"
#include <catch.hpp>
#include <initializer_list>
#include <string>
#include <vector>

using namespace rive;

// MSVC rejects \u escapes in narrow literals, so tests pass u8 literals.
#define U8(literal) reinterpret_cast<const char*>(u8##literal)

// Returns utf8 with a '|' at every break opportunity.
static std::string segments(const std::string& utf8)
{
  std::vector<uint32_t> text;
  std::vector<std::string> pieces;
  const uint8_t* ptr = (const uint8_t*)utf8.c_str();
  const uint8_t* end = ptr + utf8.size();
  while (ptr < end)
  {
    const uint8_t* start = ptr;
    text.push_back(UTF::NextUTF8(&ptr, end));
    pieces.emplace_back((const char*)start, ptr - start);
  }
  std::vector<LineBreak> out(text.size() + 1);
  computeLineBreaks(text, out);
  std::string result;
  for (size_t i = 0; i < text.size(); i++)
  {
    if (i > 0 && out[i] != LineBreak::none)
    {
      result += '|';
    }
    result += pieces[i];
  }
  return result;
}

// Joins words into one string and requires breaks exactly between them.
static void requireWords(std::initializer_list<const char*> words)
{
  std::string text;
  std::string expected;
  for (const char* word : words)
  {
    if (!text.empty())
    {
      expected += '|';
    }
    text += word;
    expected += word;
  }
  REQUIRE(segments(text) == expected);
}

#ifdef WITH_RIVE_TEXT_DICT_THAI
TEST_CASE("line break dictionary splits Thai into words", "[line break]")
{
  // ภาษา|ไทย|ง่าย|นิด|เดียว
  requireWords(
      {U8("\u0E20\u0E32\u0E29\u0E32"),
       U8("\u0E44\u0E17\u0E22"),
       U8("\u0E07\u0E48\u0E32\u0E22"),
       U8("\u0E19\u0E34\u0E14"),
       U8("\u0E40\u0E14\u0E35\u0E22\u0E27")});

  // กรุงเทพมหานคร|เป็น|เมือง|หลวง
  requireWords(
      {U8("\u0E01\u0E23\u0E38\u0E07\u0E40\u0E17\u0E1E\u0E21\u0E2B\u0E32\u0E19\u0E04\u0E23"),
       U8("\u0E40\u0E1B\u0E47\u0E19"),
       U8("\u0E40\u0E21\u0E37\u0E2D\u0E07"),
       U8("\u0E2B\u0E25\u0E27\u0E07")});

  // เด็กๆ|ชอบ|กิน|ขนม, the repetition mark stays on its word.
  requireWords(
      {U8("\u0E40\u0E14\u0E47\u0E01\u0E46"),
       U8("\u0E0A\u0E2D\u0E1A"),
       U8("\u0E01\u0E34\u0E19"),
       U8("\u0E02\u0E19\u0E21")});

  // UAX #14 still decides the edges of a Thai run.
  requireWords(
      {U8("Hello "),
       U8("\u0E20\u0E32\u0E29\u0E32"),
       U8("\u0E44\u0E17\u0E22 "),
       U8("123 "),
       U8("ok")});
}
#endif

#ifdef WITH_RIVE_TEXT_DICT_LAO
TEST_CASE("line break dictionary splits Lao into words", "[line break]")
{
  // ປະເທດ|ລາວ|ເປັນ|ປະເທດ|ທີ່|ສວຍງາມ
  requireWords(
      {U8("\u0E9B\u0EB0\u0EC0\u0E97\u0E94"),
       U8("\u0EA5\u0EB2\u0EA7"),
       U8("\u0EC0\u0E9B\u0EB1\u0E99"),
       U8("\u0E9B\u0EB0\u0EC0\u0E97\u0E94"),
       U8("\u0E97\u0EB5\u0EC8"),
       U8("\u0EAA\u0EA7\u0E8D\u0E87\u0EB2\u0EA1")});
}
#endif

#ifdef WITH_RIVE_TEXT_DICT_KHMER
TEST_CASE("line break dictionary splits Khmer into words", "[line break]")
{
  // ខ្ញុំ|ស្រលាញ់|ប្រទេស|កម្ពុជា, subscript consonants stay on their coeng.
  requireWords(
      {U8("\u1781\u17D2\u1789\u17BB\u17C6"),
       U8("\u179F\u17D2\u179A\u179B\u17B6\u1789\u17CB"),
       U8("\u1794\u17D2\u179A\u1791\u17C1\u179F"),
       U8("\u1780\u1798\u17D2\u1796\u17BB\u1787\u17B6")});
}
#endif

#ifdef WITH_RIVE_TEXT_DICT_BURMESE
TEST_CASE("line break dictionary splits Burmese into words", "[line break]")
{
  // ကျွန်တော်|မြန်မာ|စကားပြော|တတ်|တယ်
  requireWords(
      {U8("\u1000\u103B\u103D\u1014\u103A\u1010\u1031\u102C\u103A"),
       U8("\u1019\u103C\u1014\u103A\u1019\u102C"),
       U8("\u1005\u1000\u102C\u1038\u1015\u103C\u1031\u102C"),
       U8("\u1010\u1010\u103A"),
       U8("\u1010\u101A\u103A")});

  // မင်္ဂလာ|ပါ, kinzi stacks across the virama.
  requireWords(
      {U8("\u1019\u1004\u103A\u1039\u1002\u101C\u102C"), U8("\u1015\u102B")});
}
#endif

#ifdef WITH_RIVE_TEXT_DICT_THAI
TEST_CASE("line break dictionary never splits a syllable", "[line break]")
{
  // อภิญญา then a stray SARA A, which can't start a syllable: no break may
  // sit before it, so the word gives way rather than strand the vowel.
  std::string result = segments(
      U8("\u0E2D\u0E20\u0E34\u0E0D\u0E0D\u0E32\u0E30"));
  REQUIRE(result.find(U8("|\u0E30")) == std::string::npos);

  // A leading vowel never ends a segment, even when no word covers it.
  result = segments(U8("\u0E01\u0E34\u0E19\u0E40\u0E40\u0E01\u0E34\u0E19"));
  REQUIRE(result.find(U8("\u0E40|")) == std::string::npos);
}
#endif

#ifdef WITH_RIVE_TEXT_DICT_THAI
TEST_CASE("line break dictionary keeps Thai syllables whole", "[line break]")
{
  // (ัภาษา), the mark attaches to the bracket, so no break may follow it.
  requireWords({U8("\u0028\u0E31\u0E20\u0E32\u0E29\u0E32\u0029")});

  // ถา|จารย์, the consonant under THANTHAKHAT is silent and stays with the
  // syllable before it.
  requireWords({U8("\u0E16\u0E32"), U8("\u0E08\u0E32\u0E23\u0E22\u0E4C")});

  // สทิง|ไซต์ฯ, PAIYANNOI belongs to the word before it.
  requireWords(
      {U8("\u0E2A\u0E17\u0E34\u0E07"), U8("\u0E44\u0E0B\u0E15\u0E4C\u0E2F")});

  // กินดิ์|กิน and กินธุ์|ดี, a vowel above or below a silent
  // consonant is silenced with it, so the consonant still ends the syllable
  // before it when no word covers it.
  requireWords(
      {U8("\u0E01\u0E34\u0E19\u0E14\u0E34\u0E4C"), U8("\u0E01\u0E34\u0E19")});
  requireWords(
      {U8("\u0E01\u0E34\u0E19\u0E18\u0E38\u0E4C"), U8("\u0E14\u0E35")});
}
#endif

#ifdef WITH_RIVE_TEXT_DICT_BURMESE
TEST_CASE(
    "line break dictionary keeps Burmese final consonants",
    "[line break]")
{
  // ဖစ်, asat makes စ the final consonant of ဖ's syllable, though ဖ alone is
  // a word.
  requireWords({U8("\u1016\u1005\u103A")});

  // ရင်ဖွဲ့|ဘော်ထုတ်, after vowel signs asat spells the vowel instead, so ဘ
  // still starts a word.
  requireWords(
      {U8("\u101B\u1004\u103A\u1016\u103D\u1032\u1037"),
       U8("\u1018\u1031\u102C\u103A\u1011\u102F\u1010\u103A")});
}
#endif

#if defined(WITH_RIVE_TEXT_DICT_COMPRESSED) && defined(WITH_RIVE_TEXT_DICT_THAI)
TEST_CASE(
    "line break dictionary inflates every deflate block type",
    "[line break]")
{
  // กก, กข and ขก front coded, as a stored block and as fixed
  // codes. The generator only emits dynamic codes.
  static const uint8_t stored[] =
      {1, 11, 0, 244, 255, 0, 1, 1, 0, 1, 2, 0, 0, 2, 1, 0};
  static const uint8_t fixed[] =
      {99, 96, 100, 100, 96, 100, 98, 96, 96, 98, 100, 0, 0};
  LineBreakDictionary tiny = *lineBreakDictionaryFor(0x0E01);
  tiny.wordsSize = 11;
  tiny.edgeCount = 5;
  tiny.edgeChecksum = 0x5D692FE7;
  tiny.words = Span<const uint8_t>(stored, sizeof(stored));
  REQUIRE(buildLineBreakDictionaryEdges(tiny).size() == 5);
  tiny.words = Span<const uint8_t>(fixed, sizeof(fixed));
  REQUIRE(buildLineBreakDictionaryEdges(tiny).size() == 5);
}

TEST_CASE("line break dictionary rejects corrupt word lists", "[line break]")
{
  // Truncated or with a flipped bit, the word list builds nothing rather
  // than a different trie.
  const LineBreakDictionary& thai = *lineBreakDictionaryFor(0x0E01);
  std::vector<uint8_t> words(thai.words.begin(), thai.words.end());
  LineBreakDictionary copy = thai;
  uint32_t seed = 1;
  for (int i = 0; i < 32; i++)
  {
    std::vector<uint8_t> broken = words;
    seed = seed * 1664525u + 1013904223u;
    if (i % 2 == 0)
    {
      broken.resize(seed % broken.size());
    }
    else
    {
      broken[seed % broken.size()] ^= (uint8_t)(1u << (seed >> 29));
    }
    copy.words = Span<const uint8_t>(broken.data(), broken.size());
    std::vector<uint32_t> edges = buildLineBreakDictionaryEdges(copy);
    REQUIRE((edges.empty() || edges.size() == thai.edgeCount));
  }
}
#endif

TEST_CASE("line break dictionaries match the generator's tries", "[line break]")
{
  // One codepoint per script, so compressed builds rebuild every trie.
  for (uint32_t codepoint : {0x0E01u, 0x0E81u, 0x1780u, 0x1000u})
  {
    const LineBreakDictionary* dictionary = lineBreakDictionaryFor(codepoint);
    if (dictionary == nullptr)
    {
      continue;
    }
    Span<const uint32_t> edges = lineBreakDictionaryEdges(*dictionary);
    REQUIRE(edges.size() == dictionary->edgeCount);
    uint32_t checksum = 2166136261u;
    for (uint32_t edge : edges)
    {
      checksum = (checksum ^ edge) * 16777619u;
    }
    REQUIRE(checksum == dictionary->edgeChecksum);
    // Later calls share the first one's edges.
    REQUIRE(lineBreakDictionaryEdges(*dictionary).data() == edges.data());
  }
}

TEST_CASE("line break dictionaries leave other scripts alone", "[line break]")
{
  REQUIRE(segments("state-of-the-art words") == "state-|of-|the-|art |words");
  REQUIRE(lineBreakDictionaryFor('a') == nullptr);
  // Thai digits and the baht sign keep their own classes.
  REQUIRE(lineBreakDictionaryFor(0x0E50) == nullptr);
  REQUIRE(lineBreakDictionaryFor(0x0E3F) == nullptr);
}
#endif
