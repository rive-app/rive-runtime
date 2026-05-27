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

// HW support flags for the formats KTX2 may contain. When a format is not
// supported by the backend, DecodeKtx2 will software-decode mip 0 to RGBA8
// (if the corresponding RIVE_*_DECODER family was compiled in) and store
// the result in `blocks` with `format = rgba32`.
// Defaults assume the caller's backend natively supports every GPU
// compressed format we recognise. Callers that have actually queried HW
// caps should set the relevant booleans to false to opt the parser into
// the software-decode fallback. Tests + parser-only consumers can use the
// defaults to keep the original "pass blocks through verbatim" behavior.
struct Ktx2HwSupport
{
    bool bc = true;
    bool astc = true;
    bool etc2 = true;
};

// Result of parsing a KTX2 container. Block data is held in a contiguous
// owned buffer, level 0 first (largest), then level 1, … level N-1
// (smallest). Each level's region is exactly its block-grid size in bytes
// (no inter-level padding).
//
// If `softwareDecoded` is true, the GPU format in the container was not
// supported by the caller's backend and mip 0 was decoded to RGBA8. In that
// case `format == rgba32`, `levelCount == 1`, and `blocks` holds tightly
// packed RGBA8 pixels.
struct Ktx2DecodeResult
{
    GPUTextureFormat format;
    uint32_t pixelWidth;     // logical mip 0 width
    uint32_t pixelHeight;    // logical mip 0 height
    uint32_t levelCount;     // number of mip levels stored (>=1)
    uint8_t blockWidth = 4;  // compressed block footprint width (1 for rgba32)
    uint8_t blockHeight = 4; // compressed block footprint height (1 for rgba32)
    bool srgb = false;       // sRGB colour space (BC7_SRGB / ASTC_SRGB)
    std::vector<uint8_t> blocks;
    bool softwareDecoded = false;
};

// Parses a KTX2 container. Returns true on success and fills `out`. Returns
// false (with a stderr log) on:
//   - bad magic, truncated header / level index
//   - unsupported vkFormat (today: only BC7 UNORM/SRGB)
//   - non-NONE supercompressionScheme
//   - cubemaps / array layers
//   - oversized dimensions or level count
//   - level data outside the buffer
//
// `hwSupport` is consulted after parsing to decide whether to fall back to
// CPU decompression. Pass all-true to skip the fallback path entirely.
bool DecodeKtx2(const uint8_t* bytes,
                size_t byteCount,
                Ktx2DecodeResult& out,
                const Ktx2HwSupport& hwSupport = {});

} // namespace rive

#endif
