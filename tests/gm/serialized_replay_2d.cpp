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

        RawPath shape = kShape();

        if (m_replay)
        {
            SerializingFactory sf;
            auto recorder = sf.makeRenderer();
            auto path = sf.makeRenderPath(shape, FillRule::nonZero);
            auto paint = sf.makeRenderPaint();
            paint->color(0xFFFFA030);
            paint->style(RenderPaintStyle::fill);
            recorder->drawPath(path.get(), paint.get());

            replaySerializedCommands(sf.bytes(), factory, renderer);
        }
        else
        {
            auto path = factory->makeRenderPath(shape, FillRule::nonZero);
            auto paint = factory->makeRenderPaint();
            paint->color(0xFFFFA030);
            paint->style(RenderPaintStyle::fill);
            renderer->drawPath(path.get(), paint.get());
        }
    }

private:
    bool m_replay;
};

GMREGISTER(serialized_replay_2d_immediate,
           return new SerializedReplay2DGM(false))
GMREGISTER(serialized_replay_2d, return new SerializedReplay2DGM(true))
