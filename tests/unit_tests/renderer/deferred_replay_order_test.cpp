/*
 * Copyright 2026 Rive
 */

// Segment replay reorders canvas brackets before screen gaps. The replayer
// hoists creates into a record order pass and defers destroys to a trailing
// pass so reordering cannot break mint order or free a slot early.

#include "rive/renderer/cmd/deferred_render_factory.hpp"
#include "rive/renderer/cmd/deferred_replayer.hpp"
#include "rive/renderer/cmd/deferred_session.hpp"
#include "deferred_test_sink.hpp"

#include <catch.hpp>

using namespace rive;
using rive::cmd::DeferredSegment;
using Target = rive::cmd::DeferredSegment::Target;
using deferred_test::TestSink;

namespace
{
// Counts paint creations so tests can assert version materialization.
class CountingFactory : public SerializingFactory
{
public:
    int paintCount = 0;
    rcp<RenderPaint> makeRenderPaint() override
    {
        paintCount++;
        return SerializingFactory::makeRenderPaint();
    }
};
} // namespace

using CountingSink = deferred_test::TestSinkT<CountingFactory>;

TEST_CASE("a create inside a canvas bracket replays in mint order",
          "[deferred][replay][segment]")
{
    cmd::DeferredFactory factory;
    auto& buffer = factory.commandBuffer();
    auto renderer = factory.makeRenderer();

    auto paint = factory.makeRenderPaint();
    auto p1 = factory.makeEmptyRenderPath(); // path id 0, screen phase

    // Path id 1 is minted inside the canvas bracket.
    uint32_t bracketBegin = static_cast<uint32_t>(buffer.commandBytes().size());
    constexpr cmd::RenderHandle kCanvas = 7 | cmd::kCanvasHandleFlag;
    buffer.append(static_cast<uint8_t>(cmd::RenderCmd::canvasContentBegin),
                  cmd::CanvasContentPOD{kCanvas, 0xFF000000});
    auto p2 = factory.makeEmptyRenderPath(); // path id 1, canvas phase
    renderer->drawPath(p2.get(), paint.get());
    buffer.append(static_cast<uint8_t>(cmd::RenderCmd::canvasContentEnd),
                  cmd::ResIdPOD{kCanvas});
    uint32_t bracketEnd = static_cast<uint32_t>(buffer.commandBytes().size());

    // The scheduler runs the bracket first, so without the hoisted create
    // pass path id 1 would replay before id 0 and the screen draws drop.
    renderer->drawPath(p1.get(), paint.get());
    renderer->drawPath(p2.get(), paint.get());

    cmd::DeferredFrame frame;
    auto copy = [](Span<const uint8_t> s) {
        return std::vector<uint8_t>(s.data(), s.data() + s.size());
    };
    frame.commands = copy(buffer.commandBytes());
    frame.blobs = copy(buffer.blobBytes());
    frame.segments = {
        {Target::screen, 0, 0, bracketBegin},
        {Target::canvas, 7, bracketBegin, bracketEnd},
        {Target::screen,
         0,
         bracketEnd,
         static_cast<uint32_t>(frame.commands.size())},
    };

    TestSink sink;
    cmd::DeferredReplayer replayer;
    replayer.replayFrame(frame, sink);
    CHECK(replayer.droppedDraws() == 0);
}

