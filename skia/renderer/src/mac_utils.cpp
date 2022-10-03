/*
 * Copyright 2022 Rive
 */

#include "mac_utils.hpp"

#ifdef RIVE_BUILD_FOR_APPLE

#if defined(RIVE_BUILD_FOR_IOS)
#include <CoreGraphics/CGImage.h>
#include <ImageIO/CGImageSource.h>
#endif

AutoCF<CGImageRef> rive::FlipCGImageInY(AutoCF<CGImageRef> image)
{
    if (!image)
    {
        return nullptr;
    }

    auto w = CGImageGetWidth(image);
    auto h = CGImageGetHeight(image);
    AutoCF space = CGColorSpaceCreateDeviceRGB();
    auto info = kCGBitmapByteOrder32Big | kCGImageAlphaPremultipliedLast;
    AutoCF ctx = CGBitmapContextCreate(nullptr, w, h, 8, 0, space, info);
    CGContextConcatCTM(ctx, CGAffineTransformMake(1, 0, 0, -1, 0, h));
    CGContextDrawImage(ctx, CGRectMake(0, 0, w, h), image);
    return CGBitmapContextCreateImage(ctx);
}

AutoCF<CGImageRef> rive::DecodeToCGImage(rive::Span<const uint8_t> span)
{
    AutoCF data = CFDataCreate(nullptr, span.data(), span.size());
    if (!data)
    {
        printf("CFDataCreate failed\n");
        return nullptr;
    }

    AutoCF source = CGImageSourceCreateWithData(data, nullptr);
    if (!source)
    {
        printf("CGImageSourceCreateWithData failed\n");
        return nullptr;
    }

    AutoCF image = CGImageSourceCreateImageAtIndex(source, 0, nullptr);
    if (!image)
    {
        printf("CGImageSourceCreateImageAtIndex failed\n");
    }
    return image;
}

#endif
