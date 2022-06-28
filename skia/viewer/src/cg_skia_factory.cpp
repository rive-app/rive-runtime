/*
 * Copyright 2022 Rive
 */

#include "cg_skia_factory.hpp"
#include "rive/core/type_conversions.hpp"
#include <vector>

#ifdef RIVE_BUILD_FOR_APPLE

#if defined(RIVE_BUILD_FOR_OSX)
    #include <ApplicationServices/ApplicationServices.h>
#elif defined(RIVE_BUILD_FOR_IOS)
    #include <CoreGraphics/CoreGraphics.h>
    #include <ImageIO/ImageIO.h>
#endif

// Helper that remembers to call CFRelease when an object goes out of scope.
template <typename T> class AutoCF {
    T m_Obj;
public:
    AutoCF(T obj) : m_Obj(obj) {}
    ~AutoCF() { if (m_Obj) CFRelease(m_Obj); }

    operator T() const { return m_Obj; }
    operator bool() const { return m_Obj != nullptr; }
    T get() const { return m_Obj; }
};

using namespace rive;

std::vector<uint8_t> CGSkiaFactory::platformDecode(Span<const uint8_t> span,
                                                   SkiaFactory::ImageInfo* info) {
    std::vector<uint8_t> pixels;

    AutoCF data = CFDataCreateWithBytesNoCopy(nullptr, span.data(), span.size(), nullptr);
    if (!data) {
        return pixels;
    }

    AutoCF source = CGImageSourceCreateWithData(data, nullptr);
    if (!source) {
        return pixels;
    }

    AutoCF image = CGImageSourceCreateImageAtIndex(source, 0, nullptr);
    if (!image) {
        return pixels;
    }

    bool isOpaque = false;
    switch (CGImageGetAlphaInfo(image.get())) {
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
    CGBitmapInfo cgInfo = kCGBitmapByteOrder32Big;    // rgba
    if (isOpaque) {
        cgInfo |= kCGImageAlphaNoneSkipLast;
    } else {
        cgInfo |= kCGImageAlphaPremultipliedLast; // premul
    }
    const size_t width = CGImageGetWidth(image);
    const size_t height = CGImageGetHeight(image);
    const size_t rowBytes = width * 4;  // 4 bytes per pixel
    const size_t size = rowBytes * height;

    pixels.resize(size);

    AutoCF cs = CGColorSpaceCreateDeviceRGB();
    AutoCF cg = CGBitmapContextCreate(pixels.data(), width, height, bitsPerComponent, rowBytes, cs, cgInfo);
    if (!cg) {
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

#endif  // RIVE_BUILD_FOR_APPLE
