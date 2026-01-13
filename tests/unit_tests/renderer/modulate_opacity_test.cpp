/*
 * Copyright 2025 Rive
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

// Helper to create a rectangular RawPath
static RawPath& make_rect(const AABB& bounds)
{
    static RawPath path;
    path.rewind();
    path.addRect(bounds);
    return path;
}

// Helper to create an oval RawPath
static RawPath& make_oval(const AABB& bounds)
{
    static RawPath path;
    path.rewind();
    path.addOval(bounds);
    return path;
}

// Helper to create a line path
static RawPath& make_line(float x1, float y1, float x2, float y2)
{
    static RawPath path;
    path.rewind();
    path.moveTo(x1, y1);
    path.lineTo(x2, y2);
    return path;
}

// Helper to flush a frame
static void flushFrame(RenderContext* ctx)
{
    auto renderTarget =
        ctx->static_impl_cast<RenderContextNULL>()->makeRenderTarget(
            s_frameDescriptor.renderTargetWidth,
            s_frameDescriptor.renderTargetHeight);
    ctx->flush({.renderTarget = renderTarget.get()});
}

// Test that modulateOpacity stacks correctly with save/restore.
TEST_CASE("modulate-opacity-save-restore", "[RiveRenderer][opacity]")
{
    auto ctx = RenderContextNULL::MakeContext();
    ctx->beginFrame(s_frameDescriptor);

    auto path =
        ctx->makeRenderPath(make_rect({0, 0, 100, 100}), FillRule::nonZero);
    auto paint = ctx->makeRenderPaint();
    paint->color(0xffffffff);

    RiveRenderer renderer(ctx.get());

    // Initial opacity should be 1.0
    CHECK(renderer.currentModulatedOpacity() == 1.0f);

    renderer.save();
    renderer.modulateOpacity(0.5f);
    CHECK(renderer.currentModulatedOpacity() == Approx(0.5f));

    // Nested save/restore should multiply opacity
    renderer.save();
    renderer.modulateOpacity(0.5f);
    // Now effective opacity should be 0.5 * 0.5 = 0.25
    CHECK(renderer.currentModulatedOpacity() == Approx(0.25f));
    renderer.drawPath(path.get(), paint.get());
    renderer.restore();

    // After restore, opacity should be back to 0.5
    CHECK(renderer.currentModulatedOpacity() == Approx(0.5f));
    renderer.drawPath(path.get(), paint.get());
    renderer.restore();

    // After final restore, opacity should be back to 1.0
    CHECK(renderer.currentModulatedOpacity() == Approx(1.0f));
    renderer.drawPath(path.get(), paint.get());

    flushFrame(ctx.get());
}

// Test that multiple calls to modulateOpacity within the same save level
// multiply together.
TEST_CASE("modulate-opacity-multiple-calls", "[RiveRenderer][opacity]")
{
    auto ctx = RenderContextNULL::MakeContext();
    ctx->beginFrame(s_frameDescriptor);

    auto path =
        ctx->makeRenderPath(make_rect({0, 0, 100, 100}), FillRule::nonZero);
    auto paint = ctx->makeRenderPaint();
    paint->color(0xffffffff);

    RiveRenderer renderer(ctx.get());

    renderer.save();
    renderer.modulateOpacity(0.5f);
    CHECK(renderer.currentModulatedOpacity() == Approx(0.5f));
    renderer.modulateOpacity(0.5f);
    CHECK(renderer.currentModulatedOpacity() == Approx(0.25f));
    renderer.modulateOpacity(0.5f);
    // Effective opacity should be 0.5 * 0.5 * 0.5 = 0.125
    CHECK(renderer.currentModulatedOpacity() == Approx(0.125f));
    renderer.drawPath(path.get(), paint.get());
    renderer.restore();

    flushFrame(ctx.get());
}

// Test that opacity of 1.0 doesn't modify anything (fast path).
TEST_CASE("modulate-opacity-identity", "[RiveRenderer][opacity]")
{
    auto ctx = RenderContextNULL::MakeContext();
    ctx->beginFrame(s_frameDescriptor);

    auto path =
        ctx->makeRenderPath(make_rect({0, 0, 100, 100}), FillRule::nonZero);
    auto paint = ctx->makeRenderPaint();
    paint->color(0xffffffff);

    RiveRenderer renderer(ctx.get());

    // Calling modulateOpacity(1.0f) should have no effect
    renderer.save();
    renderer.modulateOpacity(1.0f);
    CHECK(renderer.currentModulatedOpacity() == Approx(1.0f));
    renderer.modulateOpacity(1.0f);
    CHECK(renderer.currentModulatedOpacity() == Approx(1.0f));
    renderer.drawPath(path.get(), paint.get());
    renderer.restore();

    flushFrame(ctx.get());
}

// Test that opacity of 0 results in fully transparent draw.
TEST_CASE("modulate-opacity-zero", "[RiveRenderer][opacity]")
{
    auto ctx = RenderContextNULL::MakeContext();
    ctx->beginFrame(s_frameDescriptor);

    auto path =
        ctx->makeRenderPath(make_rect({0, 0, 100, 100}), FillRule::nonZero);
    auto paint = ctx->makeRenderPaint();
    paint->color(0xffffffff);

    RiveRenderer renderer(ctx.get());

    renderer.save();
    renderer.modulateOpacity(0.0f);
    CHECK(renderer.currentModulatedOpacity() == Approx(0.0f));
    renderer.drawPath(path.get(), paint.get());
    renderer.restore();

    flushFrame(ctx.get());
}

// Test Gradient::getModulated returns the same gradient when opacity is 1.0.
TEST_CASE("gradient-modulated-identity", "[RiveRenderer][opacity][gradient]")
{
    ColorInt colors[] = {0xffff0000, 0xff00ff00, 0xff0000ff};
    float stops[] = {0.0f, 0.5f, 1.0f};

    auto gradient = Gradient::MakeLinear(0, 0, 100, 0, colors, stops, 3);
    REQUIRE(gradient != nullptr);

    // Modulating with 1.0 should return the same gradient instance
    auto modulated = gradient->getModulated(1.0f);
    CHECK(modulated.get() == gradient.get());
}

// Test Gradient::getModulated creates a new gradient with modulated colors.
TEST_CASE("gradient-modulated-colors", "[RiveRenderer][opacity][gradient]")
{
    ColorInt colors[] = {0xffff0000, 0xff00ff00};
    float stops[] = {0.0f, 1.0f};

    auto gradient = Gradient::MakeLinear(0, 0, 100, 0, colors, stops, 2);
    REQUIRE(gradient != nullptr);

    // Modulating with 0.5 should create a new gradient
    auto modulated = gradient->getModulated(0.5f);
    CHECK(modulated.get() != gradient.get());

    // The modulated gradient should have reduced alpha
    // 255 * 0.5 = 127.5, which rounds to 128
    const ColorInt* modColors = modulated->colors();
    CHECK(colorAlpha(modColors[0]) == 128);
    CHECK(colorAlpha(modColors[1]) == 128);
}

// Test Gradient::getModulated caches the last-used modulated gradient.
TEST_CASE("gradient-modulated-caching", "[RiveRenderer][opacity][gradient]")
{
    ColorInt colors[] = {0xffff0000, 0xff00ff00};
    float stops[] = {0.0f, 1.0f};

    auto gradient = Gradient::MakeLinear(0, 0, 100, 0, colors, stops, 2);
    REQUIRE(gradient != nullptr);

    // First call with 0.5 creates a new modulated gradient
    auto modulated1 = gradient->getModulated(0.5f);
    CHECK(modulated1.get() != gradient.get());

    // Second call with same opacity should return the cached version
    auto modulated2 = gradient->getModulated(0.5f);
    CHECK(modulated2.get() == modulated1.get());

    // Different opacity should create a different gradient (replaces cache)
    auto modulated3 = gradient->getModulated(0.25f);
    CHECK(modulated3.get() != modulated1.get());

    // Calling with 0.25 again returns cached version
    auto modulated4 = gradient->getModulated(0.25f);
    CHECK(modulated4.get() == modulated3.get());

    // Calling with 0.5 again creates a new gradient (cache was replaced)
    auto modulated5 = gradient->getModulated(0.5f);
    CHECK(modulated5.get() != modulated1.get()); // New instance
    CHECK(modulated5.get() != modulated3.get()); // Different from 0.25 version
}

// Test that modulated gradients preserve stops.
TEST_CASE("gradient-modulated-preserves-stops",
          "[RiveRenderer][opacity][gradient]")
{
    ColorInt colors[] = {0xffff0000, 0xff00ff00, 0xff0000ff};
    float stops[] = {0.0f, 0.3f, 1.0f};

    auto gradient = Gradient::MakeLinear(0, 0, 100, 0, colors, stops, 3);
    REQUIRE(gradient != nullptr);

    auto modulated = gradient->getModulated(0.5f);
    REQUIRE(modulated != nullptr);

    // Count should be preserved
    CHECK(modulated->count() == gradient->count());

    // Stops should be preserved (may be normalized but relative order kept)
    const float* modStops = modulated->stops();
    CHECK(modStops[0] <= modStops[1]);
    CHECK(modStops[1] <= modStops[2]);
}

// Test radial gradient modulation.
TEST_CASE("gradient-radial-modulated", "[RiveRenderer][opacity][gradient]")
{
    ColorInt colors[] = {0x80ff0000, 0x80ffffff};
    float stops[] = {0.0f, 1.0f};

    auto gradient = Gradient::MakeRadial(50, 50, 50, colors, stops, 2);
    REQUIRE(gradient != nullptr);
    CHECK(gradient->paintType() == PaintType::radialGradient);

    auto modulated = gradient->getModulated(0.5f);
    REQUIRE(modulated != nullptr);
    CHECK(modulated->paintType() == PaintType::radialGradient);

    // Original alpha was 0x80 (128), modulating by 0.5 gives ~64
    const ColorInt* modColors = modulated->colors();
    CHECK(colorAlpha(modColors[0]) == 64);
    CHECK(colorAlpha(modColors[1]) == 64);
}

// Test that drawing with gradient and modulated opacity works.
TEST_CASE("draw-path-gradient-modulated", "[RiveRenderer][opacity]")
{
    auto ctx = RenderContextNULL::MakeContext();
    ctx->beginFrame(s_frameDescriptor);

    auto path =
        ctx->makeRenderPath(make_rect({0, 0, 100, 100}), FillRule::nonZero);

    ColorInt colors[] = {0xffff0000, 0xff0000ff};
    float stops[] = {0.0f, 1.0f};
    auto gradient = ctx->makeLinearGradient(0, 0, 100, 0, colors, stops, 2);

    auto paint = ctx->makeRenderPaint();
    paint->shader(gradient);

    RiveRenderer renderer(ctx.get());

    // Draw with modulated opacity
    renderer.save();
    renderer.modulateOpacity(0.5f);
    CHECK(renderer.currentModulatedOpacity() == Approx(0.5f));
    renderer.drawPath(path.get(), paint.get());
    renderer.restore();

    // Draw without modulated opacity
    CHECK(renderer.currentModulatedOpacity() == Approx(1.0f));
    renderer.drawPath(path.get(), paint.get());

    flushFrame(ctx.get());
}

// Test deeply nested opacity modulation.
TEST_CASE("modulate-opacity-deeply-nested", "[RiveRenderer][opacity]")
{
    auto ctx = RenderContextNULL::MakeContext();
    ctx->beginFrame(s_frameDescriptor);

    auto path =
        ctx->makeRenderPath(make_rect({0, 0, 100, 100}), FillRule::nonZero);
    auto paint = ctx->makeRenderPaint();
    paint->color(0xffffffff);

    RiveRenderer renderer(ctx.get());

    // Create deep nesting of opacity modulation
    const int depth = 10;
    for (int i = 0; i < depth; ++i)
    {
        renderer.save();
        renderer.modulateOpacity(0.9f);
    }

    // Effective opacity is 0.9^10 ≈ 0.3486784401
    CHECK(renderer.currentModulatedOpacity() == Approx(0.3486784401f));
    renderer.drawPath(path.get(), paint.get());

    // Unwind the stack
    for (int i = 0; i < depth; ++i)
    {
        renderer.restore();
    }

    // Now draw at full opacity
    CHECK(renderer.currentModulatedOpacity() == Approx(1.0f));
    renderer.drawPath(path.get(), paint.get());

    flushFrame(ctx.get());
}

// Test that modulateOpacity works correctly with clipping.
TEST_CASE("modulate-opacity-with-clipping", "[RiveRenderer][opacity]")
{
    auto ctx = RenderContextNULL::MakeContext();
    ctx->beginFrame(s_frameDescriptor);

    auto clipPath =
        ctx->makeRenderPath(make_oval({10, 10, 90, 90}), FillRule::nonZero);
    auto drawPath =
        ctx->makeRenderPath(make_rect({0, 0, 100, 100}), FillRule::nonZero);
    auto paint = ctx->makeRenderPaint();
    paint->color(0xffffffff);

    RiveRenderer renderer(ctx.get());

    renderer.save();
    renderer.clipPath(clipPath.get());
    renderer.modulateOpacity(0.5f);
    CHECK(renderer.currentModulatedOpacity() == Approx(0.5f));
    renderer.drawPath(drawPath.get(), paint.get());
    renderer.restore();

    flushFrame(ctx.get());
}

// Test that modulateOpacity works with transforms.
TEST_CASE("modulate-opacity-with-transform", "[RiveRenderer][opacity]")
{
    auto ctx = RenderContextNULL::MakeContext();
    ctx->beginFrame(s_frameDescriptor);

    auto path =
        ctx->makeRenderPath(make_rect({0, 0, 50, 50}), FillRule::nonZero);
    auto paint = ctx->makeRenderPaint();
    paint->color(0xffffffff);

    RiveRenderer renderer(ctx.get());

    renderer.save();
    renderer.transform(Mat2D::fromTranslate(25, 25));
    renderer.transform(Mat2D::fromScale(0.5f, 0.5f));
    renderer.modulateOpacity(0.75f);
    CHECK(renderer.currentModulatedOpacity() == Approx(0.75f));
    renderer.drawPath(path.get(), paint.get());
    renderer.restore();

    flushFrame(ctx.get());
}

// Test gradient single-entry cache behavior with repeated access.
TEST_CASE("gradient-modulated-cache-repeated-access",
          "[RiveRenderer][opacity][gradient]")
{
    ColorInt colors[] = {0xffff0000, 0xff00ff00};
    float stops[] = {0.0f, 1.0f};

    auto gradient = Gradient::MakeLinear(0, 0, 100, 0, colors, stops, 2);
    REQUIRE(gradient != nullptr);

    // Create a modulated gradient with opacity 0.5
    auto modulated05 = gradient->getModulated(0.5f);
    CHECK(modulated05 != nullptr);

    // Access same opacity multiple times - should return cached version
    for (int i = 0; i < 10; ++i)
    {
        auto m = gradient->getModulated(0.5f);
        CHECK(m.get() == modulated05.get());
    }

    // Switch to different opacity - replaces cache
    auto modulated025 = gradient->getModulated(0.25f);
    CHECK(modulated025 != nullptr);
    CHECK(modulated025.get() != modulated05.get());

    // Repeated access to new opacity returns cached version
    for (int i = 0; i < 10; ++i)
    {
        auto m = gradient->getModulated(0.25f);
        CHECK(m.get() == modulated025.get());
    }
}

// Test that stroke paths work with modulated opacity.
TEST_CASE("modulate-opacity-stroke", "[RiveRenderer][opacity]")
{
    auto ctx = RenderContextNULL::MakeContext();
    ctx->beginFrame(s_frameDescriptor);

    auto path =
        ctx->makeRenderPath(make_line(10, 50, 90, 50), FillRule::nonZero);
    auto paint = ctx->makeRenderPaint();
    paint->style(RenderPaintStyle::stroke);
    paint->thickness(5.0f);
    paint->color(0xffffffff);

    RiveRenderer renderer(ctx.get());

    renderer.save();
    renderer.modulateOpacity(0.5f);
    CHECK(renderer.currentModulatedOpacity() == Approx(0.5f));
    renderer.drawPath(path.get(), paint.get());
    renderer.restore();

    flushFrame(ctx.get());
}

// Test edge case: very small opacity values.
TEST_CASE("modulate-opacity-very-small", "[RiveRenderer][opacity]")
{
    auto ctx = RenderContextNULL::MakeContext();
    ctx->beginFrame(s_frameDescriptor);

    auto path =
        ctx->makeRenderPath(make_rect({0, 0, 100, 100}), FillRule::nonZero);
    auto paint = ctx->makeRenderPaint();
    paint->color(0xffffffff);

    RiveRenderer renderer(ctx.get());

    renderer.save();
    renderer.modulateOpacity(0.001f);
    CHECK(renderer.currentModulatedOpacity() == Approx(0.001f));
    renderer.drawPath(path.get(), paint.get());
    renderer.restore();

    flushFrame(ctx.get());
}

// Test that negative opacity clamps to 0.
TEST_CASE("modulate-opacity-negative-clamps-to-zero", "[RiveRenderer][opacity]")
{
    auto ctx = RenderContextNULL::MakeContext();
    ctx->beginFrame(s_frameDescriptor);

    auto path =
        ctx->makeRenderPath(make_rect({0, 0, 100, 100}), FillRule::nonZero);
    auto paint = ctx->makeRenderPaint();
    paint->color(0xffffffff);

    RiveRenderer renderer(ctx.get());

    // Negative opacity should clamp to 0
    renderer.save();
    renderer.modulateOpacity(-0.5f);
    CHECK(renderer.currentModulatedOpacity() == 0.0f);
    renderer.drawPath(path.get(), paint.get());
    renderer.restore();

    // Very negative value should also clamp to 0
    renderer.save();
    renderer.modulateOpacity(-100.0f);
    CHECK(renderer.currentModulatedOpacity() == 0.0f);
    renderer.restore();

    // After restore, opacity should be back to 1.0
    CHECK(renderer.currentModulatedOpacity() == 1.0f);

    renderer.save();
    renderer.modulateOpacity(-0.5f);
    renderer.modulateOpacity(-0.5f);
    CHECK(renderer.currentModulatedOpacity() == 0.0f);
    renderer.drawPath(path.get(), paint.get());
    renderer.restore();
    // After restore, opacity should be back to 1.0
    CHECK(renderer.currentModulatedOpacity() == 1.0f);

    flushFrame(ctx.get());
}
