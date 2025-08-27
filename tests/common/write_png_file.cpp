/*
 * Copyright 2022 Rive
 */

#include "write_png_file.hpp"

#include "png.h"
#include "zlib.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

void WritePNGFile(uint8_t* pixels,
                  int width,
                  int height,
                  bool flipY,
                  const char* file_name,
                  PNGCompression compression)
{
    FILE* fp = fopen(file_name, "wb");
    if (!fp)
    {
        fprintf(stderr,
                "WritePNGFile: File %s could not be opened for writing "
                "(errno=%i)\n",
                file_name,
                errno);
        abort();
    }

    auto png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    if (!png)
    {
        fprintf(stderr, "WritePNGFile: png_create_write_struct failed\n");
        abort();
    }

    if (compression == PNGCompression::fast_rle)
    {
        // RLE with SUB gets best performance with our content.
        png_set_compression_strategy(png, Z_RLE);
        png_set_filter(png, 0, PNG_FILTER_SUB);
    }

    auto info = png_create_info_struct(png);
    if (!info)
    {
        fprintf(stderr, "WritePNGFile: png_create_info_struct failed\n");
        abort();
    }

    if (setjmp(png_jmpbuf(png)))
    {
        fprintf(stderr, "WritePNGFile: Error during init_io\n");
        abort();
    }

    png_init_io(png, fp);

    // Write header.
    if (setjmp(png_jmpbuf(png)))
    {
        fprintf(stderr, "WritePNGFile: Error during writing header\n");
        abort();
    }

    png_set_IHDR(png,
                 info,
                 width,
                 height,
                 8,
                 PNG_COLOR_TYPE_RGB_ALPHA,
                 PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE,
                 PNG_FILTER_TYPE_BASE);

    png_write_info(png, info);

    // Write bytes.
    if (setjmp(png_jmpbuf(png)))
    {
        fprintf(stderr, "WritePNGFile: Error during writing bytes\n");
        abort();
    }

    std::vector<uint8_t*> rows(height);
    for (int y = 0; y < height; ++y)
    {
        rows[y] = pixels + (flipY ? height - 1 - y : y) * width * 4;
    }
    png_write_image(png, rows.data());

    // End write.
    if (setjmp(png_jmpbuf(png)))
    {
        fprintf(stderr, "WritePNGFile: Error during end of write");
        abort();
    }

    png_write_end(png, NULL);
    png_destroy_write_struct(&png, &info);

    fclose(fp);
}

static void append_png_data_chunk(png_structp png,
                                  png_bytep data,
                                  png_size_t length)
{
    auto encodedData =
        reinterpret_cast<std::vector<uint8_t>*>(png_get_io_ptr(png));
    encodedData->insert(encodedData->end(), data, data + length);
}

static void flush_png_data(png_structp png) {}

std::vector<uint8_t> EncodePNGToBuffer(uint32_t width,
                                       uint32_t height,
                                       uint8_t* imageDataRGBA,
                                       PNGCompression compression)
{
    // Send the image name.
    auto png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    if (!png)
    {
        fprintf(stderr, "WritePNGFile: png_create_write_struct failed\n");
        abort();
    }

    if (compression == PNGCompression::fast_rle)
    {
        // RLE with SUB gets best performance with our content.
        png_set_compression_level(png, 6);
        png_set_compression_strategy(png, Z_RLE);
        png_set_compression_strategy(png, Z_RLE);
        png_set_filter(png, 0, PNG_FILTER_SUB);
    }

    auto info = png_create_info_struct(png);
    if (!info)
    {
        fprintf(stderr, "WritePNGFile: png_create_info_struct failed\n");
        abort();
    }

    if (setjmp(png_jmpbuf(png)))
    {
        fprintf(stderr, "WritePNGFile: Error during init_io\n");
        abort();
    }

    std::vector<uint8_t> encodedData;
    png_set_write_fn(png,
                     &encodedData,
                     &append_png_data_chunk,
                     &flush_png_data);

    // Write header.
    if (setjmp(png_jmpbuf(png)))
    {
        fprintf(stderr, "WritePNGFile: Error during writing header\n");
        abort();
    }

    png_set_IHDR(png,
                 info,
                 width,
                 height,
                 8,
                 PNG_COLOR_TYPE_RGB_ALPHA,
                 PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE,
                 PNG_FILTER_TYPE_BASE);

    png_write_info(png, info);

    // Write bytes.
    if (setjmp(png_jmpbuf(png)))
    {
        fprintf(stderr, "WritePNGFile: Error during writing bytes\n");
        abort();
    }

    std::vector<uint8_t*> rows(height);
    for (uint32_t y = 0; y < height; ++y)
    {
        rows[y] = imageDataRGBA + (height - 1 - y) * width * 4;
    }
    png_write_image(png, rows.data());

    // End write.
    if (setjmp(png_jmpbuf(png)))
    {
        fprintf(stderr, "WritePNGFile: Error during end of write");
        abort();
    }

    png_write_end(png, NULL);
    png_destroy_write_struct(&png, &info);

    return encodedData;
}
