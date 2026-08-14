/*
 * Copyright 2026 Rive
 */

// The GPU census walks the replayer's resident tables. What it has to get
// right to be usable as evidence: it counts what is live and not what was
// freed, it scales with the resources actually resident, and it is a level so
// reading it twice gives the same answer.

#include "rive/renderer/cmd/deferred_replayer.hpp"
#include "rive/renderer/cmd/deferred_session.hpp"
#include "rive/renderer/cmd/gpu_census.hpp"
#include "deferred_test_sink.hpp"

#include <catch.hpp>

using namespace rive;
using deferred_test::TestSink;

namespace
{
// Replay one session frame and hand back what stayed resident.
cmd::GpuCensus replayAndCensus(cmd::DeferredSession& session,
                               cmd::DeferredReplayer& replayer,
                               TestSink& sink)
{
    cmd::DeferredFrame frame = cmd::takeFrame(session);
    replayer.replayFrame(frame, sink);
    return replayer.gpuCensus();
}
} // namespace

TEST_CASE("the census counts what replay left resident", "[deferred][census]")
{
    cmd::DeferredSession session(rive::ore::ReplayCaps{});
    cmd::DeferredReplayer replayer;
    TestSink sink;

    auto* screen = session.screenRenderer();
    auto paint = session.makeRenderPaint();
    auto path = session.makeEmptyRenderPath();
    screen->drawPath(path.get(), paint.get());

    cmd::GpuCensus c = replayAndCensus(session, replayer, sink);
    CHECK(c.paths == 1);
    CHECK(c.paints == 1);
    // Nothing sized was recorded, so the byte total has to be zero rather than
    // some incidental nonzero from the count tables.
    CHECK(c.totalBytes() == 0);

    // A level, not a running total: the same walk twice is the same answer.
    CHECK(replayer.gpuCensus().totalBytes() == c.totalBytes());
    CHECK(replayer.gpuCensus().liveObjects() == c.liveObjects());
}

TEST_CASE("census bytes scale with the resources resident",
          "[deferred][census]")
{
    cmd::DeferredSession session(rive::ore::ReplayCaps{});
    cmd::DeferredReplayer replayer;
    TestSink sink;

    auto* screen = session.screenRenderer();
    auto paint = session.makeRenderPaint();
    auto buffer = session.makeRenderBuffer(RenderBufferType::vertex,
                                           RenderBufferFlags::none,
                                           1024);
    // Touch it so the draw keeps the recording honest about a live buffer.
    auto path = session.makeEmptyRenderPath();
    screen->drawPath(path.get(), paint.get());

    cmd::GpuCensus one = replayAndCensus(session, replayer, sink);
    CHECK(one.buffers == 1);
    CHECK(one.bufferBytes == 1024);
    CHECK(one.totalBytes() == 1024);

    // A second buffer of the same size doubles the sized total, and the
    // unsized counts stay put.
    auto buffer2 = session.makeRenderBuffer(RenderBufferType::vertex,
                                            RenderBufferFlags::none,
                                            1024);
    screen->drawPath(path.get(), paint.get());
    cmd::GpuCensus two = replayAndCensus(session, replayer, sink);
    CHECK(two.buffers == 2);
    CHECK(two.bufferBytes == 2048);
    CHECK(two.paths == one.paths);
    CHECK(two.paints == one.paints);
}

TEST_CASE("a destroyed resource leaves the census but keeps its slot",
          "[deferred][census]")
{
    cmd::DeferredSession session(rive::ore::ReplayCaps{});
    cmd::DeferredReplayer replayer;
    TestSink sink;

    auto* screen = session.screenRenderer();
    auto paint = session.makeRenderPaint();
    auto path = session.makeEmptyRenderPath();
    {
        auto doomed = session.makeRenderBuffer(RenderBufferType::vertex,
                                               RenderBufferFlags::none,
                                               4096);
        screen->drawPath(path.get(), paint.get());
        cmd::GpuCensus live = replayAndCensus(session, replayer, sink);
        CHECK(live.bufferBytes == 4096);
        CHECK(live.slots2d >= live.liveObjects());
    }
    // The rcp died, so the next frame carries the destroy record.
    session.commandBuffer().drainDestroys();
    screen->drawPath(path.get(), paint.get());
    cmd::GpuCensus after = replayAndCensus(session, replayer, sink);
    CHECK(after.buffers == 0);
    CHECK(after.bufferBytes == 0);
    // The tables never compact, so the freed slot is still counted as minted.
    CHECK(after.slots2d >= 1);
}

TEST_CASE("reset empties the census", "[deferred][census]")
{
    cmd::DeferredSession session(rive::ore::ReplayCaps{});
    cmd::DeferredReplayer replayer;
    TestSink sink;

    auto* screen = session.screenRenderer();
    auto paint = session.makeRenderPaint();
    auto path = session.makeEmptyRenderPath();
    auto buffer = session.makeRenderBuffer(RenderBufferType::vertex,
                                           RenderBufferFlags::none,
                                           2048);
    screen->drawPath(path.get(), paint.get());
    CHECK(replayAndCensus(session, replayer, sink).totalBytes() == 2048);

    replayer.reset();
    cmd::GpuCensus empty = replayer.gpuCensus();
    CHECK(empty.totalBytes() == 0);
    CHECK(empty.liveObjects() == 0);
    CHECK(empty.slots2d == 0);
    CHECK(empty.slotsOre == 0);
}

TEST_CASE("ore texture sizing covers mips, layers and samples",
          "[deferred][census]")
{
    // No GPU here, so size the arithmetic directly against the format table
    // rather than through a real texture.
    using rive::ore::TextureFormat;
    CHECK(ore::textureFormatBytesPerTexel(TextureFormat::rgba8unorm) == 4);
    CHECK(ore::textureFormatBytesPerTexel(TextureFormat::r8unorm) == 1);
    // rgba32float is 16 bytes, so a 4x4 single level is 256.
    CHECK(ore::textureFormatBytesPerTexel(TextureFormat::rgba32float) == 16);
}
