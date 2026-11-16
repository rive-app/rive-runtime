/*
 * Copyright 2026 Rive
 */

// Helpers from common.glsl, compiled as C++.

#include <catch.hpp>
#include <cstdint>

// (Last: cpp.glsl's macros would break the headers above.)
#define RIVE_GLSL_CROSS_WANT_CONSTRUCTORS
#include "cpp.glsl"
namespace common_glsl
{
RIVE_GLSL_CROSS_CONSTRUCTORS
#define GLSL
#include "generated/shaders/constants.minified.glsl"
#include "generated/shaders/common.minified.glsl"
} // namespace common_glsl

TEST_CASE("isExactlyOneFlagSet", "[shader_common]")
{
  // Every pair of distinct single bits, against every combination of the two,
  // with unrelated bits set and clear.
  for (uint32_t i = 0; i < 32; ++i)
  {
    for (uint32_t j = 0; j < 32; ++j)
    {
      if (i == j)
      {
        continue;
      }
      const uint32_t flag0 = 1u << i;
      const uint32_t flag1 = 1u << j;
      const uint32_t others = ~(flag0 | flag1);
      for (uint32_t noise : {0u, others, others & 0x5555aaaau})
      {
        for (uint32_t set : {0u, flag0, flag1, flag0 | flag1})
        {
          const uint32_t flags = noise | set;
          const bool expected = ((flags & flag0) != 0) !=
                                ((flags & flag1) != 0);
          CHECK(
              common_glsl::isExactlyOneFlagSet(flags, flag0, flag1) ==
              expected);
        }
      }
    }
  }
}
