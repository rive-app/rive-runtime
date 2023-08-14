/*
 * Copyright 2023 Rive
 */

#include "rive/pls/decoding/pls_decoding_factory.hpp"

#include "rive/decoders/bitmap_decoder.hpp"
#include "rive/pls/pls_image.hpp"

namespace rive::pls
{
std::unique_ptr<RenderImage> PLSDecodingFactory::decodeImage(Span<const uint8_t> encodedBytes)
{
    auto bitmap = Bitmap::decode(encodedBytes.data(), encodedBytes.size());
    if (bitmap)
    {
        // For now, PLSRenderContextImpl::makeImageTexture() only accepts RGBA.
        if (bitmap->pixelFormat() != Bitmap::PixelFormat::RGBA)
        {
            bitmap->pixelFormat(Bitmap::PixelFormat::RGBA);
        }
        return std::make_unique<PLSImage>(bitmap->width(), bitmap->height(), bitmap->detachBytes());
    }
    return nullptr;
}
} // namespace rive::pls
