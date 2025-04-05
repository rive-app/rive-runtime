/*
 * Copyright 2023 Rive
 */

#include "rive/decoders/bitmap_decoder.hpp"
#include "rive/rive_types.hpp"
#include "rive/math/simd.hpp"
#include <stdio.h>
#include <string.h>

Bitmap::Bitmap(uint32_t width,
               uint32_t height,
               PixelFormat pixelFormat,
               std::unique_ptr<const uint8_t[]> bytes) :
    m_Width(width),
    m_Height(height),
    m_PixelFormat(pixelFormat),
    m_Bytes(std::move(bytes))
{}

Bitmap::Bitmap(uint32_t width,
               uint32_t height,
               PixelFormat pixelFormat,
               const uint8_t* bytes) :
    Bitmap(width, height, pixelFormat, std::unique_ptr<const uint8_t[]>(bytes))
{}

static size_t bytes_per_pixel(Bitmap::PixelFormat format)
{
    switch (format)
    {
        case Bitmap::PixelFormat::RGB:
            return 3;
        case Bitmap::PixelFormat::RGBA:
        case Bitmap::PixelFormat::RGBAPremul:
            return 4;
    }
    RIVE_UNREACHABLE();
}

void Bitmap::pixelFormat(PixelFormat format)
{
    if (format == m_PixelFormat)
    {
        return;
    }

    size_t imageNumPixels = m_Height * m_Width;
    size_t fromBytesPerPixel = bytes_per_pixel(m_PixelFormat);
    size_t toBytesPerPixel = bytes_per_pixel(format);
    size_t toSizeInBytes = imageNumPixels * toBytesPerPixel;

    // Round our allocation up to a multiple of 8 so we can premultiply two
    // pixels at once if needed.
    size_t allocSize = (toSizeInBytes + 7) & ~7llu;
    assert(allocSize >= toSizeInBytes && allocSize <= toSizeInBytes + 7);
    auto toBytes = std::unique_ptr<uint8_t[]>(new uint8_t[allocSize]);

    // Copy into the new pixel buffer.
    int writeIndex = 0;
    int readIndex = 0;
    for (size_t i = 0; i < imageNumPixels; i++)
    {
        for (size_t j = 0; j < toBytesPerPixel; j++)
        {
            toBytes[writeIndex++] =
                j < fromBytesPerPixel ? m_Bytes[readIndex++] : 255;
        }
    }

    if (format == PixelFormat::RGBAPremul)
    {
        // Convert to premultiplied alpha.
        for (size_t i = 0; i < toSizeInBytes; i += 8)
        {
            // Load 2 pixels into 64 bits. We overallocated our buffer to ensure
            // we could always premultiply in twos.
            assert(i + 8 <= allocSize);
            rive::uint8x8 twoPx = rive::simd::load<uint8_t, 8>(&toBytes[i]);
            uint8_t a0 = twoPx[3];
            uint8_t a1 = twoPx[7];
            // Don't premultiply fully opaque pixels.
            if (a0 != 255 || a1 != 255)
            {
                // Cast to 16 bits to avoid overflow.
                rive::uint16x8 widePx = rive::simd::cast<uint16_t>(twoPx);
                // Multiply by alpha.
                rive::uint16x8 alpha = {a0, a0, a0, 255, a1, a1, a1, 255};
                widePx = rive::simd::div255(widePx * alpha);
                // Cast back to 8 bits and store.
                twoPx = rive::simd::cast<uint8_t>(widePx);
                rive::simd::store(&toBytes[i], twoPx);
            }
        }
    }
    else
    {
        // Unmultiplying alpha is not currently supported.
        assert(m_PixelFormat != PixelFormat::RGBAPremul);
    }

    m_Bytes = std::move(toBytes);
    m_PixelFormat = format;
}
