/*
 * Copyright 2022 Rive
 */

#include "viewer/viewer_host.hpp"
#include "viewer/viewer_content.hpp"

#ifdef RIVE_RENDERER_SKIA

#include "skia_factory.hpp"
#include "skia_renderer.hpp"

#include "include/core/SkSurface.h"
#include "include/core/SkCanvas.h"
#include "include/core/SkPaint.h"
#include "include/core/SkSize.h"
#include "GrDirectContext.h"

sk_sp<GrDirectContext> makeSkiaContext();
sk_sp<SkSurface> makeSkiaSurface(GrDirectContext* context, int width, int height);
void skiaPresentSurface(sk_sp<SkSurface> surface);

class SkiaViewerHost : public ViewerHost {
public:
    sk_sp<GrDirectContext> m_context;
    SkISize m_dimensions;

    bool init(sg_pass_action* action, int width, int height) override {
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
        *action = (sg_pass_action){.colors[0] = {.action = SG_ACTION_DONTCARE}};
#endif

        m_context = makeSkiaContext();
        return m_context != nullptr;
    }

    void handleResize(int width, int height) override { m_dimensions = {width, height}; }

    void beforeDefaultPass(ViewerContent* content, double elapsed) override {
        m_context->resetContext();
        auto surf = makeSkiaSurface(m_context.get(), m_dimensions.width(), m_dimensions.height());
        SkCanvas* canvas = surf->getCanvas();
        SkPaint paint;
        paint.setColor(0xFF161616);
        canvas->drawPaint(paint);

        rive::SkiaRenderer skiaRenderer(canvas);
        if (content) {
            content->handleDraw(&skiaRenderer, elapsed);
        }

        canvas->flush();
        skiaPresentSurface(surf);
        sg_reset_state_cache();
    }
};

std::unique_ptr<ViewerHost> ViewerHost::Make() { return std::make_unique<SkiaViewerHost>(); }

rive::Factory* ViewerHost::Factory() {
    static rive::SkiaFactory skiaFactory;
    return &skiaFactory;
}

#endif
