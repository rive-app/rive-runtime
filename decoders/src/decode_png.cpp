/*
 * Copyright 2023 Rive
 */

#include "rive/decoders/bitmap_decoder.hpp"
#include "png.h"
#include <algorithm>
#include <cassert>
#include <string.h>

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

    png_set_strip_16(png_ptr);

    int bitDepth = 0;
    if (color_type == PNG_COLOR_TYPE_PALETTE || color_type == PNG_COLOR_TYPE_RGB)
    {
        png_set_expand(png_ptr);
        bitDepth = 24;
        if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
        {
            png_set_expand(png_ptr);
            bitDepth += 8;
        }
    }
    else if (color_type == PNG_COLOR_TYPE_GRAY)
    {
        png_set_expand(png_ptr);
        bitDepth = 8;
    }
    else if (color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
    {
        png_set_expand(png_ptr);
        png_set_gray_to_rgb(png_ptr);
        bitDepth = 32;
    }
    else if (color_type == PNG_COLOR_TYPE_RGB_ALPHA)
    {
        png_set_expand(png_ptr);
        bitDepth = 32;
    }

    int pixelBytes = bitDepth / 8;
    auto pixelBuffer = new uint8_t[width * height * pixelBytes];

    png_bytep* row_pointers = new png_bytep[height];

    for (unsigned row = 0; row < height; row++)
    {
        unsigned int rIndex = row;
        // if (flipY) {
        //     rIndex = height - row - 1;
        // }
        row_pointers[rIndex] = pixelBuffer + (row * (width * pixelBytes));
    }
    png_read_image(png_ptr, row_pointers);
    png_read_end(png_ptr, info_ptr);

    png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp) nullptr);

    delete[] row_pointers;

    Bitmap::PixelFormat pixelFormat;
    assert(bitDepth == 32 || bitDepth == 24 || bitDepth == 8);
    switch (bitDepth)
    {
        case 32:
            pixelFormat = Bitmap::PixelFormat::RGBA;
            break;
        case 24:
            pixelFormat = Bitmap::PixelFormat::RGB;
            break;
        case 8:
            pixelFormat = Bitmap::PixelFormat::R;
            break;
    }
    return std::make_unique<Bitmap>(width, height, pixelFormat, pixelBuffer);
}
