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
    AutoCF data = CFDataCreateWithBytesNoCopy(nullptr, span.data(), span.size(), nullptr);
    if (!data) {
        return nullptr;
    }

    AutoCF source = CGImageSourceCreateWithData(data, nullptr);
    if (!source) {
        return nullptr;
    }

    return CGImageSourceCreateImageAtIndex(source, 0, nullptr);
}

#endif
