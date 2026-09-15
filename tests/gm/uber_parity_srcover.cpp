/*
 * Copyright 2026 Rive
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "common/testing_window.hpp"
#include "rive/renderer.hpp"
#include "rive/renderer/render_context.hpp"
#include "rive/renderer/render_context_impl.hpp"

#include "assets/nomoon.png.hpp"

using namespace rivegm;
using namespace rive;

// Companion to uber_parity: the same left-uber / right-specialized layout,
// but with a srcOver-only scene and dithering disabled. Where uber_parity
// checks that the features an ubershader *uses* match specialized rendering,
// this GM checks that the features an ubershader compiles but does NOT use
// are true no-ops:
//
//   - ENABLE_DITHER: specialized pipelines omit the feature entirely; the
//     ubershader still runs the dither math and must neutralize it via
//     0-value uniforms (ditherMode == none zeroes ditherScale/ditherBias).
//     This zero-uniform path has no other test coverage.
//   - ENABLE_ADVANCED_BLEND: compiled in the ubershader (flipping the whole
//     paint path to unmultiplied colors), but every draw here is srcOver.
//   - ENABLE_EVEN_ODD, ENABLE_CLIP_RECT, ENABLE_CLIPPING: compiled in the
//     ubershader; the scene contains no even-odd fills, no clipRect, and no
//     clipping.
//
// The specialized half compiles none of the above. The halves must match
// within a few LSB of premult/unmult rounding on translucent content; any
// structural divergence means an enabled-but-unused ubershader feature is
// not a no-op.
//
// On backends that can't switch shader compilation mode at runtime, both
// cells render specialized and trivially match.
constexpr float CELL = 300.f;

static void draw_srcover_scene(Renderer* renderer, float ox)
{
    Factory* factory = TestingWindow::Get()->factory();
    constexpr float stops[2] = {0.f, 1.f};

    renderer->save();
    renderer->translate(ox, 0);

    // Opaque horizontal gradient base. Banding on this smooth ramp must be
    // identical in both halves: neither half may dither (the ubershader half
    // has the dither code compiled and must no-op it).
    AABB base = {20.f, 20.f, 280.f, 280.f};
    ColorInt baseColors[2] = {0xff2060c0, 0xffc06020};
    Paint basePaint;
    basePaint->shader(factory->makeLinearGradient(base.left(),
                                                  0,
                                                  base.right(),
                                                  0,
                                                  baseColors,
                                                  stops,
                                                  2));
    renderer->drawPath(PathBuilder::Rect(base), basePaint);

    // Translucent vertical gradient fading to near-zero alpha: the
    // premultiplied path rounds rgb*a per fragment, the unmultiplied path
    // premultiplies after coverage; low-alpha texels expose the difference.
    AABB overlay = {50.f, 50.f, 250.f, 250.f};
    ColorInt overlayColors[2] = {0xccff2020, 0x0dffff20};
    Paint overlayPaint;
    overlayPaint->shader(factory->makeLinearGradient(0,
                                                     overlay.top(),
                                                     0,
                                                     overlay.bottom(),
                                                     overlayColors,
                                                     stops,
                                                     2));
    renderer->drawPath(PathBuilder::Rect(overlay), overlayPaint);

    // Image at low opacity: in the unmultiplied path, image texels round-trip
    // through unmultiply_rgb and a later premultiply.
    auto img = LoadImage(assets::nomoon_png());
    if (img != nullptr)
    {
        renderer->save();
        renderer->translate(60.f, 120.f);
        float scale = 140.f / std::max(img->width(), img->height());
        renderer->scale(scale, scale);
        renderer->drawImage(img.get(),
                            ImageSampler::LinearClamp(),
                            BlendMode::srcOver,
                            .35f);
        renderer->restore();
    }

    // Thin translucent strokes over everything: AA edge rounding.
    for (int i = 0; i < 3; ++i)
    {
        Paint strokePaint;
        strokePaint->style(RenderPaintStyle::stroke);
        strokePaint->thickness(1.5f);
        strokePaint->color(0x8020ff80);
        renderer->drawPath(PathBuilder::Circle(150.f, 150.f, 60.f + 25.f * i),
                           strokePaint);
    }

    renderer->restore();
}

class uber_parity_srcover_GM : public rivegm::GM
{
public:
    uber_parity_srcover_GM() : GM(600, 300) {}
    ColorInt clearColor() const override { return 0xff404040; }

    void updateFrameOptions(TestingWindow::FrameOptions* options) const override
    {
        // Makes GM::run restore the process-wide mode after endFrame(); the
        // per-half modes below override it in between.
        options->shaderCompilationMode =
            rive::gpu::ShaderCompilationMode::onlyUbershaders;
    }

    void onDraw(rive::Renderer* renderer) override
    {
        gpu::RenderContext* renderContext =
            TestingWindow::Get()->renderContext();
        if (renderContext == nullptr)
        {
            // No runtime mode switching on this backend; both cells render
            // the same way and trivially match.
            draw_srcover_scene(renderer, 0);
            draw_srcover_scene(renderer, CELL);
            return;
        }

        gpu::RenderContext::FrameDescriptor frameDescriptor =
            renderContext->frameDescriptor();
        frameDescriptor.loadAction = gpu::LoadAction::preserveRenderTarget;
        // Dither off: the specialized half drops ENABLE_DITHER from its
        // pipelines; the ubershader half keeps the feature compiled and must
        // neutralize it through the zeroed dither uniforms.
        frameDescriptor.ditherMode = gpu::DitherMode::none;

        for (int i = 0; i < 2; ++i)
        {
            // Flushes the previous half (and initially the clear) under the
            // mode that was live when its draws were recorded.
            TestingWindow::Get()->flushPLSContext();
            renderContext->impl()->testingOnly_setShaderCompilationMode(
                i == 0 ? gpu::ShaderCompilationMode::onlyUbershaders
                       : gpu::ShaderCompilationMode::alwaysSynchronous);
            renderContext->beginFrame(frameDescriptor);
            draw_srcover_scene(renderer, i * CELL);
        }
        // endFrame() flushes the right half under alwaysSynchronous, then
        // GM::run restores the process-wide mode.
    }
};
GMREGISTER(uber_parity_srcover, return new uber_parity_srcover_GM)
