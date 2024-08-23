/*
 * Copyright 2022 Rive
 */

#include "viewer/viewer_host.hpp"
#include "viewer/viewer_content.hpp"

#ifdef RIVE_RENDERER_SKIA

#ifdef RIVE_BUILD_FOR_APPLE
#include "cg_skia_factory.hpp"
static rive::CGSkiaFactory skiaFactory;
#else
#include "skia_factory.hpp"
static rive::SkiaFactory skiaFactory;
#endif
#include "skia_renderer.hpp"

#include "include/core/SkSurface.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkPaint.h"
#include "include/core/SkSize.h"
#include "GrDirectContext.h"

sk_sp<GrDirectContext> makeSkiaContext();
sk_sp<SkSurface> makeSkiaSurface(GrDirectContext* context, int width, int height);
void skiaPresentSurface(sk_sp<SkSurface> surface);

// Experimental flag, until we complete coregraphics_host
// #define TEST_CG_RENDERER

// #define SW_SKIA_MODE

#ifdef TEST_CG_RENDERER
#include "cg_factory.hpp"
#include "cg_renderer.hpp"
#include "mac_utils.hpp"
static void render_with_cg(SkCanvas* canvas, int w, int h, ViewerContent* content, double elapsed)
{
    // cons up a CGContext
    auto pixels = SkData::MakeUninitialized(w * h * 4);
    auto bytes = (uint8_t*)pixels->writable_data();
    std::fill(bytes, bytes + pixels->size(), 0);
    AutoCF space = CGColorSpaceCreateDeviceRGB();
    auto info = kCGBitmapByteOrder32Big | kCGImageAlphaPremultipliedLast;
    AutoCF ctx = CGBitmapContextCreate(bytes, w, h, 8, w * 4, space, info);

    // Wrap it with our renderer
    rive::CGRenderer renderer(ctx, w, h);
    content->handleDraw(&renderer, elapsed);
    CGContextFlush(ctx);

    // Draw the pixels into the canvas
    auto img = SkImage::MakeRasterData(SkImageInfo::MakeN32Premul(w, h), pixels, w * 4);
    canvas->drawImage(img, 0, 0, SkSamplingOptions(SkFilterMode::kNearest), nullptr);
}
#endif

class SkiaViewerHost : public ViewerHost
{
public:
    sk_sp<GrDirectContext> m_context;
    SkISize m_dimensions;

    bool init(sg_pass_action* action, int width, int height) override
    {
        m_dimensions = {width, height};

#if defined(SK_METAL)
        // Skia is layered behind the Sokol view, so we need to make sure Sokol
        // clears transparent. Skia will draw the background.
        *action = (sg_pass_action){.colors[0] = {
            .action = SG_ACTION_CLEAR,
            .value =
            { 0.0f,
              0.0,
              0.0f,
              0.0 }
        }};
#elif defined(SK_GL)
        // Skia commands are issued to the same GL context before Sokol, so we need
        // to make sure Sokol does not clear the buffer.
        *action = (sg_pass_action){.colors[0] = {.action = SG_ACTION_DONTCARE }};
#endif

        m_context = makeSkiaContext();
        return m_context != nullptr;
    }

    void handleResize(int width, int height) override { m_dimensions = {width, height}; }

    void beforeDefaultPass(ViewerContent* content, double elapsed) override
    {
        m_context->resetContext();
        auto surf = makeSkiaSurface(m_context.get(), m_dimensions.width(), m_dimensions.height());
        SkCanvas* canvas = surf->getCanvas();
        SkPaint paint;
        paint.setColor(0xFF161616);
        canvas->drawPaint(paint);

        if (content)
        {
#ifdef TEST_CG_RENDERER
            render_with_cg(canvas, m_dimensions.width(), m_dimensions.height(), content, elapsed);
#elif defined(SW_SKIA_MODE)
            auto info = SkImageInfo::MakeN32Premul(m_dimensions.width(), m_dimensions.height());
            auto swsurf = SkSurface::MakeRaster(info);
            rive::SkiaRenderer skiaRenderer(swsurf->getCanvas());
            content->handleDraw(&skiaRenderer, elapsed);
            auto img = swsurf->makeImageSnapshot();
            canvas->drawImage(img, 0, 0, SkSamplingOptions(SkFilterMode::kNearest), nullptr);
#else
            rive::SkiaRenderer skiaRenderer(canvas);
            content->handleDraw(&skiaRenderer, elapsed);
#endif
        }

        canvas->flush();
        skiaPresentSurface(surf);
        sg_reset_state_cache();
    }
};

std::unique_ptr<ViewerHost> ViewerHost::Make() { return rivestd::make_unique<SkiaViewerHost>(); }

rive::Factory* ViewerHost::Factory()
{
#ifdef TEST_CG_RENDERER
    static rive::CGFactory gFactory;
    return &gFactory;
#else
    return &skiaFactory;
#endif
}

#endif
