/*
 * Copyright 2026 Rive
 */

// Screen segments are the 2D stream regions outside canvas brackets, each
// naming the render target its draws belong to. Recording only, no GPU.

#include "rive/renderer/cmd/deferred_render_factory.hpp"
#include "rive/renderer/cmd/deferred_session.hpp"

#include <catch.hpp>

using namespace rive;
using rive::cmd::DeferredSegment;
using Target = rive::cmd::DeferredSegment::Target;

namespace
{
// A canvas recorder like the one beginCanvasContent hands a script.
std::unique_ptr<cmd::DeferredRenderer> canvasRecorder(
    cmd::DeferredSession& session,
    uint64_t canvasId)
{
    return std::make_unique<cmd::DeferredRenderer>(&session.commandBuffer(),
                                                   &session.canvases(),
                                                   &session,
                                                   canvasId);
}
} // namespace

TEST_CASE("a screen only frame is one screen segment", "[ore][cmd][segment]")
{
    cmd::DeferredSession session(rive::ore::ReplayCaps{});
    auto paint = session.makeRenderPaint();
    auto path = session.makeEmptyRenderPath();
    uint32_t afterCreates =
        static_cast<uint32_t>(session.commandBuffer().commandBytes().size());
    session.screenRenderer()->drawPath(path.get(), paint.get());
    session.closeOpenRange();

    auto all = session.schedulerSegments();
    REQUIRE(all.size() == 1);
    CHECK(all[0].target == Target::screen);
    CHECK(all[0].targetId == 0u);
    CHECK(all[0].begin == afterCreates);
    CHECK(all[0].end == session.commandBuffer().commandBytes().size());
}

TEST_CASE("bytes recorded before any target draws claim no segment",
          "[ore][cmd][segment]")
{
    cmd::DeferredSession session(rive::ore::ReplayCaps{});
    // Creates and drained destroys replay from the whole stream, so they need
    // no segment; giving them one would open a target's frame in a frame
    // where only other targets drew.
    auto paint = session.makeRenderPaint();
    auto path = session.makeEmptyRenderPath();
    CHECK(session.commandBuffer().commandBytes().size() > 0);
    session.closeOpenRange();
    CHECK(session.schedulerSegments().empty());
}

TEST_CASE("a canvas bracket carves leading and trailing screen segments",
          "[ore][cmd][segment]")
{
    cmd::DeferredSession session(rive::ore::ReplayCaps{});
    auto canvas = canvasRecorder(session, 1);
    auto paint = session.makeRenderPaint();
    auto path = session.makeEmptyRenderPath();
    uint32_t afterCreates =
        static_cast<uint32_t>(session.commandBuffer().commandBytes().size());

    session.screenRenderer()->drawPath(path.get(), paint.get());
    canvas->drawPath(path.get(), paint.get());
    session.screenRenderer()->drawPath(path.get(), paint.get());
    session.closeOpenRange();

    auto all = session.schedulerSegments();
    REQUIRE(all.size() == 3);
    CHECK(all[0].target == Target::screen);
    CHECK(all[0].begin == afterCreates);
    CHECK(all[1].target == Target::canvas);
    CHECK(all[1].targetId == 1u);
    CHECK(all[1].begin == all[0].end);
    CHECK(all[2].target == Target::screen);
    CHECK(all[2].begin == all[1].end);
    CHECK(all[2].end == session.commandBuffer().commandBytes().size());
}

TEST_CASE("a canvas bracket at offset 0 has no leading screen segment",
          "[ore][cmd][segment]")
{
    cmd::DeferredSession session(rive::ore::ReplayCaps{});
    auto canvas = canvasRecorder(session, 1);
    auto paint = session.makeRenderPaint();
    auto path = session.makeEmptyRenderPath();

    // Creates land in the stream before the first draw, so open the canvas
    // range from the very start by drawing into it first.
    session.closeOpenRange();
    uint32_t start =
        static_cast<uint32_t>(session.commandBuffer().commandBytes().size());
    canvas->drawPath(path.get(), paint.get());
    session.screenRenderer()->drawPath(path.get(), paint.get());
    session.closeOpenRange();

    std::vector<DeferredSegment> after;
    for (const auto& s : session.schedulerSegments())
    {
        if (s.begin >= start)
        {
            after.push_back(s);
        }
    }
    REQUIRE(after.size() == 2);
    CHECK(after[0].target == Target::canvas);
    CHECK(after[0].begin == start);
    CHECK(after[1].target == Target::screen);
    CHECK(after[1].begin == after[0].end);
}

