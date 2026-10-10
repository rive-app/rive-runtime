/*
 * Copyright 2026 Rive
 */

// Recording allocates no canvas backing, so replay backs each canvas on the
// sink's render context, and a sink without one drops the content.

#ifdef RIVE_CANVAS

#include "common/render_context_null.hpp"
#include "deferred_test_sink.hpp"
#include "rive/renderer/cmd/deferred_replayer.hpp"
#include "rive/renderer/cmd/deferred_session.hpp"
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/render_context.hpp"

#include <catch.hpp>

using namespace rive;
using namespace rive::cmd;

namespace
{
class BackingSink : public deferred_test::TestSink
{
public:
    explicit BackingSink(std::unique_ptr<gpu::RenderContext> rc) :
        m_rc(std::move(rc))
    {}

    std::vector<gpu::RenderCanvas*> contentOpened;

    gpu::RenderContext* renderContext() override { return m_rc.get(); }

    Renderer* beginCanvasContent(gpu::RenderCanvas* canvas, uint32_t) override
    {
        contentOpened.push_back(canvas);
        return nullptr;
    }

private:
    std::unique_ptr<gpu::RenderContext> m_rc;
};

void recordCanvasDraw(DeferredSession& session, gpu::RenderCanvas* canvas)
{
    Renderer* content = session.beginCanvasContent(canvas, 0);
    auto paint = session.makeRenderPaint();
    auto path = session.makeEmptyRenderPath();
    content->drawPath(path.get(), paint.get());
    session.endCanvasContent(canvas);
    session.closeOpenRange();
}
} // namespace

TEST_CASE("replay backs a recorded canvas on the sink's render context",
          "[deferred][canvas-backing]")
{
    DeferredSession session(rive::ore::ReplayCaps{});
    auto canvas = make_rcp<gpu::RenderCanvas>(8, 8);
    recordCanvasDraw(session, canvas.get());
    REQUIRE(!canvas->isBacked());

    BackingSink sink(RenderContextNULL::MakeContext());
    DeferredReplayer replayer;
    replayer.replayFrame(snapshotFrame(session), sink);

    CHECK(canvas->isBacked());
    CHECK(sink.contentOpened == std::vector<gpu::RenderCanvas*>{canvas.get()});
}

TEST_CASE("a sink with no render context leaves the canvas unbacked",
          "[deferred][canvas-backing]")
{
    DeferredSession session(rive::ore::ReplayCaps{});
    auto canvas = make_rcp<gpu::RenderCanvas>(8, 8);
    recordCanvasDraw(session, canvas.get());

    BackingSink sink(nullptr);
    DeferredReplayer replayer;
    replayer.replayFrame(snapshotFrame(session), sink);

    CHECK(!canvas->isBacked());
    CHECK(sink.contentOpened.empty());
}

#endif
