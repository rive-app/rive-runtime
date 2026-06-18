/*
 * Copyright 2026 Rive
 */

// Parse-only KTX2 reader for Rive runtime.
//
// Extracts vkFormat, logical pixel dims, and mip-level block payloads from a
// KTX2 container. Does NOT decompress — block data is returned verbatim so
// the renderer can hand it straight to the GPU's compressed texture upload
// path. Today only BC7 (UNORM / SRGB) is recognized; other vkFormat values
// are rejected.
//
// Spec: https://registry.khronos.org/KTX/specs/2.0/ktxspec.v2.html

#include "rive/decoders/decode_ktx2.hpp"
#include "rive/decoders/astc_footprints.hpp"
#include "rive/decoders/texture_decoder.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace rive
{
namespace
{
constexpr uint8_t Ktx2Identifier[12] = {
    0xAB,
    0x4B,
    0x54,
    0x58,
    0x20,
    0x32,
    0x30,
    0xBB,
    0x0D,
    0x0A,
    0x1A,
    0x0A,
};

constexpr uint32_t VK_FORMAT_BC7_UNORM_BLOCK = 145;
constexpr uint32_t VK_FORMAT_BC7_SRGB_BLOCK = 146;
constexpr uint32_t VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK = 151;
constexpr uint32_t VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK = 152;
// ASTC block range endpoints; the body maps the family by index between them.
constexpr uint32_t VK_FORMAT_ASTC_4x4_UNORM_BLOCK = 157;
constexpr uint32_t VK_FORMAT_ASTC_12x12_SRGB_BLOCK = 184;
constexpr uint32_t SupercompressionNone = 0;

#pragma pack(push, 1)
struct Ktx2Header
{
    uint32_t vkFormat;
    uint32_t typeSize;
    uint32_t pixelWidth;
    uint32_t pixelHeight;
    uint32_t pixelDepth;
    uint32_t layerCount;
    uint32_t faceCount;
    uint32_t levelCount;
    uint32_t supercompressionScheme;
    uint32_t dfdByteOffset;
    uint32_t dfdByteLength;
    uint32_t kvdByteOffset;
    uint32_t kvdByteLength;
    uint64_t sgdByteOffset;
    uint64_t sgdByteLength;
};
struct Ktx2LevelIndex
{
    uint64_t byteOffset;
    uint64_t byteLength;
    uint64_t uncompressedByteLength;
};
#pragma pack(pop)
static_assert(sizeof(Ktx2Header) == 68, "KTX2 header must be 68 bytes");
static_assert(sizeof(Ktx2LevelIndex) == 24,
              "KTX2 level index entry must be 24 bytes");

constexpr uint32_t MaxDimension = 16384;
constexpr uint32_t MaxLevels = 16;

inline uint64_t expectedBlockBytes(uint32_t pixelWidth,
                                   uint32_t pixelHeight,
                                   uint32_t blockWidth,
                                   uint32_t blockHeight,
                                   uint32_t bytesPerBlock)
{
    const uint64_t blocksX = (pixelWidth + blockWidth - 1u) / blockWidth;
    const uint64_t blocksY = (pixelHeight + blockHeight - 1u) / blockHeight;
    return blocksX * blocksY * bytesPerBlock;
}
} // namespace

bool DecodeKtx2(const uint8_t* bytes,
                size_t byteCount,
                Ktx2DecodeResult& out,
                const Ktx2HwSupport& hwSupport)
{
    if (byteCount < sizeof(Ktx2Identifier) + sizeof(Ktx2Header))
    {
        std::fprintf(stderr, "DecodeKtx2: file too small\n");
        return false;
    }
    if (std::memcmp(bytes, Ktx2Identifier, sizeof(Ktx2Identifier)) != 0)
    {
        std::fprintf(stderr, "DecodeKtx2: bad magic\n");
        return false;
    }

    Ktx2Header header;
    std::memcpy(&header, bytes + sizeof(Ktx2Identifier), sizeof(header));

    // Map vkFormat → (GPUTextureFormat, blockWidth, blockHeight,
    // bytesPerBlock, srgb).
    GPUTextureFormat outFormat;
    uint8_t blockWidth = 4;
    uint8_t blockHeight = 4;
    uint32_t bytesPerBlock = 16;
    bool srgb = false;
    if (header.vkFormat == VK_FORMAT_BC7_UNORM_BLOCK ||
        header.vkFormat == VK_FORMAT_BC7_SRGB_BLOCK)
    {
        outFormat = GPUTextureFormat::bc7;
        srgb = (header.vkFormat == VK_FORMAT_BC7_SRGB_BLOCK);
    }

    else if (header.vkFormat == VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK ||
             header.vkFormat == VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK)
    {
        outFormat = GPUTextureFormat::etc2;
        // ETC2 RGBA8 = 8 bytes EAC alpha + 8 bytes ETC2 RGB = 16 per block.
        bytesPerBlock = 16;
        srgb = (header.vkFormat == VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK);
    }

    else if (header.vkFormat >= VK_FORMAT_ASTC_4x4_UNORM_BLOCK &&
             header.vkFormat <= VK_FORMAT_ASTC_12x12_SRGB_BLOCK)
    {
        const uint32_t idx =
            (header.vkFormat - VK_FORMAT_ASTC_4x4_UNORM_BLOCK) / 2u;
        outFormat = GPUTextureFormat::astc;

        blockWidth = AstcFootprints[idx].width;
        blockHeight = AstcFootprints[idx].height;
        srgb = (header.vkFormat % 2u) == 0u; // SRGB at UNORM+1.
    }
    else
    {
        std::fprintf(stderr,
                     "DecodeKtx2: unsupported vkFormat %u\n",
                     header.vkFormat);
        return false;
    }

    if (header.supercompressionScheme != SupercompressionNone)
    {
        std::fprintf(stderr,
                     "DecodeKtx2: supercompressionScheme %u not supported\n",
                     header.supercompressionScheme);
        return false;
    }
    if (header.faceCount != 1 || header.layerCount != 0)
    {
        std::fprintf(
            stderr,
            "DecodeKtx2: cubemaps/arrays not supported (faces=%u layers=%u)\n",
            header.faceCount,
            header.layerCount);
        return false;
    }
    if (header.pixelWidth == 0 || header.pixelWidth > MaxDimension ||
        header.pixelHeight == 0 || header.pixelHeight > MaxDimension)
    {
        std::fprintf(stderr,
                     "DecodeKtx2: dimensions out of range (%ux%u)\n",
                     header.pixelWidth,
                     header.pixelHeight);
        return false;
    }

    const uint32_t levelCount = header.levelCount == 0 ? 1u : header.levelCount;
    if (levelCount > MaxLevels)
    {
        std::fprintf(stderr,
                     "DecodeKtx2: levelCount %u exceeds cap %u\n",
                     levelCount,
                     MaxLevels);
        return false;
    }

    const size_t levelIndexOffset = sizeof(Ktx2Identifier) + sizeof(Ktx2Header);
    const size_t levelIndexBytes =
        static_cast<size_t>(levelCount) * sizeof(Ktx2LevelIndex);
    if (byteCount < levelIndexOffset + levelIndexBytes)
    {
        std::fprintf(stderr, "DecodeKtx2: truncated level index\n");
        return false;
    }

    // Read level entries (level 0 first in the index = largest mip).
    std::vector<Ktx2LevelIndex> entries(levelCount);
    std::memcpy(entries.data(), bytes + levelIndexOffset, levelIndexBytes);

    // Validate each level + sum total payload size for the output buffer.
    uint64_t totalBytes = 0;
    for (uint32_t i = 0; i < levelCount; ++i)
    {
        const Ktx2LevelIndex& e = entries[i];
        const uint32_t logW = std::max<uint32_t>(1u, header.pixelWidth >> i);
        const uint32_t logH = std::max<uint32_t>(1u, header.pixelHeight >> i);
        const uint64_t expected = expectedBlockBytes(logW,
                                                     logH,
                                                     blockWidth,
                                                     blockHeight,
                                                     bytesPerBlock);
        if (e.byteLength != expected)
        {
            std::fprintf(
                stderr,
                "DecodeKtx2: level %u byteLength %llu != block grid %llu "
                "for %ux%u\n",
                i,
                static_cast<unsigned long long>(e.byteLength),
                static_cast<unsigned long long>(expected),
                logW,
                logH);
            return false;
        }
        if (e.byteOffset > byteCount || e.byteLength > byteCount - e.byteOffset)
        {
            std::fprintf(stderr, "DecodeKtx2: level %u out of buffer\n", i);
            return false;
        }
        totalBytes += e.byteLength;
    }

    // Concatenate level 0 .. N-1 into one contiguous buffer (largest first).
    out.format = outFormat;
    out.pixelWidth = header.pixelWidth;
    out.pixelHeight = header.pixelHeight;
    out.levelCount = levelCount;
    out.blockWidth = blockWidth;
    out.blockHeight = blockHeight;
    out.srgb = srgb;
    out.softwareDecoded = false;
    out.blocks.resize(static_cast<size_t>(totalBytes));
    size_t writeOffset = 0;
    for (uint32_t i = 0; i < levelCount; ++i)
    {
        const Ktx2LevelIndex& e = entries[i];
        std::memcpy(out.blocks.data() + writeOffset,
                    bytes + e.byteOffset,
                    static_cast<size_t>(e.byteLength));
        writeOffset += static_cast<size_t>(e.byteLength);
    }

    // HW-cap fallback: if the backend can't sample this format directly,
    // software-decode mip 0 to RGBA8 in place. Caller uploads as rgba32.
    bool needFallback = false;
    switch (out.format)
    {
        case GPUTextureFormat::bc1:
        case GPUTextureFormat::bc2:
        case GPUTextureFormat::bc3:
        case GPUTextureFormat::bc7:
            needFallback = !hwSupport.bc;
            break;
        case GPUTextureFormat::astc:
            needFallback = !hwSupport.astc;
            break;
        case GPUTextureFormat::etc2:
            needFallback = !hwSupport.etc2;
            break;
        default:
            break;
    }
    if (needFallback)
    {
        // Decode every level. Source layout: level 0 first, levels tight
        // (matches how we just wrote `out.blocks`). Output layout: each
        // level's logical width * height * 4 bytes, also tight.
        //
        // We allocate the decoded chain into a temp buffer first so the
        // original block bytes remain valid for the per-level decode calls.
        std::vector<uint8_t> decoded;
        size_t totalRgba = 0;
        for (uint32_t i = 0; i < levelCount; ++i)
        {

            const uint32_t logW = std::max<uint32_t>(1u, out.pixelWidth >> i);
            const uint32_t logH = std::max<uint32_t>(1u, out.pixelHeight >> i);

            totalRgba += static_cast<size_t>(logW) * logH * 4;
        }
        decoded.reserve(totalRgba);

        size_t srcOffset = 0;
        for (uint32_t i = 0; i < levelCount; ++i)
        {

            const uint32_t logW = std::max<uint32_t>(1u, out.pixelWidth >> i);
            const uint32_t logH = std::max<uint32_t>(1u, out.pixelHeight >> i);
            const size_t levelBytes =
                static_cast<size_t>(entries[i].byteLength);
            auto bmp = decode_texture(out.blocks.data() + srcOffset,
                                      levelBytes,
                                      logW,
                                      logH,
                                      out.format,
                                      out.blockWidth,
                                      out.blockHeight);
            if (!bmp)
            {
                std::fprintf(stderr,
                             "DecodeKtx2: HW lacks support for format %u "
                             "and software decoder unavailable (level %u)\n",
                             static_cast<unsigned>(out.format),
                             i);
                return false;
            }
            // Match the PNG runtime path: premultiplied texels.
            bmp->pixelFormat(Bitmap::PixelFormat::RGBAPremul);
            decoded.insert(decoded.end(),
                           bmp->bytes(),
                           bmp->bytes() + bmp->numBytes());
            srcOffset += levelBytes;
        }

        out.blocks = std::move(decoded);
        out.format = GPUTextureFormat::rgba32;

        out.blockWidth = 1;
        out.blockHeight = 1;
        out.srgb = false;
        out.softwareDecoded = true;
        // `out.levelCount` already matches the KTX2 level count.
    }

    return true;
}

} // namespace rive
