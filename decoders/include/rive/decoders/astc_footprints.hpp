/*
 * Copyright 2026 Rive
 */

#ifndef _RIVE_ASTC_FOOTPRINTS_HPP_
#define _RIVE_ASTC_FOOTPRINTS_HPP_

#include <cstdint>

namespace rive
{

// LDR ASTC block footprints in canonical (Vulkan / KHR_ldr) spec order. The
// index into this table also indexes the corresponding GPU enums:
//   VkFormat (UNORM) = VK_FORMAT_ASTC_4x4_UNORM_BLOCK (157) + 2 * idx
//   VkFormat (SRGB)  = UNORM + 1
//   GL enum  (UNORM) = 0x93B0 + idx
//   GL enum  (SRGB)  = 0x93D0 + idx
struct AstcFootprint
{
    uint8_t width;
    uint8_t height;
};

constexpr AstcFootprint AstcFootprints[] = {
    {4, 4},
    {5, 4},
    {5, 5},
    {6, 5},
    {6, 6},
    {8, 5},
    {8, 6},
    {8, 8},
    {10, 5},
    {10, 6},
    {10, 8},
    {10, 10},
    {12, 10},
    {12, 12},
};
constexpr int AstcFootprintCount =
    sizeof(AstcFootprints) / sizeof(AstcFootprints[0]);

// Returns -1 if (blockWidth, blockHeight) is not a recognised LDR ASTC
// footprint.
inline int astcFootprintIndex(uint8_t blockWidth, uint8_t blockHeight)
{
    for (int i = 0; i < AstcFootprintCount; ++i)
    {
        if (AstcFootprints[i].width == blockWidth &&
            AstcFootprints[i].height == blockHeight)
        {
            return i;
        }
    }
    return -1;
}

} // namespace rive

#endif
