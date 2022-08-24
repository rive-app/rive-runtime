#ifdef RIVE_RENDERER_TESS
#include "viewer/tess/viewer_sokol_factory.hpp"
#include "viewer/tess/bitmap_decoder.hpp"
#include "rive/tess/sokol/sokol_tess_renderer.hpp"
#include "sokol_gfx.h"

std::unique_ptr<rive::RenderImage>
ViewerSokolFactory::decodeImage(rive::Span<const uint8_t> bytes) {
    auto bitmap = Bitmap::decode(bytes);
    if (bitmap) {
        // We have a bitmap, let's make an image.

        // For now our SokolRenderImage only works with RGBA.
        if (bitmap->pixelFormat() != Bitmap::PixelFormat::RGBA) {
            bitmap->pixelFormat(Bitmap::PixelFormat::RGBA);
        }

        return std::make_unique<rive::SokolRenderImage>(bitmap->bytes(),
                                                        bitmap->width(),
                                                        bitmap->height());
    }
    return nullptr;
}
#endif