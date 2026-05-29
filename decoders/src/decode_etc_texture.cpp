/*
 * Copyright 2026 Rive
 */

#include "rive/decoders/bitmap_decoder.hpp"
#include "rive/gpu_texture_format.hpp"

#include <cstdio>
#include <cstring>
#include <memory>

// Forward declarations for Ericsson ETCPACK's etcdec.cxx (built as part of
// rive_decoders when RIVE_ETC_DECODER is set). Each entrypoint writes one 4x4
// block into a planar RGBA buffer at byte offsets +0/+1/+2 (RGB) and +3 (A).
extern void decompressBlockETC2c(unsigned int block_part1,
                                 unsigned int block_part2,
                                 unsigned char* img,
                                 int width,
                                 int height,
                                 int startx,
                                 int starty,
                                 int channels);
extern void decompressBlockAlphaC(unsigned char* data,
                                  unsigned char* img,
                                  int width,
                                  int height,
                                  int ix,
                                  int iy,
                                  int channels);

namespace
{
unsigned int readBE32(const uint8_t* p)
{
    return (static_cast<unsigned int>(p[0]) << 24) |
           (static_cast<unsigned int>(p[1]) << 16) |
           (static_cast<unsigned int>(p[2]) << 8) |
           static_cast<unsigned int>(p[3]);
}
} // namespace

// Decodes ETC2 RGBA8 (16 bytes/block: 8 bytes EAC alpha + 8 bytes ETC2 RGB).
std::unique_ptr<Bitmap> decode_etc_texture(const uint8_t* blocks,
                                           size_t byteCount,
                                           uint32_t width,
                                           uint32_t height,
                                           rive::GPUTextureFormat format)
{
    if (format != rive::GPUTextureFormat::etc2 || width == 0 || height == 0)
    {
        return nullptr;
    }

    const uint32_t paddedW = (width + 3u) & ~3u;
    const uint32_t paddedH = (height + 3u) & ~3u;
    const uint32_t blocksX = paddedW / 4u;
    const uint32_t blocksY = paddedH / 4u;
    const size_t expectedBytes = static_cast<size_t>(blocksX) * blocksY * 16u;
    if (byteCount != expectedBytes)
    {
        fprintf(stderr,
                "DecodeEtcTexture - byteCount %zu != expected %zu for %ux%u\n",
                byteCount,
                expectedBytes,
                width,
                height);
        return nullptr;
    }

    const size_t paddedPixels =
        static_cast<size_t>(paddedW) * static_cast<size_t>(paddedH);
    auto padded = std::make_unique<uint8_t[]>(paddedPixels * 4);

    const uint8_t* src = blocks;
    for (uint32_t by = 0; by < blocksY; ++by)
    {
        for (uint32_t bx = 0; bx < blocksX; ++bx)
        {
            const int startX = static_cast<int>(bx * 4u);
            const int startY = static_cast<int>(by * 4u);
            decompressBlockAlphaC(const_cast<uint8_t*>(src),
                                  padded.get() + 3,
                                  static_cast<int>(paddedW),
                                  static_cast<int>(paddedH),
                                  startX,
                                  startY,
                                  4);
            const unsigned int p1 = readBE32(src + 8);
            const unsigned int p2 = readBE32(src + 12);
            decompressBlockETC2c(p1,
                                 p2,
                                 padded.get(),
                                 static_cast<int>(paddedW),
                                 static_cast<int>(paddedH),
                                 startX,
                                 startY,
                                 4);
            src += 16;
        }
    }

    // Crop padded RGBA grid down to (width, height).
    const size_t outPixels =
        static_cast<size_t>(width) * static_cast<size_t>(height);
    auto pixels = std::make_unique<uint8_t[]>(outPixels * 4);
    for (uint32_t y = 0; y < height; ++y)
    {
        std::memcpy(pixels.get() + static_cast<size_t>(y) * width * 4,
                    padded.get() + static_cast<size_t>(y) * paddedW * 4,
                    static_cast<size_t>(width) * 4);
    }

    return std::make_unique<Bitmap>(width,
                                    height,
                                    outPixels * 4,
                                    Bitmap::PixelFormat::RGBA,
                                    std::move(pixels));
}
