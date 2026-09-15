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

// Renders the same scene twice, side by side: the LEFT cell through
// fully-featured ubershader pipelines and the RIGHT cell through specialized
// pipelines (via a render pass break between the two). The cells must match:
// a few LSB of rounding difference is expected on translucent content (the
// premultiplied and unmultiplied paint paths round differently), but any
// visible or structural divergence is an ubershader bug. The golden pins
// both halves, so a regression in either flavor diffs immediately.
//
// The scene is chosen to stress where the two pipeline flavors are most
// likely to diverge: ubershaders compile ENABLE_ADVANCED_BLEND, which flips
// the entire paint path to unmultiplied colors. So the scene leans on
// content whose premultiplied vs unmultiplied evaluation differs in rounding
// and codepath:
//   - translucent gradients fading to very low alpha (premultiply rounding),
//   - an image drawn at low opacity (the unmultiplied path round-trips image
//     texels through unmultiply/premultiply),
//   - an advanced blend (multiply) overlapping srcOver content (forces the
//     dst-read machinery alongside the plain path),
//   - thin strokes over the gradient (AA edge rounding).
//
// On backends that can't switch shader compilation mode at runtime, both
// cells render specialized and trivially match.
constexpr float CELL = 300.f;

static void draw_scene(Renderer* renderer, float ox)
{
    Factory* factory = TestingWindow::Get()->factory();
    constexpr float stops[2] = {0.f, 1.f};

    renderer->save();
    renderer->translate(ox, 0);

    // Opaque horizontal gradient base.
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

    // Multiply square overlapping both gradients: advanced blend content in
    // the same scene as plain srcOver draws.
    Paint multiplyPaint;
    multiplyPaint->color(0xff90b0d0);
    multiplyPaint->blendMode(BlendMode::multiply);
    renderer->drawPath(PathBuilder::Rect({140.f, 80.f, 260.f, 200.f}),
                       multiplyPaint);

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

class uber_parity_GM : public rivegm::GM
{
public:
    uber_parity_GM() : GM(600, 300) {}
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
            draw_scene(renderer, 0);
            draw_scene(renderer, CELL);
            return;
        }

        gpu::RenderContext::FrameDescriptor frameDescriptor =
            renderContext->frameDescriptor();
        frameDescriptor.loadAction = gpu::LoadAction::preserveRenderTarget;

        for (int i = 0; i < 2; ++i)
        {
            // Flushes the previous half (and initially the clear) under the
            // mode that was live when its draws were recorded.
            TestingWindow::Get()->flushPLSContext();
            renderContext->impl()->testingOnly_setShaderCompilationMode(
                i == 0 ? gpu::ShaderCompilationMode::onlyUbershaders
                       : gpu::ShaderCompilationMode::alwaysSynchronous);
            renderContext->beginFrame(frameDescriptor);
            draw_scene(renderer, i * CELL);
        }
        // endFrame() flushes the right half under alwaysSynchronous, then
        // GM::run restores the process-wide mode.
    }
};
GMREGISTER(uber_parity, return new uber_parity_GM)
