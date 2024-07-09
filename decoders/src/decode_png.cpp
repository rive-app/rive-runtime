/*
 * Copyright 2023 Rive
 */

#include "rive/decoders/bitmap_decoder.hpp"
#include "png.h"
#include <algorithm>
#include <cassert>
#include <string.h>
#include <setjmp.h>

struct EncodedImageBuffer
{
    const uint8_t* bytes;
    size_t position;
    size_t size;
};

static void ReadDataFromMemory(png_structp png_ptr, png_bytep outBytes, png_size_t byteCountToRead)
{
    png_voidp a = png_get_io_ptr(png_ptr);
    if (a == nullptr)
    {
        return;
    }
    EncodedImageBuffer& stream = *(EncodedImageBuffer*)a;

    size_t bytesRead = std::min(byteCountToRead, (stream.size - stream.position));
    memcpy(outBytes, stream.bytes + stream.position, bytesRead);
    stream.position += bytesRead;

    if ((png_size_t)bytesRead != byteCountToRead)
    {
        // Report image error?
    }
}

std::unique_ptr<Bitmap> DecodePng(const uint8_t bytes[], size_t byteCount)
{
    png_structp png_ptr;
    png_infop info_ptr;
    png_uint_32 width, height;
    int bit_depth, color_type, interlace_type;
    std::unique_ptr<uint8_t[]> pixelBuffer;
    std::unique_ptr<png_bytep[]> rowsPointer;

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);

    if (png_ptr == nullptr)
    {
        printf("DecodePng - libpng failed (png_create_read_struct).");
        return nullptr;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL)
    {
        png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
        printf("DecodePng - libpng failed (png_create_info_struct).");
        return nullptr;
    }

    if (setjmp(png_jmpbuf(png_ptr)))
    {
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        return nullptr;
    }

    EncodedImageBuffer stream = {bytes, 0, byteCount};

    png_set_read_fn(png_ptr, &stream, ReadDataFromMemory);

    png_read_info(png_ptr, info_ptr);

    png_get_IHDR(png_ptr,
                 info_ptr,
                 &width,
                 &height,
                 &bit_depth,
                 &color_type,
                 &interlace_type,
                 NULL,
                 NULL);

    if (color_type == PNG_COLOR_TYPE_PALETTE ||
        (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8))
    {
        // expand to 3 or 4 channels
        png_set_expand(png_ptr);
    }

    png_bytep trns = 0;
    int trnsCount = 0;
    if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
    {
        png_get_tRNS(png_ptr, info_ptr, &trns, &trnsCount, 0);
        png_set_expand(png_ptr);
    }

    if (bit_depth == 16)
    {
        png_set_strip_16(png_ptr);
    }

    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
    {
        png_set_gray_to_rgb(png_ptr);
    }

    png_read_update_info(png_ptr, info_ptr);
    uint8_t channels = png_get_channels(png_ptr, info_ptr);

    size_t pixelBufferSize =
        static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(channels);
    pixelBuffer = std::make_unique<uint8_t[]>(pixelBufferSize);
    const uint8_t* pixelBufferEnd = pixelBuffer.get() + pixelBufferSize;

    rowsPointer = std::make_unique<png_bytep[]>(height);
    png_bytep* rows = rowsPointer.get();
    size_t rowStride = (size_t)width * (size_t)channels;
    uint8_t* rowWrite = pixelBuffer.get();
    for (png_uint_32 row = 0; row < height; row++)
    {
        rows[row] = rowWrite;
        rowWrite += rowStride;
        if (rowWrite > pixelBufferEnd)
        {
            // Read would overflow.
            return nullptr;
        }
    }
    png_read_image(png_ptr, rows);
    png_read_end(png_ptr, info_ptr);

    png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp) nullptr);

    Bitmap::PixelFormat pixelFormat;
    assert(channels == 3 || channels == 4);
    switch (channels)
    {
        case 4:
            pixelFormat = Bitmap::PixelFormat::RGBA;
            break;
        case 3:
            pixelFormat = Bitmap::PixelFormat::RGB;
            break;
    }
    return std::make_unique<Bitmap>(width, height, pixelFormat, std::move(pixelBuffer));
}
