/*
 * Copyright 2023 Rive
 */

#include "rive/decoders/bitmap_decoder.hpp"
#include "rive/rive_types.hpp"
#include <stdio.h>
#include <string.h>
#include <vector>

std::unique_ptr<Bitmap> DecodePng(const uint8_t bytes[], size_t byteCount);
std::unique_ptr<Bitmap> DecodeJpeg(const uint8_t bytes[], size_t byteCount);
std::unique_ptr<Bitmap> DecodeWebP(const uint8_t bytes[], size_t byteCount);

using BitmapDecoder = std::unique_ptr<Bitmap> (*)(const uint8_t bytes[], size_t byteCount);
struct ImageFormat
{
    const char* name;
    std::vector<uint8_t> fingerprint;
    BitmapDecoder decodeImage;
};

std::unique_ptr<Bitmap> Bitmap::decode(const uint8_t bytes[], size_t byteCount)
{
    static ImageFormat decoders[] = {
        {
            "png",
            {0x89, 0x50, 0x4E, 0x47},
            DecodePng,
        },
        {
            "jpeg",
            {0xFF, 0xD8, 0xFF},
            DecodeJpeg,
        },
        {
            "webp",
            {0x52, 0x49, 0x46},
            DecodeWebP,
        },
    };

    for (auto recognizer : decoders)
    {
        auto& fingerprint = recognizer.fingerprint;

        // Immediately discard decoders with fingerprints that are longer than
        // the file buffer.
        if (recognizer.fingerprint.size() > byteCount)
        {
            continue;
        }

        // If the fingerprint doesn't match, discrd this decoder. These are all bytes so .size() is
        // fine here.
        if (memcmp(fingerprint.data(), bytes, fingerprint.size()) != 0)
        {
            continue;
        }

        auto bitmap = recognizer.decodeImage(bytes, byteCount);
        if (!bitmap)
        {
            fprintf(stderr, "Bitmap::decode - failed to decode a %s.\n", recognizer.name);
        }
        return bitmap;
    }
    return nullptr;
}
