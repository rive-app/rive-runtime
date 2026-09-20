/*
 * Copyright 2026 Rive
 */

#include "common/render_context_null.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "rive/shapes/paint/color.hpp"
#include "rive/math/raw_path.hpp"
#include "gradient.hpp"
#include <catch.hpp>

using namespace rive;
using namespace rive::gpu;

static RenderContext::FrameDescriptor s_frameDescriptor = {
    .renderTargetWidth = 100,
    .renderTargetHeight = 100,
};

static void flushFrame(RenderContext* ctx)
{
    auto renderTarget =
        ctx->static_impl_cast<RenderContextNULL>()->makeRenderTarget(
            s_frameDescriptor.renderTargetWidth,
            s_frameDescriptor.renderTargetHeight);
    ctx->flush({.renderTarget = renderTarget.get()});
}

TEST_CASE("modulate-color-save-restore", "[RiveRenderer][color]")
{
    auto ctx = RenderContextNULL::MakeContext();
    ctx->beginFrame(s_frameDescriptor);

    RawPath rect;
    rect.addRect({0, 0, 100, 100});
    auto path = ctx->makeRenderPath(rect, FillRule::nonZero);
    auto paint = ctx->makeRenderPaint();
    paint->color(0xffffffff);

    RiveRenderer renderer(ctx.get());
    CHECK(renderer.currentModulatedColor() == 0xffffffff);

    renderer.save();
    renderer.modulateColor(0xff000000);
    renderer.modulateColor(0xff808080);
    CHECK(renderer.currentModulatedColor() == 0xff000000);
    renderer.drawPath(path.get(), paint.get());

    renderer.save();
    renderer.modulateColor(0xff804020, true);
    CHECK(renderer.currentModulatedColor() == 0xff804020);
    renderer.modulateColor(0x80ff8000);
    CHECK(renderer.currentModulatedColor() == 0x80802000);
    renderer.drawPath(path.get(), paint.get());
    renderer.restore();

    CHECK(renderer.currentModulatedColor() == 0xff000000);
    renderer.restore();
    CHECK(renderer.currentModulatedColor() == 0xffffffff);

    flushFrame(ctx.get());
}

// A white modulation has to leave every existing draw bit identical.
TEST_CASE("color-modulate-is-exact-at-identity", "[RiveRenderer][color]")
{
    for (ColorInt color : {0x00000000u, 0x80123456u, 0xfffefdfcu})
    {
        for (float opacity : {1.0f, 0.5f, 0.3f})
        {
            CHECK(colorModulate(color, 0xffffffff, opacity) ==
                  colorModulateOpacity(color, opacity));
        }
    }
    CHECK(colorModulate(0xff8040ff, 0xff000000) == 0xff000000);
}

TEST_CASE("gradient-modulated-by-color", "[RiveRenderer][color][gradient]")
{
    ColorInt colors[] = {0xffff0000, 0xff00ff00};
    float stops[] = {0.0f, 1.0f};
    auto gradient = Gradient::MakeLinear(0, 0, 100, 0, colors, stops, 2);
    REQUIRE(gradient != nullptr);

    auto black = gradient->getModulated(1.0f, 0xff000000);
    REQUIRE(black.get() != gradient.get());
    CHECK(black->colors()[0] == 0xff000000);
    CHECK(black->colors()[1] == 0xff000000);

    CHECK(gradient->getModulated(1.0f, 0xff000000).get() == black.get());
    CHECK(gradient->getModulated(1.0f, 0xff808080).get() != black.get());
}
