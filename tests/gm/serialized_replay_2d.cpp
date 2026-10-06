/*
 * Copyright 2026 Rive
 *
 * Draws the same shape immediately and via a SerializingFactory recording
 * replayed against the real factory and renderer. The goldens must be byte
 * identical since the recorded stream is the deferral contract.
 */

#include "gm.hpp"
#include "gmutils.hpp"
#include "utils/serializing_factory.hpp"
#include "utils/serialized_replay.hpp"
#include "rive/math/raw_path.hpp"

using namespace rivegm;
using namespace rive;

static RawPath kShape()
{
    // Nonconvex polygon so winding is exercised.
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

static const Mat2D GradientTransform = {0.8f, 0.3f, -0.3f, 0.8f, 20, -10};

static void drawScene(Factory* factory, Renderer* renderer)
{
    RawPath shape = kShape();
    auto path = factory->makeRenderPath(shape, FillRule::nonZero);
    auto paint = factory->makeRenderPaint();
    paint->color(0xFFFFA030);
    paint->style(RenderPaintStyle::fill);
    renderer->drawPath(path.get(), paint.get());

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
    gradPaint->shaderTransform(GradientTransform);
    renderer->drawPath(boxPath.get(), gradPaint.get());
}

class SerializedReplay2DGM : public GM
{
public:
    SerializedReplay2DGM(bool replay) : GM(256, 256), m_replay(replay) {}

    ColorInt clearColor() const override { return 0xff202028; }

    void onDraw(rive::Renderer* renderer) override
    {
        Factory* factory = TestingWindow::Get()->factory();
        if (!factory)
            return;

        if (m_replay)
        {
            SerializingFactory sf;
            auto recorder = sf.makeRenderer();
            drawScene(&sf, recorder.get());

            replaySerializedCommands(sf.bytes(), factory, renderer);
        }
        else
        {
            drawScene(factory, renderer);
        }
    }

private:
    bool m_replay;
};

GMREGISTER(serialized_replay_2d_immediate,
           return new SerializedReplay2DGM(false))
GMREGISTER(serialized_replay_2d, return new SerializedReplay2DGM(true))
