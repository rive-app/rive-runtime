/*
 * Copyright 2026 Rive
 */

#include "rive/text/line_break.hpp"
#include <catch.hpp>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace rive;

// Runs Unicode's LineBreakTest.txt (comments stripped by
// dev/unicode/generate_line_break.py) through computeLineBreaks. Cases with
// Hangul are skipped: computeLineBreaks tailors Hangul to AL so Korean wraps
// at spaces, while the conformance data breaks between syllables.
TEST_CASE("line break classes match UAX #14 conformance data", "[line break]")
{
  FILE* fp = fopen("assets/unicode/LineBreakTest.txt", "r");
  REQUIRE(fp != nullptr);
  char line[2048];
  int cases = 0;
  int tailored = 0;
  int failures = 0;
  std::string firstFailure;
  while (fgets(line, sizeof(line), fp))
  {
    if (line[0] == '#' || line[0] == '\n')
    {
      continue;
    }
    std::vector<uint32_t> text;
    std::vector<bool> expected; // one per boundary, index 0 is sot
    char* p = line;
    while (*p)
    {
      if ((unsigned char)p[0] == 0xC3 && (unsigned char)p[1] == 0xB7)
      {
        expected.push_back(true); // ÷
        p += 2;
      }
      else if ((unsigned char)p[0] == 0xC3 && (unsigned char)p[1] == 0x97)
      {
        expected.push_back(false); // ×
        p += 2;
      }
      else if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
      {
        p++;
      }
      else
      {
        text.push_back((uint32_t)strtoul(p, &p, 16));
      }
    }
    REQUIRE(expected.size() == text.size() + 1);
    cases++;
    bool isTailored = false;
    for (uint32_t cp : text)
    {
      LineBreakClass cls = lineBreakProps(cp).cls;
      if (cls == LineBreakClass::JL || cls == LineBreakClass::JV ||
          cls == LineBreakClass::JT || cls == LineBreakClass::H2 ||
          cls == LineBreakClass::H3)
      {
        isTailored = true;
        break;
      }
    }
    if (isTailored)
    {
      tailored++;
      continue;
    }
    std::vector<LineBreak> out(text.size() + 1);
    computeLineBreaks(text, out);
    bool ok = true;
    for (size_t i = 1; i < expected.size(); i++)
    {
      bool actual = out[i] != LineBreak::none;
      if (actual != expected[i])
      {
        ok = false;
      }
    }
    if (!ok)
    {
      failures++;
      if (firstFailure.empty())
      {
        firstFailure = line;
      }
    }
  }
  fclose(fp);
  INFO("first failing case: " << firstFailure);
  REQUIRE(cases > 19000);
  REQUIRE(cases - tailored > 16000);
  REQUIRE(failures == 0);
}
