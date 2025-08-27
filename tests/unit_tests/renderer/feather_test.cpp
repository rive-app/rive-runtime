/*
 * Copyright 2025 Rive
 */

#include "common/render_context_null.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include <catch.hpp>

namespace rive::gpu
{
// Ensures that a zero-area double cusp does not trigger assertions in the
// feathering code.
TEST_CASE("feathercusp", "[feather]")
{
    std::unique_ptr<RenderContext> renderContext =
        RenderContextNULL::MakeContext();

    renderContext->beginFrame({
        .renderTargetWidth = 1000,
        .renderTargetHeight = 1000,
    });

    auto path = renderContext->makeEmptyRenderPath();
    path->fillRule(FillRule::clockwise);
    path->moveTo(95.164070f, 799.999939f);
    path->cubicTo(-125.766922f,
                  799.999939f,
                  274.233032f,
                  799.999939f,
                  -95.164070f,
                  799.999939f);

    auto paint = renderContext->makeRenderPaint();
    paint->color(0xffffffff);
    paint->feather(32);

    RiveRenderer renderer(renderContext.get());
    renderer.drawPath(path.get(), paint.get());

    auto renderTarget =
        renderContext->static_impl_cast<RenderContextNULL>()->makeRenderTarget(
            renderContext->frameDescriptor().renderTargetWidth,
            renderContext->frameDescriptor().renderTargetHeight);
    renderContext->flush({.renderTarget = renderTarget.get()});

    // The test passes if nothing crashes.
}
} // namespace rive::gpu
