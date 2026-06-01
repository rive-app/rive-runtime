/*
 * Copyright 2026 Rive
 */

#include "rive/decoders/bitmap_decoder.hpp"
#include "astcenc.h"

#include <cstdio>
#include <memory>
#include <vector>

std::unique_ptr<Bitmap> decode_astc_texture(const uint8_t* blocks,
                                            size_t byteCount,
                                            uint32_t width,
                                            uint32_t height,
                                            uint32_t blockWidth,
                                            uint32_t blockHeight)
{
    astcenc_config config;
    astcenc_error err = astcenc_config_init(ASTCENC_PRF_LDR_SRGB,
                                            blockWidth,
                                            blockHeight,
                                            1,
                                            ASTCENC_PRE_MEDIUM,
                                            ASTCENC_FLG_DECOMPRESS_ONLY,
                                            &config);
    if (err != ASTCENC_SUCCESS)
    {
        fprintf(stderr,
                "DecodeAstcTexture - astcenc_config_init failed: %s\n",
                astcenc_get_error_string(err));
        return nullptr;
    }

    astcenc_context* ctx = nullptr;
    err = astcenc_context_alloc(&config, 1, &ctx);
    if (err != ASTCENC_SUCCESS)
    {
        fprintf(stderr,
                "DecodeAstcTexture - astcenc_context_alloc failed: %s\n",
                astcenc_get_error_string(err));
        return nullptr;
    }

    const size_t pixelCount = static_cast<size_t>(width) * height;
    auto pixels = std::make_unique<uint8_t[]>(pixelCount * 4);

    void* slicePtr = pixels.get();
    astcenc_image outImage;
    outImage.dim_x = width;
    outImage.dim_y = height;
    outImage.dim_z = 1;
    outImage.data_type = ASTCENC_TYPE_U8;
    outImage.data = &slicePtr;

    const astcenc_swizzle swizzle = {ASTCENC_SWZ_R,
                                     ASTCENC_SWZ_G,
                                     ASTCENC_SWZ_B,
                                     ASTCENC_SWZ_A};

    err = astcenc_decompress_image(ctx,
                                   blocks,
                                   byteCount,
                                   &outImage,
                                   &swizzle,
                                   0);
    astcenc_context_free(ctx);

    if (err != ASTCENC_SUCCESS)
    {
        fprintf(stderr,
                "DecodeAstcTexture - astcenc_decompress_image failed: %s\n",
                astcenc_get_error_string(err));
        return nullptr;
    }

    const size_t numBytes = static_cast<size_t>(width) * height * 4;
    return std::make_unique<Bitmap>(width,
                                    height,
                                    numBytes,
                                    Bitmap::PixelFormat::RGBA,
                                    std::move(pixels));
}
