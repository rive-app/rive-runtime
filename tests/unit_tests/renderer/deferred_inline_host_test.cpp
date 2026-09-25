/*
 * Copyright 2026 Rive
 */

// DeferredInlineHost is the synchronous record and replay lifecycle every
// runtime integration builds on. Recording only, no GPU.

#if defined(RIVE_CANVAS) && defined(RIVE_ORE)

#include "common/render_context_null.hpp"
#include "rive/renderer/cmd/deferred_host.hpp"
#include "rive/renderer/render_canvas.hpp"
#include "utils/serializing_factory.hpp"

#include <catch.hpp>

using namespace rive;

namespace
{
// GPU free HostFrameSink: the factory and ore overrides keep the null render
// context unreachable.
class InlineTestSink : public cmd::HostFrameSink
{
public:
    InlineTestSink(bool openScreen = true) :
        cmd::HostFrameSink(true, 0), m_openScreen(openScreen)
    {}

    SerializingFactory serializingFactory;

    gpu::RenderContext* renderContext() override { return nullptr; }
    Factory* factory() override { return &serializingFactory; }
    ore::Context* oreContext() override { return nullptr; }
    Renderer* beginScreen(uint64_t, bool, uint32_t) override
    {
        if (!m_openScreen)
        {
            return nullptr;
        }
        serializingFactory.frameSize(256, 256);
        serializingFactory.addFrame();
        m_screen = serializingFactory.makeRenderer();
        return m_screen.get();
    }

private:
    bool m_openScreen;
    std::unique_ptr<Renderer> m_screen;
};

// Opens canvas frames on the null device, the only part of the sink that
// needs a render context.
class CanvasModeSink : public cmd::HostFrameSink
{
public:
    CanvasModeSink() :
        cmd::HostFrameSink(true, 0), m_rc(RenderContextNULL::MakeContext())
    {
        m_useExternalCommandBuffer = false;
        m_screenMode.clockwiseFillOverride = true;
    }

    gpu::RenderContext* renderContext() override { return m_rc.get(); }
    ore::Context* oreContext() override { return nullptr; }
    Renderer* beginScreen(uint64_t, bool, uint32_t) override { return nullptr; }

private:
    std::unique_ptr<gpu::RenderContext> m_rc;
};

class MSAACanvasSink : public CanvasModeSink
{
    FrameMode canvasMode(gpu::RenderCanvas*) override
    {
        return {.msaaSampleCount = 4};
    }
};
} // namespace

TEST_CASE("a frame that never opened neither replays nor presents",
          "[cmd][inline_host]")
{
    cmd::DeferredSession session(rive::ore::ReplayCaps{});
    cmd::DeferredInlineHost host;
    host.bindSession(&session);

    InlineTestSink sink;
    int presents = 0;
    CHECK(!host.replayInline(sink, [&] { presents++; }));
    CHECK(presents == 0);
    CHECK(!sink.began());
}

TEST_CASE("a marker only frame consumes the marker without presenting",
          "[cmd][inline_host]")
{
    cmd::DeferredSession session(rive::ore::ReplayCaps{});
    cmd::DeferredInlineHost host;
    host.bindSession(&session);
    host.beginRecord(true, 0xff112233);

    // No draws and no ore content: the replay has no screen segment to open,
    // so the last presented frame stays up.
    InlineTestSink sink;
    int presents = 0;
    CHECK(host.replayInline(sink, [&] { presents++; }));
    CHECK(presents == 0);
    CHECK(!sink.began());
    CHECK(!session.recordedThisFrame());
}

TEST_CASE("a recorded frame replays once and resets the session",
          "[cmd][inline_host]")
{
    cmd::DeferredSession session(rive::ore::ReplayCaps{});
    cmd::DeferredInlineHost host;
    host.bindSession(&session);

    host.beginRecord(true, 0);
    auto paint = session.makeRenderPaint();
    auto path = session.makeEmptyRenderPath();
    host.screenRenderer()->drawPath(path.get(), paint.get());

    InlineTestSink sink;
    int presents = 0;
    CHECK(host.replayInline(sink, [&] { presents++; }));
    CHECK(presents == 1);
    CHECK(sink.began());
    CHECK(!session.recordedThisFrame());

    // Nothing new recorded, so the next flush keeps the last frame up.
    InlineTestSink secondSink;
    CHECK(!host.replayInline(secondSink, [&] { presents++; }));
    CHECK(presents == 1);
}

TEST_CASE("present is skipped when the screen never opens",
          "[cmd][inline_host]")
{
    cmd::DeferredSession session(rive::ore::ReplayCaps{});
    cmd::DeferredInlineHost host;
    host.bindSession(&session);

    host.beginRecord(true, 0);
    auto paint = session.makeRenderPaint();
    auto path = session.makeEmptyRenderPath();
    host.screenRenderer()->drawPath(path.get(), paint.get());

    InlineTestSink sink(/*openScreen=*/false);
    int presents = 0;
    CHECK(host.replayInline(sink, [&] { presents++; }));
    CHECK(presents == 0);
    CHECK(!sink.began());
}

TEST_CASE("a canvas frame opens in the screen's mode", "[cmd][inline_host]")
{
    CanvasModeSink sink;
    gpu::RenderContext* rc = sink.renderContext();
    auto canvas = rc->makeDeferredRenderCanvas(64, 64);
    CHECK(sink.beginCanvasContent(canvas.get(), 0) != nullptr);
    CHECK(rc->frameDescriptor().clockwiseFillOverride);
    CHECK(rc->frameDescriptor().msaaSampleCount == 0);
    CHECK(rc->frameInterlockMode() == gpu::InterlockMode::clockwise);
    sink.endCanvasContent();
}

TEST_CASE("a sink can open a canvas frame in its own mode",
          "[cmd][inline_host]")
{
    MSAACanvasSink sink;
    gpu::RenderContext* rc = sink.renderContext();
    auto canvas = rc->makeDeferredRenderCanvas(64, 64);
    CHECK(sink.beginCanvasContent(canvas.get(), 0) != nullptr);
    CHECK(!rc->frameDescriptor().clockwiseFillOverride);
    CHECK(rc->frameDescriptor().msaaSampleCount == 4);
    CHECK(rc->frameInterlockMode() == gpu::InterlockMode::depthStencil);
    sink.endCanvasContent();
}

#endif // RIVE_CANVAS && RIVE_ORE
