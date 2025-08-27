/*
 * Copyright 2023 Rive
 */

#include "rive/decoders/bitmap_decoder.hpp"

#ifdef __APPLE__
#include "rive/rive_types.hpp"
#include "rive/math/math_types.hpp"
#include "rive/core/type_conversions.hpp"
#include "utils/auto_cf.hpp"

#include <TargetConditionals.h>

#if TARGET_OS_IPHONE
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#elif TARGET_OS_MAC
#include <ApplicationServices/ApplicationServices.h>
#endif
#endif

#include <stdio.h>
#include <string.h>
#include <vector>

#ifdef RIVE_PNG
std::unique_ptr<Bitmap> DecodePng(const uint8_t bytes[], size_t byteCount);
#endif
#ifdef RIVE_JPEG
std::unique_ptr<Bitmap> DecodeJpeg(const uint8_t bytes[], size_t byteCount);
#endif
#ifdef RIVE_WEBP
std::unique_ptr<Bitmap> DecodeWebP(const uint8_t bytes[], size_t byteCount);
#endif

static Bitmap::ImageFormat _formats[] = {
    {
        "png",
        Bitmap::ImageType::png,
        {0x89, 0x50, 0x4E, 0x47},
#ifdef RIVE_PNG
        DecodePng,
#else
        nullptr,
#endif
    },
    {
        "jpeg",
        Bitmap::ImageType::jpeg,
        {0xFF, 0xD8, 0xFF},
#ifdef RIVE_JPEG
        DecodeJpeg,
#else
        nullptr,
#endif
    },
    {
        "webp",
        Bitmap::ImageType::webp,
        {0x52, 0x49, 0x46},
#ifdef RIVE_WEBP
        DecodeWebP,
#else
        nullptr,
#endif
    },
};

const Bitmap::ImageFormat* Bitmap::RecognizeImageFormat(const uint8_t bytes[],
                                                        size_t byteCount)
{
    for (const ImageFormat& format : _formats)
    {
        auto& fingerprint = format.fingerprint;

        // Immediately discard decoders with fingerprints that are longer than
        // the file buffer.
        if (format.fingerprint.size() > byteCount)
        {
            continue;
        }

        // If the fingerprint doesn't match, discrd this decoder. These are all
        // bytes so .size() is fine here.
        if (memcmp(fingerprint.data(), bytes, fingerprint.size()) != 0)
        {
            continue;
        }

        return &format;
    }
    return nullptr;
}

#ifdef __APPLE__
// Represents raw, premultiplied, RGBA image data with tightly packed rows
// (width * 4 bytes).
struct PlatformCGImage
{
    uint32_t width = 0;
    uint32_t height = 0;
    bool opaque = false;
    std::unique_ptr<uint8_t[]> pixels;
};

bool cg_image_decode(const uint8_t* encodedBytes,
                     size_t encodedSizeInBytes,
                     PlatformCGImage* platformImage)
{
    AutoCF data =
        CFDataCreate(kCFAllocatorDefault, encodedBytes, encodedSizeInBytes);
    if (!data)
    {
        return false;
    }

    AutoCF source = CGImageSourceCreateWithData(data, nullptr);
    if (!source)
    {
        return false;
    }

    AutoCF image = CGImageSourceCreateImageAtIndex(source, 0, nullptr);
    if (!image)
    {
        return false;
    }

    bool isOpaque = false;
    switch (CGImageGetAlphaInfo(image.get()))
    {
        case kCGImageAlphaNone:
        case kCGImageAlphaNoneSkipFirst:
        case kCGImageAlphaNoneSkipLast:
            isOpaque = true;
            break;
        default:
            break;
    }

    const size_t width = CGImageGetWidth(image);
    const size_t height = CGImageGetHeight(image);
    const size_t rowBytes = width * 4; // 4 bytes per pixel
    const size_t size = rowBytes * height;

    const size_t bitsPerComponent = 8;
    CGBitmapInfo cgInfo = kCGBitmapByteOrder32Big; // rgba
    if (isOpaque)
    {
        cgInfo |= kCGImageAlphaNoneSkipLast;
    }
    else
    {
        cgInfo |= kCGImageAlphaPremultipliedLast;
    }

    std::unique_ptr<uint8_t[]> pixels(new uint8_t[size]);

    AutoCF cs = CGColorSpaceCreateDeviceRGB();
    AutoCF cg = CGBitmapContextCreate(pixels.get(),
                                      width,
                                      height,
                                      bitsPerComponent,
                                      rowBytes,
                                      cs,
                                      cgInfo);
    if (!cg)
    {
        return false;
    }

    CGContextSetBlendMode(cg, kCGBlendModeCopy);
    CGContextDrawImage(cg, CGRectMake(0, 0, width, height), image);

    platformImage->width = rive::castTo<uint32_t>(width);
    platformImage->height = rive::castTo<uint32_t>(height);
    platformImage->opaque = isOpaque;
    platformImage->pixels = std::move(pixels);

    return true;
}

std::unique_ptr<Bitmap> Bitmap::decode(const uint8_t bytes[], size_t byteCount)
{
    PlatformCGImage image;
    if (!cg_image_decode(bytes, byteCount, &image))
    {
#if defined RIVE_APPLETVOS || defined RIVE_APPLETVOS_SIMULATOR
        const Bitmap::ImageFormat* format =
            Bitmap::RecognizeImageFormat(bytes, byteCount);
        if (format != nullptr)
        {
            auto bitmap = format->decodeImage != nullptr
                              ? format->decodeImage(bytes, byteCount)
                              : nullptr;
            return bitmap;
        }
#endif
        return nullptr;
    }

    return std::make_unique<Bitmap>(image.width,
                                    image.height,
                                    // CG always premultiplies alpha.
                                    PixelFormat::RGBAPremul,
                                    std::move(image.pixels));
}
#else
std::unique_ptr<Bitmap> Bitmap::decode(const uint8_t bytes[], size_t byteCount)
{
    const ImageFormat* format = RecognizeImageFormat(bytes, byteCount);
    if (format != nullptr)
    {
        auto bitmap = format->decodeImage != nullptr
                          ? format->decodeImage(bytes, byteCount)
                          : nullptr;
        if (!bitmap)
        {
            fprintf(stderr,
                    "Bitmap::decode - failed to decode a %s.\n",
                    format->name);
        }
        return bitmap;
    }
    return nullptr;
}
#endif
