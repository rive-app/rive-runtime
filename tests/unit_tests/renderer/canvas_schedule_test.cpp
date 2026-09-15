/*
 * Copyright 2026 Rive
 */

// Sampler canvases replay after the canvases they sample regardless of
// record order. Pure byte math, no GPU.

#include "deferred_test_sink.hpp"
#include "rive/renderer/cmd/canvas_schedule.hpp"
#include "rive/renderer/cmd/deferred_replayer.hpp"
#include "rive/renderer/render_canvas.hpp"

#include <catch.hpp>

using namespace rive;
using namespace rive::cmd;
using Target = DeferredSegment::Target;

namespace
{
// A hand-built 2D stream plus its canvas segments.
struct StreamBuilder
{
    std::vector<uint8_t> bytes;
    std::vector<DeferredSegment> segments;

    template <typename POD> void append(RenderCmd c, const POD& pod)
    {
        bytes.push_back(static_cast<uint8_t>(c));
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&pod);
        bytes.insert(bytes.end(), p, p + sizeof(POD));
    }

    // Records a canvas bracket holding the given flagged image samples.
    void canvasRange(uint64_t canvasId,
                     std::initializer_list<uint64_t> sampledCanvasIds)
    {
        uint32_t begin = static_cast<uint32_t>(bytes.size());
        // Noise the walker must skip.
        DrawPathPOD path = {};
        append(RenderCmd::drawPath, path);
        for (uint64_t sampled : sampledCanvasIds)
        {
            DrawImagePOD draw = {};
            draw.image = kCanvasHandleFlag | static_cast<RenderHandle>(sampled);
            append(RenderCmd::drawImage, draw);
        }
        segments.push_back({Target::canvas,
                            canvasId,
                            begin,
                            static_cast<uint32_t>(bytes.size())});
    }

    // A foreign image draw that is not a written canvas (host image).
    void canvasRangeSamplingForeign(uint64_t canvasId, uint32_t foreignIndex)
    {
        uint32_t begin = static_cast<uint32_t>(bytes.size());
        DrawImagePOD draw = {};
        draw.image = kCanvasHandleFlag | foreignIndex;
        append(RenderCmd::drawImage, draw);
        segments.push_back({Target::canvas,
                            canvasId,
                            begin,
                            static_cast<uint32_t>(bytes.size())});
    }

    CanvasSchedule schedule() const
    {
        return scheduleCanvases(Span<const uint8_t>(bytes.data(), bytes.size()),
                                segments);
    }
};
} // namespace

TEST_CASE("in-order sampler keeps record order", "[cmd][canvas-dag]")
{
    StreamBuilder b;
    b.canvasRange(1, {});  // A writes
    b.canvasRange(2, {1}); // B samples A, recorded after
    auto s = b.schedule();
    REQUIRE(s.order == std::vector<uint64_t>{1, 2});
    CHECK_FALSE(s.hadCycle);
    CHECK_FALSE(s.multiWriteFallback);
}

TEST_CASE("reader recorded before its writer reorders", "[cmd][canvas-dag]")
{
    StreamBuilder b;
    b.canvasRange(2, {1}); // B samples A but records first
    b.canvasRange(1, {});  // A writes
    auto s = b.schedule();
    REQUIRE(s.order == std::vector<uint64_t>{1, 2});
    CHECK_FALSE(s.hadCycle);
}

TEST_CASE("reversed three-canvas chain schedules writer first",
          "[cmd][canvas-dag]")
{
    StreamBuilder b;
    b.canvasRange(3, {2}); // C samples B
    b.canvasRange(2, {1}); // B samples A
    b.canvasRange(1, {});  // A writes last in record order
    auto s = b.schedule();
    REQUIRE(s.order == std::vector<uint64_t>{1, 2, 3});
}

TEST_CASE("cycle demotes to record order and flags", "[cmd][canvas-dag]")
{
    StreamBuilder b;
    b.canvasRange(1, {2}); // A samples B
    b.canvasRange(2, {1}); // B samples A
    auto s = b.schedule();
    REQUIRE(s.order == std::vector<uint64_t>{1, 2});
    CHECK(s.hadCycle);
}

TEST_CASE("self sample is a demoted edge, not a reorder", "[cmd][canvas-dag]")
{
    StreamBuilder b;
    b.canvasRange(1, {1});
    auto s = b.schedule();
    REQUIRE(s.order == std::vector<uint64_t>{1});
    CHECK(s.hadCycle);
}

TEST_CASE("sampling an unwritten id adds no edge", "[cmd][canvas-dag]")
{
    StreamBuilder b;
    b.canvasRangeSamplingForeign(1, 7); // host image or unwritten canvas
    b.canvasRange(2, {});
    auto s = b.schedule();
    REQUIRE(s.order == std::vector<uint64_t>{1, 2});
    CHECK_FALSE(s.hadCycle);
}

