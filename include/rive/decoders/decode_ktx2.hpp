/*
 * Copyright 2026 Rive
 */

#ifndef _RIVE_DECODE_KTX2_HPP_
#define _RIVE_DECODE_KTX2_HPP_

#include "rive/gpu_texture_format.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace rive
{

// Result of parsing a KTX2 container. Block data is held in a contiguous
// owned buffer, level 0 first (largest), then level 1, … level N-1
// (smallest). Each level's region is exactly its block-grid size in bytes
// (no inter-level padding).
struct Ktx2DecodeResult
{
    GPUTextureFormat format;
    uint32_t pixelWidth;  // logical mip 0 width
    uint32_t pixelHeight; // logical mip 0 height
    uint32_t levelCount;  // number of mip levels stored (>=1)
    std::vector<uint8_t> blocks;
};

// Parses a KTX2 container. Returns true on success and fills `out`. Returns
// false (with a stderr log) on:
//   - bad magic, truncated header / level index
//   - unsupported vkFormat (today: only BC7 UNORM/SRGB)
//   - non-NONE supercompressionScheme
//   - cubemaps / array layers
//   - oversized dimensions or level count
//   - level data outside the buffer
bool DecodeKtx2(const uint8_t* bytes, size_t byteCount, Ktx2DecodeResult& out);

} // namespace rive

#endif
