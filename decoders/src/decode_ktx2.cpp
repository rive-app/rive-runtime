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

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace rive
{
namespace
{
constexpr uint8_t kKtx2Identifier[12] = {
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

constexpr uint32_t kSupercompressionNone = 0;

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

// Defensive caps.
constexpr uint32_t kMaxDimension = 16384;
constexpr uint32_t kMaxLevels = 16;

constexpr uint32_t kBc7BlockBytes = 16;

// Expected block-grid byte length for a BC7 mip level at the given logical
// pixel dimensions. BC7 = 4x4 blocks, 16 bytes/block.
inline uint64_t expectedBc7Bytes(uint32_t pixelWidth, uint32_t pixelHeight)
{
    const uint64_t blocksX = (pixelWidth + 3u) / 4u;
    const uint64_t blocksY = (pixelHeight + 3u) / 4u;
    return blocksX * blocksY * kBc7BlockBytes;
}
} // namespace

bool DecodeKtx2(const uint8_t* bytes, size_t byteCount, Ktx2DecodeResult& out)
{
    if (byteCount < sizeof(kKtx2Identifier) + sizeof(Ktx2Header))
    {
        std::fprintf(stderr, "DecodeKtx2: file too small\n");
        return false;
    }
    if (std::memcmp(bytes, kKtx2Identifier, sizeof(kKtx2Identifier)) != 0)
    {
        std::fprintf(stderr, "DecodeKtx2: bad magic\n");
        return false;
    }

    Ktx2Header header;
    std::memcpy(&header, bytes + sizeof(kKtx2Identifier), sizeof(header));

    if (header.vkFormat != VK_FORMAT_BC7_UNORM_BLOCK &&
        header.vkFormat != VK_FORMAT_BC7_SRGB_BLOCK)
    {
        std::fprintf(stderr,
                     "DecodeKtx2: unsupported vkFormat %u (only BC7 wired)\n",
                     header.vkFormat);
        return false;
    }
    if (header.supercompressionScheme != kSupercompressionNone)
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
    if (header.pixelWidth == 0 || header.pixelWidth > kMaxDimension ||
        header.pixelHeight == 0 || header.pixelHeight > kMaxDimension)
    {
        std::fprintf(stderr,
                     "DecodeKtx2: dimensions out of range (%ux%u)\n",
                     header.pixelWidth,
                     header.pixelHeight);
        return false;
    }

    const uint32_t levelCount = header.levelCount == 0 ? 1u : header.levelCount;
    if (levelCount > kMaxLevels)
    {
        std::fprintf(stderr,
                     "DecodeKtx2: levelCount %u exceeds cap %u\n",
                     levelCount,
                     kMaxLevels);
        return false;
    }

    const size_t levelIndexOffset =
        sizeof(kKtx2Identifier) + sizeof(Ktx2Header);
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
        const uint64_t expected = expectedBc7Bytes(logW, logH);
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
    out.format = header.vkFormat == VK_FORMAT_BC7_SRGB_BLOCK
                     ? GPUTextureFormat::bc7
                     : GPUTextureFormat::bc7;
    out.pixelWidth = header.pixelWidth;
    out.pixelHeight = header.pixelHeight;
    out.levelCount = levelCount;
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

    return true;
}

} // namespace rive
