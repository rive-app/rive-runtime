/*
 * Copyright 2022 Rive
 */

#include "rive/core/type_conversions.hpp"
#include <vector>

#ifdef RIVE_BUILD_FOR_APPLE

#include "cg_skia_factory.hpp"
#include "cg_renderer.hpp"

#if defined(RIVE_BUILD_FOR_OSX)
#include <ApplicationServices/ApplicationServices.h>
#elif defined(RIVE_BUILD_FOR_IOS)
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#endif

using namespace rive;

std::vector<uint8_t> CGSkiaFactory::platformDecode(Span<const uint8_t> span,
                                                   SkiaFactory::ImageInfo* info)
{
    std::vector<uint8_t> pixels;

    AutoCF image = CGRenderer::DecodeToCGImage(span);
    if (!image)
    {
        return pixels;
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

    // Now create a drawing context to produce RGBA pixels

    const size_t bitsPerComponent = 8;
    CGBitmapInfo cgInfo = kCGBitmapByteOrder32Big; // rgba
    if (isOpaque)
    {
        cgInfo |= kCGImageAlphaNoneSkipLast;
    }
    else
    {
        cgInfo |= kCGImageAlphaPremultipliedLast; // premul
    }
    const size_t width = CGImageGetWidth(image);
    const size_t height = CGImageGetHeight(image);
    const size_t rowBytes = width * 4; // 4 bytes per pixel
    const size_t size = rowBytes * height;

    pixels.resize(size);

    AutoCF cs = CGColorSpaceCreateDeviceRGB();
    AutoCF cg =
        CGBitmapContextCreate(pixels.data(), width, height, bitsPerComponent, rowBytes, cs, cgInfo);
    if (!cg)
    {
        pixels.clear();
        return pixels;
    }

    CGContextSetBlendMode(cg, kCGBlendModeCopy);
    CGContextDrawImage(cg, CGRectMake(0, 0, width, height), image);

    info->alphaType = isOpaque ? AlphaType::opaque : AlphaType::premul;
    info->colorType = ColorType::rgba;
    info->width = castTo<uint32_t>(width);
    info->height = castTo<uint32_t>(height);
    info->rowBytes = rowBytes;
    return pixels;
};

#endif // RIVE_BUILD_FOR_APPLE
