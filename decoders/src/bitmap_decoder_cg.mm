/*
 * Copyright 2023 Rive
 */

#include "rive/decoders/bitmap_decoder.hpp"
#include "rive/rive_types.hpp"
#include "rive/math/simd.hpp"
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

// Represents raw, premultiplied, RGBA image data with tightly packed rows (width * 4 bytes).
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
    AutoCF data = CFDataCreate(kCFAllocatorDefault, encodedBytes, encodedSizeInBytes);
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
    AutoCF cg =
        CGBitmapContextCreate(pixels.get(), width, height, bitsPerComponent, rowBytes, cs, cgInfo);
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

    // CG only supports premultiplied alpha. Unmultiply now.
    size_t imageNumPixels = image.height * image.width;
    size_t imageSizeInBytes = imageNumPixels * 4;
    // Process 2 pixels at once, deal with odd number of pixels
    if (imageNumPixels & 1)
    {
        imageSizeInBytes -= 4;
    }
    size_t i;
    for (i = 0; i < imageSizeInBytes; i += 8)
    {
        // Load 2 pixels into 64 bits
        auto twoPixels = rive::simd::load<uint8_t, 8>(&image.pixels[i]);
        auto a0 = twoPixels[3];
        auto a1 = twoPixels[7];
        // Avoid computation if both pixels are either fully transparent or opaque pixels
        if ((a0 > 0 && a0 < 255) || (a1 > 0 && a1 < 255))
        {
            // Avoid potential division by zero
            a0 = std::max<uint8_t>(a0, 1);
            a1 = std::max<uint8_t>(a1, 1);
            // Cast to 16 bits to avoid overflow
            rive::uint16x8 rgbaWidex2 = rive::simd::cast<uint16_t>(twoPixels);
            // Unpremult: multiply by RGB by "255.0 / alpha"
            rgbaWidex2 *= rive::uint16x8{255, 255, 255, 1, 255, 255, 255, 1};
            rgbaWidex2 /= rive::uint16x8{a0, a0, a0, 1, a1, a1, a1, 1};
            // Cast back to 8 bits and store
            twoPixels = rive::simd::cast<uint8_t>(rgbaWidex2);
            rive::simd::store(&image.pixels[i], twoPixels);
        }
    }
    // Process last odd pixel if needed
    if (imageNumPixels & 1)
    {
        // Load 1 pixel into 32 bits
        auto rgba = rive::simd::load<uint8_t, 4>(&image.pixels[i]);
        // Avoid computation for fully transparent or opaque pixels
        if (rgba.a > 0 && rgba.a < 255)
        {
            // Cast to 16 bits to avoid overflow
            rive::uint16x4 rgbaWide = rive::simd::cast<uint16_t>(rgba);
            // Unpremult: multiply by RGB by "255.0 / alpha"
            rgbaWide *= rive::uint16x4{255, 255, 255, 1};
            rgbaWide /= rive::uint16x4{rgba.a, rgba.a, rgba.a, 1};
            // Cast back to 8 bits and store
            rgba = rive::simd::cast<uint8_t>(rgbaWide);
            rive::simd::store(&image.pixels[i], rgba);
        }
    }

    return std::make_unique<Bitmap>(
        image.width, image.height, PixelFormat::RGBA, std::move(image.pixels));
}
