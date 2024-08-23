/*
 * Copyright 2023 Rive
 */

#include "rive/decoders/bitmap_decoder.hpp"
#include "rive/rive_types.hpp"
#include <stdio.h>
#include <string.h>
#include <vector>

Bitmap::Bitmap(uint32_t width,
               uint32_t height,
               PixelFormat pixelFormat,
               std::unique_ptr<const uint8_t[]> bytes) :
    m_Width(width), m_Height(height), m_PixelFormat(pixelFormat), m_Bytes(std::move(bytes))
{}

Bitmap::Bitmap(uint32_t width, uint32_t height, PixelFormat pixelFormat, const uint8_t* bytes) :
    Bitmap(width, height, pixelFormat, std::unique_ptr<const uint8_t[]>(bytes))
{}

size_t Bitmap::bytesPerPixel(PixelFormat format) const
{
    switch (format)
    {
        case PixelFormat::RGB:
            return 3;
        case PixelFormat::RGBA:
            return 4;
    }
    RIVE_UNREACHABLE();
}

size_t Bitmap::byteSize(PixelFormat format) const
{
    return m_Width * m_Height * bytesPerPixel(format);
}

size_t Bitmap::byteSize() const { return byteSize(m_PixelFormat); }

void Bitmap::pixelFormat(PixelFormat format)
{
    if (format == m_PixelFormat)
    {
        return;
    }
    auto nextByteSize = byteSize(format);
    auto nextBytes = std::unique_ptr<uint8_t[]>(new uint8_t[nextByteSize]);

    size_t fromBytesPerPixel = bytesPerPixel(m_PixelFormat);
    size_t toBytesPerPixel = bytesPerPixel(format);
    int writeIndex = 0;
    int readIndex = 0;
    for (uint32_t i = 0; i < m_Width * m_Height; i++)
    {
        for (size_t j = 0; j < toBytesPerPixel; j++)
        {
            nextBytes[writeIndex++] = j < fromBytesPerPixel ? m_Bytes[readIndex++] : 255;
        }
    }

    m_Bytes = std::move(nextBytes);
    m_PixelFormat = format;
}
