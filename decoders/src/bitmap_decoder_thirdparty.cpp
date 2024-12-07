/*
 * Copyright 2023 Rive
 */

#include "rive/decoders/bitmap_decoder.hpp"
#include <stdio.h>
#include <string.h>
#include <vector>

#ifdef RIVE_PNG
std::unique_ptr<Bitmap> DecodePng(const uint8_t bytes[], size_t byteCount);
#endif
#ifdef RIVE_JPEG
std::unique_ptr<Bitmap> DecodeJpeg(const uint8_t bytes[], size_t byteCount);
#endif
std::unique_ptr<Bitmap> DecodeWebP(const uint8_t bytes[], size_t byteCount);

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
        DecodeWebP,
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
