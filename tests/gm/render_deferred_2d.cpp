/*
 * Copyright 2026 Rive
 *
 * Draws the same scene immediately and via a DeferredFactory recording
 * replayed against the real factory and renderer. The goldens must be byte
 * identical.
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "rive/renderer/cmd/deferred_render_factory.hpp"
#include "rive/renderer/cmd/render_replay.hpp"
#include "rive/math/raw_path.hpp"
#include "rive/math/mat2d.hpp"
#include "rive/shapes/paint/image_sampler.hpp"
#include "assets/batdude.png.hpp"

using namespace rivegm;
using namespace rive;

static RawPath kShape()
{
    RawPath p;
    p.move({40, 40});
    p.line({160, 70});
    p.line({120, 110});
    p.line({200, 200});
    p.line({120, 160});
    p.line({60, 210});
    p.close();
    return p;
}

// Fill and stroke so all paint properties get recorded.
static void drawScene(Factory* factory, Renderer* renderer)
{
    RawPath shape = kShape();
    auto path = factory->makeRenderPath(shape, FillRule::nonZero);

    auto fill = factory->makeRenderPaint();
    fill->style(RenderPaintStyle::fill);
    fill->color(0xFFFFA030);
    renderer->drawPath(path.get(), fill.get());

    auto stroke = factory->makeRenderPaint();
    stroke->style(RenderPaintStyle::stroke);
    stroke->color(0xFF3050FF);
    stroke->thickness(10);
    stroke->join(StrokeJoin::round);
    stroke->cap(StrokeCap::round);
    renderer->drawPath(path.get(), stroke.get());

    // Exercises gradient shaders.
    RawPath box;
    box.move({30, 30});
    box.line({226, 30});
    box.line({226, 90});
    box.line({30, 90});
    box.close();
    auto boxPath = factory->makeRenderPath(box, FillRule::nonZero);
    const ColorInt colors[] = {0xFF00E0A0, 0xFFE000A0};
    const float stops[] = {0.0f, 1.0f};
    auto grad = factory->makeLinearGradient(30, 30, 226, 90, colors, stops, 2);
    auto gradPaint = factory->makeRenderPaint();
    gradPaint->style(RenderPaintStyle::fill);
    gradPaint->shader(grad);
    renderer->drawPath(boxPath.get(), gradPaint.get());

    // Exercises paths built verb by verb.
    auto tri = factory->makeEmptyRenderPath();
    tri->fillRule(FillRule::nonZero);
    tri->moveTo(20, 195);
    tri->lineTo(80, 195);
    tri->lineTo(50, 150);
    tri->close();
    auto triPaint = factory->makeRenderPaint();
    triPaint->style(RenderPaintStyle::fill);
    triPaint->color(0xFFFFFFFF);
    renderer->drawPath(tri.get(), triPaint.get());

    // Exercises image decode and draw.
    auto img = factory->decodeImage(assets::batdude_png());
    if (img)
    {
        renderer->save();
        renderer->transform(Mat2D(0.12f, 0, 0, 0.12f, 150, 110));
        renderer->drawImage(img.get(),
                            ImageSampler::LinearClamp(),
                            BlendMode::srcOver,
                            1.0f);
        renderer->restore();
    }
}

class RenderDeferred2DGM : public GM
{
public:
    RenderDeferred2DGM(bool deferred) : GM(256, 256), m_deferred(deferred) {}

    ColorInt clearColor() const override { return 0xff202028; }

    void onDraw(rive::Renderer* renderer) override
    {
        Factory* factory = TestingWindow::Get()->factory();
        if (!factory)
        {
            return;
        }

        if (m_deferred)
        {
            cmd::DeferredFactory df;
            auto dr = df.makeRenderer();
            drawScene(&df, dr.get());
            cmd::replayRenderCommands(factory, renderer, df.commandBuffer());
        }
        else
        {
            drawScene(factory, renderer);
        }
    }

private:
    bool m_deferred;
};

GMREGISTER(render_deferred_2d_immediate, return new RenderDeferred2DGM(false))
GMREGISTER(render_deferred_2d, return new RenderDeferred2DGM(true))