TEST_CASE("read between two writes of one canvas falls back",
          "[cmd][canvas-dag]")
{
    StreamBuilder b;
    b.canvasRange(1, {});  // A@v1
    b.canvasRange(2, {1}); // B samples A mid-frame
    b.canvasRange(1, {});  // A writes again
    auto s = b.schedule();
    REQUIRE(s.order == std::vector<uint64_t>{1, 2});
    CHECK(s.multiWriteFallback);
}

TEST_CASE("drawImageMesh creates edges like drawImage", "[cmd][canvas-dag]")
{
    StreamBuilder b;
    uint32_t begin = static_cast<uint32_t>(b.bytes.size());
    DrawImageMeshPOD mesh = {};
    mesh.image = kCanvasHandleFlag | 1u;
    b.append(RenderCmd::drawImageMesh, mesh);
    b.segments.push_back(
        {Target::canvas, 2, begin, static_cast<uint32_t>(b.bytes.size())});
    b.canvasRange(1, {});
    auto s = b.schedule();
    REQUIRE(s.order == std::vector<uint64_t>{1, 2});
}

TEST_CASE("independent canvases keep record order among themselves",
          "[cmd][canvas-dag]")
{
    StreamBuilder b;
    b.canvasRange(3, {});
    b.canvasRange(1, {5}); // samples a later writer
    b.canvasRange(4, {});
    b.canvasRange(5, {});
    auto s = b.schedule();
    // 5 must precede 1; 3 and 4 stay put relative to everyone they can.
    REQUIRE(s.order == std::vector<uint64_t>{3, 4, 5, 1});
}

namespace
{
struct FakeTarget : gpu::RenderTarget
{
    FakeTarget() : RenderTarget(8, 8) {}
};

// Logs canvas frame open order; canvas draws drop against the null renderer.
class OrderSink : public deferred_test::TestSink
{
public:
    std::vector<gpu::RenderCanvas*> opened;
    Renderer* beginCanvasContent(gpu::RenderCanvas* canvas, uint32_t) override
    {
        opened.push_back(canvas);
        return nullptr;
    }
};

rcp<gpu::RenderCanvas> fakeCanvas()
{
    auto canvas = make_rcp<gpu::RenderCanvas>(8, 8);
    canvas->setBacking(nullptr, make_rcp<FakeTarget>());
    return canvas;
}
} // namespace

TEST_CASE("replay opens the sampled canvas before its reader despite record "
          "order",
          "[cmd][canvas-dag]")
{
    DeferredSession session(rive::ore::ReplayCaps{});
    auto canvasA = fakeCanvas();
    auto canvasB = fakeCanvas();

    // B samples A but records first, exactly as a script may issue it.
    Renderer* b = session.beginCanvasContent(canvasB.get(), 0);
    b->drawImage(canvasA->renderImage(), {}, BlendMode::srcOver, 1.0f);
    session.endCanvasContent(canvasB.get());
    Renderer* a = session.beginCanvasContent(canvasA.get(), 0);
    auto paint = session.makeRenderPaint();
    auto path = session.makeEmptyRenderPath();
    a->drawPath(path.get(), paint.get());
    session.endCanvasContent(canvasA.get());
    session.closeOpenRange();

    auto frame = snapshotFrame(session);
    OrderSink sink;
    DeferredReplayer replayer;
    replayer.replayFrame(frame, sink);

    REQUIRE(sink.opened.size() == 2);
    CHECK(sink.opened[0] == canvasA.get());
    CHECK(sink.opened[1] == canvasB.get());
}

TEST_CASE("a canvas only frame still opens a screen frame", "[cmd][canvas-dag]")
{
    DeferredSession session(rive::ore::ReplayCaps{});
    auto canvas = fakeCanvas();

    Renderer* c = session.beginCanvasContent(canvas.get(), 0);
    auto paint = session.makeRenderPaint();
    auto path = session.makeEmptyRenderPath();
    c->drawPath(path.get(), paint.get());
    session.endCanvasContent(canvas.get());
    session.closeOpenRange();

    auto frame = snapshotFrame(session);
    OrderSink sink;
    DeferredReplayer replayer;
    replayer.replayFrame(frame, sink);

    REQUIRE(sink.opened.size() == 1);
    // The screen frame is where the host's clear and present live, so a frame
    // that only fills canvases still owes its target one.
    CHECK(sink.openedTargets() == 1);
}

TEST_CASE("a frame that only creates resources opens no screen frame",
          "[cmd][canvas-dag]")
{
    DeferredSession session(rive::ore::ReplayCaps{});
    // Creates land outside every renderer. Attributing them would open a
    // target that drew nothing, which is why they stay unattributed.
    auto paint = session.makeRenderPaint();
    auto path = session.makeEmptyRenderPath();
    session.closeOpenRange();

    auto frame = snapshotFrame(session);
    REQUIRE_FALSE(frame.commands.empty());

    OrderSink sink;
    DeferredReplayer replayer;
    replayer.replayFrame(frame, sink);

    CHECK(sink.openedTargets() == 0);
}
