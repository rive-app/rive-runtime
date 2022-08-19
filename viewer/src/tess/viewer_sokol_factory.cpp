#ifdef RIVE_RENDERER_TESS
#include "viewer/tess/viewer_sokol_factory.hpp"
#include "viewer/tess/bitmap_decoder.hpp"
#include "rive/tess/sokol/sokol_tess_renderer.hpp"
#include "sokol_gfx.h"

sg_pixel_format SgPixelFormat(Bitmap* bitmap) {
    switch (bitmap->pixelFormat()) {
        case Bitmap::PixelFormat::R: return SG_PIXELFORMAT_R8;
        case Bitmap::PixelFormat::RGBA: return SG_PIXELFORMAT_RGBA8;
        default: return SG_PIXELFORMAT_NONE;
    }
}

std::unique_ptr<rive::RenderImage>
ViewerSokolFactory::decodeImage(rive::Span<const uint8_t> bytes) {
    auto bitmap = Bitmap::decode(bytes);
    if (bitmap) {
        // We have a bitmap, let's make an image.

        // Sokol doesn't support an RGB pixel format, so we have to bump RGB to RGBA.
        if (bitmap->pixelFormat() == Bitmap::PixelFormat::RGB) {
            bitmap->pixelFormat(Bitmap::PixelFormat::RGBA);
        }
        return std::make_unique<rive::SokolRenderImage>(sg_make_image((sg_image_desc){
            .width = (int)bitmap->width(),
            .height = (int)bitmap->height(),
            .data.subimage[0][0] = {bitmap->bytes(), bitmap->byteSize()},
            .pixel_format = SgPixelFormat(bitmap.get()),
        }));
    }
    return nullptr;
}
#endif