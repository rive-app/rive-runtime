/*
 * Copyright 2022 Rive
 */

#include "mac_utils.hpp"

#ifdef RIVE_BUILD_FOR_APPLE

#if defined(RIVE_BUILD_FOR_IOS)
#include <CoreGraphics/CGImage.h>
#include <ImageIO/CGImageSource.h>
#endif

CGImageRef rive::DecodeToCGImage(rive::Span<const uint8_t> span) {
    AutoCF data = CFDataCreate(nullptr, span.data(), span.size());
    if (!data) {
        printf("CFDataCreate failed\n");
        return nullptr;
    }

    AutoCF source = CGImageSourceCreateWithData(data, nullptr);
    if (!source) {
        printf("CGImageSourceCreateWithData failed\n");
        return nullptr;
    }

    auto image = CGImageSourceCreateImageAtIndex(source, 0, nullptr);
    if (!image) {
        printf("CGImageSourceCreateImageAtIndex failed\n");
        return nullptr;
    }
    return image;
}

#endif