TEST_CASE("each screen target gets its own segments", "[ore][cmd][segment]")
{
    cmd::DeferredSession session(rive::ore::ReplayCaps{});
    auto paint = session.makeRenderPaint();
    auto path = session.makeEmptyRenderPath();

    // Two widgets painting in one frame, interleaved as Flutter would.
    session.screenRenderer(0)->drawPath(path.get(), paint.get());
    session.screenRenderer(7)->drawPath(path.get(), paint.get());
    session.screenRenderer(0)->drawPath(path.get(), paint.get());
    session.closeOpenRange();

    auto all = session.schedulerSegments();
    REQUIRE(all.size() == 3);
    CHECK(all[0].targetId == 0u);
    CHECK(all[1].targetId == 7u);
    CHECK(all[2].targetId == 0u);
    for (const auto& s : all)
    {
        CHECK(s.target == Target::screen);
    }
    for (size_t i = 1; i < all.size(); i++)
    {
        CHECK(all[i].begin == all[i - 1].end);
    }
}

TEST_CASE("a canvas hands the stream back to the screen it interrupted",
          "[ore][cmd][segment]")
{
    cmd::DeferredSession session(rive::ore::ReplayCaps{});
    auto canvas = canvasRecorder(session, 1);
    auto paint = session.makeRenderPaint();
    auto path = session.makeEmptyRenderPath();

    session.screenRenderer(7)->drawPath(path.get(), paint.get());
    canvas->drawPath(path.get(), paint.get());
    session.closeOpenRange(); // closes the canvas range, as a snapshot would
    // Creates recorded after that belong to target 7, which was drawing, not
    // to the default screen, which drew nothing this frame.
    auto later = session.makeEmptyRenderPath();
    session.closeOpenRange();

    auto all = session.schedulerSegments();
    REQUIRE(all.size() == 3);
    CHECK(all[0].target == Target::screen);
    CHECK(all[0].targetId == 7u);
    CHECK(all[1].target == Target::canvas);
    CHECK(all[2].target == Target::screen);
    CHECK(all[2].targetId == 7u);
}

TEST_CASE("a session frame closes when the last target finishes",
          "[ore][cmd][segment]")
{
    cmd::DeferredSession session(rive::ore::ReplayCaps{});
    uint64_t a = session.acquireScreenTarget();
    uint64_t b = session.acquireScreenTarget();
    CHECK(a == 0u);
    CHECK(b == 1u);

    // Sequential painting: each target opens and closes its own window.
    session.beginTargetFrame(a);
    CHECK(session.endTargetFrame(a));
    session.beginTargetFrame(b);
    CHECK(session.endTargetFrame(b));

    // Nested painting: the inner finish must not end the session's frame,
    // resetting the stream under a target still recording.
    session.beginTargetFrame(a);
    session.beginTargetFrame(b);
    CHECK(!session.endTargetFrame(b));
    CHECK(session.endTargetFrame(a));

    // A released target's id and recorder are reclaimed.
    session.releaseScreenTarget(a);
    CHECK(session.acquireScreenTarget() == a);
}

TEST_CASE("the screen recorder for a target survives resetFrame",
          "[ore][cmd][segment]")
{
    cmd::DeferredSession session(rive::ore::ReplayCaps{});
    // FFI hosts take this raw and keep drawing through it across frames.
    Renderer* first = session.screenRenderer(3);
    session.resetFrame();
    CHECK(session.screenRenderer(3) == first);
}
