/*
 * Copyright 2026 Rive
 */

// Destroys enter the stream only at a frame boundary, so a session nothing
// draws any more strands them and the resident tables keep the resources.
// takePendingDestroys + replayDestroys is the way out that needs no frame.

#include "rive/renderer/cmd/deferred_replayer.hpp"
#include "rive/renderer/cmd/deferred_session.hpp"
#include "rive/renderer/cmd/gpu_census.hpp"
#include "rive/renderer/ore/cmd/ore_command_buffer.hpp"
#include "rive/renderer/ore/cmd/ore_commands.hpp"
#include "deferred_test_sink.hpp"

#include <catch.hpp>

using namespace rive;
using deferred_test::TestSink;

namespace
{
void replayFrame(cmd::DeferredSession& session,
                 cmd::DeferredReplayer& replayer,
                 TestSink& sink)
{
    cmd::DeferredFrame frame = cmd::takeFrame(session);
    replayer.replayFrame(frame, sink);
}
} // namespace

TEST_CASE("an idle session strands destroys until they are flushed",
          "[deferred][destroys]")
{
    cmd::DeferredSession session(rive::ore::ReplayCaps{});
    cmd::DeferredReplayer replayer;
    TestSink sink;

    auto paint = session.makeRenderPaint();
    auto path = session.makeEmptyRenderPath();
    session.screenRenderer()->drawPath(path.get(), paint.get());
    replayFrame(session, replayer, sink);
    REQUIRE(replayer.gpuCensus().paths == 1);
    REQUIRE(replayer.gpuCensus().paints == 1);

    // The handles die with no frame to follow, so nothing reaches replay.
    paint = nullptr;
    path = nullptr;
    CHECK(replayer.gpuCensus().paths == 1);
    CHECK(replayer.gpuCensus().paints == 1);

    auto pending = session.takePendingDestroys();
    CHECK_FALSE(pending.empty());
    replayer.replayDestroys(pending.commands, pending.oreCommands);
    CHECK(replayer.gpuCensus().paths == 0);
    CHECK(replayer.gpuCensus().paints == 0);
}

TEST_CASE("nothing queued takes nothing", "[deferred][destroys]")
{
    cmd::DeferredSession session(rive::ore::ReplayCaps{});
    CHECK(session.takePendingDestroys().empty());
}

TEST_CASE("an ore handle's destroy is taken and replays outside a frame",
          "[deferred][destroys]")
{
    cmd::DeferredSession session(rive::ore::ReplayCaps{});
    cmd::DeferredReplayer replayer;

    ore::BufferDesc desc{};
    desc.usage = ore::BufferUsage::uniform;
    desc.size = 64;
    auto buffer = session.oreContext().makeBuffer(desc);
    REQUIRE(buffer != nullptr);
    // The create is still in the stream; only the tail may be taken.
    REQUIRE(session.takePendingDestroys().empty());

    buffer = nullptr;
    auto pending = session.takePendingDestroys();
    CHECK(pending.commands.empty());
    REQUIRE_FALSE(pending.oreCommands.empty());

    // Exactly one destroy, and nothing else rode along.
    ore::cmd::OreCommandReader reader(
        Span<const uint8_t>(pending.oreCommands.data(),
                            pending.oreCommands.size()),
        Span<const uint8_t>());
    ore::cmd::CommandType type;
    REQUIRE(reader.next(type));
    CHECK(type == ore::cmd::CommandType::destroyResource);
    reader.read<ore::cmd::DestroyResourcePOD>();
    CHECK_FALSE(reader.next(type));

    // No GPU here, so the buffer was never resident. The replay still has to
    // consume the stream cleanly, and a second pass must be a no-op too.
    replayer.replayDestroys(pending.commands, pending.oreCommands);
    replayer.replayDestroys(pending.commands, pending.oreCommands);
    CHECK(replayer.gpuCensus().liveObjects() == 0);
}

TEST_CASE("flushed destroys replay again with the next frame harmlessly",
          "[deferred][destroys]")
{
    cmd::DeferredSession session(rive::ore::ReplayCaps{});
    cmd::DeferredReplayer replayer;
    TestSink sink;

    auto paint = session.makeRenderPaint();
    auto path = session.makeEmptyRenderPath();
    session.screenRenderer()->drawPath(path.get(), paint.get());
    replayFrame(session, replayer, sink);

    paint = nullptr;
    path = nullptr;
    auto pending = session.takePendingDestroys();
    replayer.replayDestroys(pending.commands, pending.oreCommands);
    REQUIRE(replayer.gpuCensus().liveObjects() == 0);

    // The flushed bytes stay in the stream, and the ids they released are
    // free to be retaken. The retake must survive the second replay of those
    // destroys: they carry the old generation.
    auto paint2 = session.makeRenderPaint();
    auto path2 = session.makeEmptyRenderPath();
    session.screenRenderer()->drawPath(path2.get(), paint2.get());
    replayFrame(session, replayer, sink);
    CHECK(replayer.gpuCensus().paths == 1);
    CHECK(replayer.gpuCensus().paints == 1);
}