TEST_CASE("interleaved multi-target drawing splits per-renderer ranges",
          "[deferred][replay][segment]")
{
    cmd::DeferredSession session(nullptr);
    auto* screen = session.screenRenderer();
    // Routed canvas recorders like beginCanvasContent hands a script.
    cmd::DeferredRenderer c1(&session.commandBuffer(),
                             &session.canvases(),
                             &session,
                             1);
    cmd::DeferredRenderer c2(&session.commandBuffer(),
                             &session.canvases(),
                             &session,
                             2);

    auto paint = session.makeRenderPaint();
    auto path = session.makeEmptyRenderPath();

    // Attribution is per renderer, so interleaving must record non nested
    // canvas ranges around the screen gaps.
    screen->drawPath(path.get(), paint.get());
    c1.drawPath(path.get(), paint.get());
    c2.drawPath(path.get(), paint.get());
    c1.drawPath(path.get(), paint.get());
    screen->drawPath(path.get(), paint.get());
    session.closeOpenRange();

    std::vector<DeferredSegment> segs;
    for (const auto& s : session.recordedSegments())
    {
        if (s.target == Target::canvas)
        {
            segs.push_back(s);
        }
    }
    REQUIRE(segs.size() == 3);
    CHECK(segs[0].targetId == 1u);
    CHECK(segs[1].targetId == 2u);
    CHECK(segs[2].targetId == 1u);
    for (size_t i = 1; i < segs.size(); i++)
    {
        CHECK(segs[i].begin >= segs[i - 1].end);
    }

    // Canvas 1's two ranges group into one canvas frame, nothing drops.
    cmd::DeferredFrame frame;
    auto copy = [](Span<const uint8_t> s) {
        return std::vector<uint8_t>(s.data(), s.data() + s.size());
    };
    frame.commands = copy(session.commandBuffer().commandBytes());
    frame.blobs = copy(session.commandBuffer().blobBytes());
    frame.segments = session.schedulerSegments();

    TestSink sink;
    cmd::DeferredReplayer replayer;
    replayer.replayFrame(frame, sink);
    CHECK(replayer.droppedDraws() == 0);
}

TEST_CASE("Image:view on a decoded image records an imageView wrap",
          "[deferred][replay][image]")
{
    cmd::DeferredSession session(nullptr);
    auto view = session.oreContext().recordWrapImageView(42, 64, 64);
    REQUIRE(view != nullptr);

    // In imageView mode the canvasId field carries the 2D image resource id.
    const auto& stream = session.oreContext().stream();
    ore::cmd::OreCommandReader reader(stream.commandBytes(),
                                      stream.blobBytes());
    ore::cmd::CommandType type;
    REQUIRE(reader.next(type));
    REQUIRE(type == ore::cmd::CommandType::wrapCanvasView);
    auto pod = reader.read<ore::cmd::WrapCanvasViewPOD>();
    CHECK(pod.canvasId == 42u);
    CHECK(pod.mode ==
          static_cast<uint32_t>(ore::cmd::WrapCanvasViewMode::imageView));
}

TEST_CASE("a screen-gap destroy does not starve a reordered canvas segment",
          "[deferred][replay][segment]")
{
    cmd::DeferredFactory factory;
    auto& buffer = factory.commandBuffer();
    auto renderer = factory.makeRenderer();

    auto paint = factory.makeRenderPaint();
    auto p1 = factory.makeEmptyRenderPath();

    // The rcp release records a destroy.
    renderer->drawPath(p1.get(), paint.get());
    cmd::RenderHandle id = cmd::DeferredRenderPath::idOfPath(p1.get());
    REQUIRE(id != cmd::kInvalidRenderHandle);
    p1 = nullptr;
    buffer.drainDestroys();

    // This bracket is recorded after the destroy but replays before the
    // screen gap, so the destroy must stay behind its draws.
    uint32_t bracketBegin = static_cast<uint32_t>(buffer.commandBytes().size());
    constexpr cmd::RenderHandle kCanvas = 3 | cmd::kCanvasHandleFlag;
    buffer.append(static_cast<uint8_t>(cmd::RenderCmd::canvasContentBegin),
                  cmd::CanvasContentPOD{kCanvas, 0xFF000000});
    auto p2 = factory.makeEmptyRenderPath();
    renderer->drawPath(p2.get(), paint.get());
    buffer.append(static_cast<uint8_t>(cmd::RenderCmd::canvasContentEnd),
                  cmd::ResIdPOD{kCanvas});
    uint32_t bracketEnd = static_cast<uint32_t>(buffer.commandBytes().size());

    renderer->drawPath(p2.get(), paint.get());

    cmd::DeferredFrame frame;
    auto copy = [](Span<const uint8_t> s) {
        return std::vector<uint8_t>(s.data(), s.data() + s.size());
    };
    frame.commands = copy(buffer.commandBytes());
    frame.blobs = copy(buffer.blobBytes());
    frame.segments = {
        {Target::screen, 0, 0, bracketBegin},
        {Target::canvas, 3, bracketBegin, bracketEnd},
        {Target::screen,
         0,
         bracketEnd,
         static_cast<uint32_t>(frame.commands.size())},
    };

    TestSink sink;
    cmd::DeferredReplayer replayer;
    replayer.replayFrame(frame, sink);
    CHECK(replayer.droppedDraws() == 0);
}

