/*
 * Copyright 2026 Rive
 */

#include "rive/renderer/ore/ore_types.hpp"
#include <catch.hpp>

namespace rive::ore
{
TEST_CASE("textureFormatBytesPerTexel", "[ore_types]")
{
    // 8-bit formats
    CHECK(textureFormatBytesPerTexel(TextureFormat::r8unorm) == 1);
    CHECK(textureFormatBytesPerTexel(TextureFormat::rg8unorm) == 2);
    CHECK(textureFormatBytesPerTexel(TextureFormat::rgba8unorm) == 4);
    CHECK(textureFormatBytesPerTexel(TextureFormat::rgba8snorm) == 4);
    CHECK(textureFormatBytesPerTexel(TextureFormat::bgra8unorm) == 4);

    // 16-bit float
    CHECK(textureFormatBytesPerTexel(TextureFormat::rgba16float) == 8);
    CHECK(textureFormatBytesPerTexel(TextureFormat::rg16float) == 4);
    CHECK(textureFormatBytesPerTexel(TextureFormat::r16float) == 2);

    // 32-bit float
    CHECK(textureFormatBytesPerTexel(TextureFormat::rgba32float) == 16);
    CHECK(textureFormatBytesPerTexel(TextureFormat::rg32float) == 8);
    CHECK(textureFormatBytesPerTexel(TextureFormat::r32float) == 4);

    // Packed
    CHECK(textureFormatBytesPerTexel(TextureFormat::rgb10a2unorm) == 4);
    CHECK(textureFormatBytesPerTexel(TextureFormat::r11g11b10float) == 4);

    // Depth/stencil
    CHECK(textureFormatBytesPerTexel(TextureFormat::depth16unorm) == 2);
    CHECK(textureFormatBytesPerTexel(TextureFormat::depth24plusStencil8) == 4);
    CHECK(textureFormatBytesPerTexel(TextureFormat::depth32float) == 4);
    CHECK(textureFormatBytesPerTexel(TextureFormat::depth32floatStencil8) == 8);

    // Block-compressed formats return 0
    CHECK(textureFormatBytesPerTexel(TextureFormat::bc1unorm) == 0);
    CHECK(textureFormatBytesPerTexel(TextureFormat::bc3unorm) == 0);
    CHECK(textureFormatBytesPerTexel(TextureFormat::bc7unorm) == 0);
    CHECK(textureFormatBytesPerTexel(TextureFormat::etc2rgb8) == 0);
    CHECK(textureFormatBytesPerTexel(TextureFormat::etc2rgba8) == 0);
    CHECK(textureFormatBytesPerTexel(TextureFormat::astc4x4) == 0);
    CHECK(textureFormatBytesPerTexel(TextureFormat::astc6x6) == 0);
    CHECK(textureFormatBytesPerTexel(TextureFormat::astc8x8) == 0);
}

TEST_CASE("ColorWriteMask-operators", "[ore_types]")
{
    // OR combines flags
    CHECK((ColorWriteMask::red | ColorWriteMask::green) ==
          static_cast<ColorWriteMask>(0x3));
    CHECK((ColorWriteMask::red | ColorWriteMask::blue |
           ColorWriteMask::alpha) == static_cast<ColorWriteMask>(0xD));
    CHECK((ColorWriteMask::none | ColorWriteMask::all) == ColorWriteMask::all);
    CHECK((ColorWriteMask::red | ColorWriteMask::red) == ColorWriteMask::red);

    // AND masks flags
    CHECK((ColorWriteMask::all & ColorWriteMask::red) == ColorWriteMask::red);
    CHECK(((ColorWriteMask::red | ColorWriteMask::green) &
           ColorWriteMask::red) == ColorWriteMask::red);
    CHECK((ColorWriteMask::none & ColorWriteMask::all) == ColorWriteMask::none);
    CHECK((ColorWriteMask::red & ColorWriteMask::green) ==
          ColorWriteMask::none);
}
} // namespace rive::ore
