/*
 * Copyright 2026 Rive
 *
 * A path painted with a canvas, the way an artboard paint source fills a
 * shape: the paint's modulated image is a deferred canvas on its first frame,
 * sampled both on screen and from inside another canvas. The image is set
 * once, before either canvas records -- a paint's modulated image is state --
 * so each canvas draw has to name the canvas its paint samples. The
 * reversed GM records the sampling canvas before the canvas it samples; with
 * the edge seen, both GMs produce identical pixels.
 */

#include "gm.hpp"
#include "gmutils.hpp"

#ifdef RIVE_CANVAS

#include "common/testing_window_deferred_sink.hpp"
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "rive/renderer/cmd/deferred_replayer.hpp"
#include "rive/renderer/cmd/deferred_session.hpp"

using namespace rivegm;
using namespace rive;
using namespace rive::gpu;

namespace
{
rcp<RenderPath> ovalPath(rive::cmd::DeferredSession& session, AABB bounds)
{
  RawPath raw;
  raw.addOval(bounds);
  return session.makeRenderPath(raw, FillRule::nonZero);
}

void drawCanvasImage(Renderer* r, RenderCanvas* canvas, float x, float y)
{
  r->save();
  r->translate(x, y);
  r->drawImage(
      canvas->renderImage(),
      {.filter = ImageFilter::nearest},
      BlendMode::srcOver,
      1.0f);
  r->restore();
}
} // namespace

class CanvasPaintImageGM : public GM
{
public:
  CanvasPaintImageGM(bool reversed) : GM(256, 256), m_reversed(reversed) {}

  ColorInt clearColor() const override { return 0xff202028; }

  void onDraw(Renderer*) override
  {
    auto rc = TestingWindow::Get()->renderContext();
    if (!rc)
    {
      return;
    }
    auto mainDesc = rc->frameDescriptor();

    rive::cmd::DeferredSession session(rive::ore::ReplayCaps{});
    session.bindRenderContext(rc);
    rive::cmd::DeferredReplayer replayer;

    // Unbacked until the replay backs them -- the first-frame case a
    // paint's modulated image used to miss, binding no texture at all.
    auto pattern = session.makeContentCanvas(32, 32);
    auto tile = session.makeContentCanvas(128, 128);
    if (!pattern || !tile)
    {
      return;
    }

    auto green = session.makeRenderPaint();
    green->color(0xff30c060);
    auto patterned = session.makeRenderPaint();
    patterned->color(0xffffffff);
    // Set once, up front, outside any canvas: the draws below only refer
    // to the paint. One pattern tile per 32 units, repeated.
    patterned->modulatedImage(
        pattern->renderImage(),
        {
            .wrapX = ImageWrap::repeat,
            .wrapY = ImageWrap::repeat,
            .filter = ImageFilter::nearest,
        },
        Mat2D::fromScale(32.0f, 32.0f));

    auto dot = ovalPath(session, {4, 4, 28, 28});
    auto big = ovalPath(session, {8, 8, 120, 120});

    auto recordPattern = [&]() {
      Renderer* r = session.beginCanvasContent(pattern.get(), 0xff103050);
      r->drawPath(dot.get(), green.get());
      session.endCanvasContent(pattern.get());
    };
    auto recordTile = [&]() {
      Renderer* r = session.beginCanvasContent(tile.get(), 0xff501030);
      r->drawPath(big.get(), patterned.get());
      session.endCanvasContent(tile.get());
    };
    if (m_reversed)
    {
      recordTile();
      recordPattern();
    }
    else
    {
      recordPattern();
      recordTile();
    }

    auto screen = session.makeScreenRenderer();
    // The canvas that sampled the pattern through a paint...
    drawCanvasImage(screen.get(), tile.get(), 0, 64);
    // ...and the pattern painted straight onto the screen.
    screen->save();
    screen->translate(128, 64);
    screen->drawPath(big.get(), patterned.get());
    screen->restore();

    rive::cmd::DeferredFrame frame = rive::cmd::snapshotFrame(session);
    session.resetFrame();
    rive_tests::TestingWindowFrameSink sink(TestingWindow::Get(), rc, mainDesc);
    replayer.replayFrame(frame, sink);
  }

private:
  bool m_reversed;
};

GMREGISTER(canvas_paint_image, return new CanvasPaintImageGM(false))
GMREGISTER(canvas_paint_image_reversed, return new CanvasPaintImageGM(true))

#else
// Canvas disabled: nothing to register.

#endif