TEST_CASE("a paint mutated after a draw keeps the draw's version",
          "[deferred][replay][version]")
{
    cmd::DeferredSession session(nullptr);
    auto* screen = session.screenRenderer();
    cmd::DeferredRenderer canvas(&session.commandBuffer(),
                                 &session.canvases(),
                                 &session,
                                 1);

    auto paint = session.makeRenderPaint();
    auto path = session.makeEmptyRenderPath();

    // Mutations replay in record order ahead of every draw, so the draws pin
    // the version they saw: red for the canvas draw, blue for the screen one.
    paint->color(0xFFFF0000);
    canvas.drawPath(path.get(), paint.get());
    paint->color(0xFF0000FF);
    screen->drawPath(path.get(), paint.get());
    session.closeOpenRange();

    cmd::DeferredFrame frame;
    auto copy = [](Span<const uint8_t> s) {
        return std::vector<uint8_t>(s.data(), s.data() + s.size());
    };
    frame.commands = copy(session.commandBuffer().commandBytes());
    frame.blobs = copy(session.commandBuffer().blobBytes());
    frame.segments = session.schedulerSegments();

    CountingSink sink;
    cmd::DeferredReplayer replayer;
    replayer.replayFrame(frame, sink);
    CHECK(replayer.droppedDraws() == 0);
    // The bump materialized the red version alongside the live blue paint.
    CHECK(sink.serializingFactory.paintCount == 2);
}

TEST_CASE("a paint mutated only before its draws stays one object",
          "[deferred][replay][version]")
{
    cmd::DeferredSession session(nullptr);
    auto* screen = session.screenRenderer();

    auto paint = session.makeRenderPaint();
    auto path = session.makeEmptyRenderPath();
    paint->color(0xFF00FF00);
    paint->thickness(2.0f);
    screen->drawPath(path.get(), paint.get());
    screen->drawPath(path.get(), paint.get());
    session.closeOpenRange();

    cmd::DeferredFrame frame;
    auto copy = [](Span<const uint8_t> s) {
        return std::vector<uint8_t>(s.data(), s.data() + s.size());
    };
    frame.commands = copy(session.commandBuffer().commandBytes());
    frame.blobs = copy(session.commandBuffer().blobBytes());
    frame.segments = session.schedulerSegments();

    CountingSink sink;
    cmd::DeferredReplayer replayer;
    replayer.replayFrame(frame, sink);
    CHECK(replayer.droppedDraws() == 0);
    CHECK(sink.serializingFactory.paintCount == 1);
}

TEST_CASE("the first mutation of a new frame reuses the live object",
          "[deferred][replay][version]")
{
    cmd::DeferredSession session(nullptr);
    auto* screen = session.screenRenderer();
    auto paint = session.makeRenderPaint();
    auto path = session.makeEmptyRenderPath();

    CountingSink sink;
    cmd::DeferredReplayer replayer;
    auto runFrame = [&](ColorInt color) {
        paint->color(color);
        screen->drawPath(path.get(), paint.get());
        auto frame = cmd::snapshotFrame(session);
        session.resetFrame();
        replayer.replayFrame(frame, sink);
        CHECK(replayer.droppedDraws() == 0);
    };
    // Animated content mutates every frame; the resident paint must be
    // reused in place, not reallocated per frame.
    runFrame(0xFFFF0000);
    runFrame(0xFF00FF00);
    runFrame(0xFF0000FF);
    CHECK(sink.serializingFactory.paintCount == 1);
}
