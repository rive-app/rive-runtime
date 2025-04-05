/*
 * Copyright 2023 Rive
 */

#include "rive/decoders/bitmap_decoder.hpp"
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

#include <stdio.h>
#include <string.h>
#include <vector>

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
    AutoCF cg = CGBitmapContextCreate(
        pixels.get(), width, height, bitsPerComponent, rowBytes, cs, cgInfo);
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
        return nullptr;
    }

    return std::make_unique<Bitmap>(image.width,
                                    image.height,
                                    // CG always premultiplies alpha.
                                    PixelFormat::RGBAPremul,
                                    std::move(image.pixels));
}
