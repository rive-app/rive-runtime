/*
 * Copyright 2026 Rive
 */

#include "rive/decoders/bitmap_decoder.hpp"
#include "rive/gpu_texture_format.hpp"

// bc7decomp provides BC7 software decompression.
// rgbcx provides BC1/BC2/BC3 software decompression.
#include "bc7decomp.h"
#include "rgbcx.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <memory>

std::unique_ptr<Bitmap> decode_bc_texture(const uint8_t* blocks,
                                          size_t /*byteCount*/,
                                          uint32_t width,
                                          uint32_t height,
                                          rive::GPUTextureFormat format)
{
    // All BCn formats use 4x4 pixel blocks.
    const uint32_t blocksX = (width + 3) / 4;
    const uint32_t blocksY = (height + 3) / 4;

    const size_t pixelCount = static_cast<size_t>(width) * height;
    auto pixels = std::make_unique<uint8_t[]>(pixelCount * 4);
    memset(pixels.get(), 0, pixelCount * 4);

    const uint8_t* src = blocks;

    for (uint32_t by = 0; by < blocksY; by++)
    {
        for (uint32_t bx = 0; bx < blocksX; bx++)
        {
            // Decode one 4x4 block into a temporary 16-pixel RGBA buffer.
            // uint32_t storage gives the bc7decomp/rgbcx union casts proper
            // alignment (required on ARM) and lets the copy below move one
            // pixel per assignment.
            uint32_t blockPixels[16] = {};

            switch (format)
            {
                case rive::GPUTextureFormat::bc7:
                    bc7decomp::unpack_bc7(
                        src,
                        reinterpret_cast<bc7decomp::color_rgba*>(blockPixels));
                    src += 16;
                    break;

                case rive::GPUTextureFormat::bc1:
                    rgbcx::unpack_bc1(
                        src,
                        reinterpret_cast<rgbcx::color32*>(blockPixels),
                        true);
                    src += 8;
                    break;

                case rive::GPUTextureFormat::bc3:
                    rgbcx::unpack_bc3(
                        src,
                        reinterpret_cast<rgbcx::color32*>(blockPixels));
                    src += 16;
                    break;

                default:
                    fprintf(stderr,
                            "DecodeBcTexture - unsupported BC format %u\n",
                            static_cast<unsigned>(format));
                    return nullptr;
            }

            // Copy decoded pixels into the output image. The last block
            // row/column may extend past the image edge — clamp via a
            // precomputed pixel count so each loop has a single exit.
            uint32_t* dst32 = reinterpret_cast<uint32_t*>(pixels.get());
            const uint32_t copyW = std::min<uint32_t>(4u, width - bx * 4);
            const uint32_t copyH = std::min<uint32_t>(4u, height - by * 4);
            for (uint32_t py = 0; py < copyH; py++)
            {
                const uint32_t dstY = by * 4 + py;
                for (uint32_t px = 0; px < copyW; px++)
                {
                    const uint32_t dstX = bx * 4 + px;
                    dst32[dstY * width + dstX] = blockPixels[py * 4 + px];
                }
            }
        }
    }

    const size_t numBytes = static_cast<size_t>(width) * height * 4;
    return std::make_unique<Bitmap>(width,
                                    height,
                                    numBytes,
                                    Bitmap::PixelFormat::RGBA,
                                    std::move(pixels));
}
