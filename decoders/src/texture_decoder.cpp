/*
 * Copyright 2026 Rive
 */

#include "rive/decoders/texture_decoder.hpp"

#include <cstdio>

#ifdef RIVE_ASTC_DECODER
std::unique_ptr<Bitmap> decode_astc_texture(const uint8_t* blocks,
                                            size_t byteCount,
                                            uint32_t width,
                                            uint32_t height,
                                            uint32_t blockWidth,
                                            uint32_t blockHeight);
#endif

#ifdef RIVE_BC_DECODER
std::unique_ptr<Bitmap> decode_bc_texture(const uint8_t* blocks,
                                          size_t byteCount,
                                          uint32_t width,
                                          uint32_t height,
                                          rive::GPUTextureFormat format);
#endif

#ifdef RIVE_ETC_DECODER
std::unique_ptr<Bitmap> decode_etc_texture(const uint8_t* blocks,
                                           size_t byteCount,
                                           uint32_t width,
                                           uint32_t height,
                                           rive::GPUTextureFormat format);
#endif

// Body branches reference these params via #ifdef; mark as maybe-unused so
// no-decoder-compiled-in builds don't warn.
std::unique_ptr<Bitmap> decode_texture([[maybe_unused]] const uint8_t* blocks,
                                       [[maybe_unused]] size_t byteCount,
                                       [[maybe_unused]] uint32_t width,
                                       [[maybe_unused]] uint32_t height,
                                       rive::GPUTextureFormat format,
                                       [[maybe_unused]] uint32_t blockWidth,
                                       [[maybe_unused]] uint32_t blockHeight)
{

    switch (format)
    {
        case rive::GPUTextureFormat::astc:
#ifdef RIVE_ASTC_DECODER
            return decode_astc_texture(blocks,
                                       byteCount,
                                       width,
                                       height,
                                       blockWidth,
                                       blockHeight);
#else
            fprintf(stderr,
                    "ASTC texture not supported "
                    "(build with --with_rive_astc_decoder)\n");
            return nullptr;
#endif

        case rive::GPUTextureFormat::bc1:
        case rive::GPUTextureFormat::bc2:
        case rive::GPUTextureFormat::bc3:
        case rive::GPUTextureFormat::bc7:
#ifdef RIVE_BC_DECODER
            return decode_bc_texture(blocks, byteCount, width, height, format);
#else
            fprintf(stderr,
                    "BC texture not supported "
                    "(build with --with_rive_bc_decoder)\n");
            return nullptr;
#endif

        case rive::GPUTextureFormat::etc2:
#ifdef RIVE_ETC_DECODER
            return decode_etc_texture(blocks, byteCount, width, height, format);
#else
            fprintf(stderr,
                    "ETC texture not supported "
                    "(build with --with_rive_etc_decoder)\n");
            return nullptr;
#endif

        default:
            fprintf(stderr,
                    "decode_texture - unsupported format %u\n",
                    static_cast<unsigned>(format));
            return nullptr;
    }
}
