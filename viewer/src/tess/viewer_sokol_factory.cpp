#ifdef RIVE_RENDERER_TESS
#include "viewer/tess/viewer_sokol_factory.hpp"
#include "rive/decoders/bitmap_decoder.hpp"
#include "rive/tess/sokol/sokol_tess_renderer.hpp"
#include "sokol_gfx.h"

rive::rcp<rive::RenderImage> ViewerSokolFactory::decodeImage(rive::Span<const uint8_t> bytes)
{
    auto bitmap = Bitmap::decode(bytes.data(), bytes.size());
    if (bitmap)
    {
        // We have a bitmap, let's make an image.

        // For now our SokolRenderImage only works with RGBA.
        if (bitmap->pixelFormat() != Bitmap::PixelFormat::RGBA)
        {
            bitmap->pixelFormat(Bitmap::PixelFormat::RGBA);
        }

        // In this case the image is in-band and the imageGpuResource is only
        // used once by the unique SokolRenderImage. We introduced this
        // abstraction because when the image is loaded externally (say by
        // something that built up an atlas) the image gpu resource may be
        // shared by multiple RenderImage referenced by multiple Rive objects.
        auto imageGpuResource = rive::rcp<rive::SokolRenderImageResource>(
            new rive::SokolRenderImageResource(bitmap->bytes(), bitmap->width(), bitmap->height()));

        static rive::Mat2D identity;
        return rive::make_rcp<rive::SokolRenderImage>(imageGpuResource,
                                                      bitmap->width(),
                                                      bitmap->height(),
                                                      identity);
    }
    return nullptr;
}
#endif
