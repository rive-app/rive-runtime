/*
 * Copyright 2023 Rive
 */

#ifndef _RIVE_BITMAP_DECODER_HPP_
#define _RIVE_BITMAP_DECODER_HPP_

#include <memory>
#include <vector>

/// Bitmap will always take ownership of the bytes it is constructed with.
class Bitmap
{
public:
    enum class PixelFormat : uint8_t
    {
        RGB,
        RGBA,
        RGBAPremul,
    };

    Bitmap(uint32_t width,
           uint32_t height,
           PixelFormat pixelFormat,
           std::unique_ptr<const uint8_t[]> bytes);

    Bitmap(uint32_t width,
           uint32_t height,
           PixelFormat pixelFormat,
           const uint8_t* bytes);

private:
    uint32_t m_Width;
    uint32_t m_Height;
    PixelFormat m_PixelFormat;
    std::unique_ptr<const uint8_t[]> m_Bytes;

public:
    uint32_t width() const { return m_Width; }
    uint32_t height() const { return m_Height; }
    PixelFormat pixelFormat() const { return m_PixelFormat; }
    const uint8_t* bytes() const { return m_Bytes.get(); }
    std::unique_ptr<const uint8_t[]> detachBytes()
    {
        return std::move(m_Bytes);
    }

    enum class ImageType
    {
        png,
        jpeg,
        webp,
    };

    using BitmapDecoder = std::unique_ptr<Bitmap> (*)(const uint8_t bytes[],
                                                      size_t byteCount);

    struct ImageFormat
    {
        const char* name;
        ImageType type;
        std::vector<uint8_t> fingerprint;
        BitmapDecoder decodeImage;
    };

    static const ImageFormat* RecognizeImageFormat(const uint8_t bytes[],
                                                   size_t byteCount);

    static std::unique_ptr<Bitmap> decode(const uint8_t bytes[],
                                          size_t byteCount);

    // Change the pixel format (note this will resize bytes).
    void pixelFormat(PixelFormat format);
};

#endif
