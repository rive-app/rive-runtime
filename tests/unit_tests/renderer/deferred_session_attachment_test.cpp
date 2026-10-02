/*
 * Copyright 2026 Rive
 */

// A host keeps a raw DeferredSession pointer across frames. Whichever side
// dies first, the survivor must never reach freed memory.

#if defined(RIVE_CANVAS) && defined(RIVE_ORE)

#include "deferred_test_sink.hpp"
#include "rive/renderer/cmd/deferred_replayer.hpp"
#include "rive/renderer/cmd/deferred_session.hpp"

#include <catch.hpp>
#include <memory>
#include <vector>

using namespace rive;

namespace
{
// The rive_native texture host in miniature: a raw pointer and a target.
class TestHost : public cmd::DeferredSessionAttachment
{
public:
    ~TestHost() override
    {
        if (session != nullptr)
        {
            session->detach(this);
            session->releaseScreenTarget(target);
        }
    }

    void join(cmd::DeferredSession* s)
    {
        session = s;
        target = s->acquireScreenTarget();
        s->attach(this);
    }

    void deferredSessionDestroyed() override
    {
        session = nullptr;
        target = 0;
        notices++;
    }

    Renderer* screenRenderer() const
    {
        return session == nullptr ? nullptr : session->screenRenderer(target);
    }

    cmd::DeferredSession* session = nullptr;
    uint64_t target = 0;
    int notices = 0;
};
} // namespace

TEST_CASE("a dying session clears every attached host", "[cmd][attachment]")
{
    auto session = std::make_unique<cmd::DeferredSession>(ore::ReplayCaps{});
    TestHost a, b;
    a.join(session.get());
    b.join(session.get());
    CHECK(session->attachmentCount() == 2);
    CHECK(session->attachedTargetCount() == 2);
    CHECK(a.target != b.target);
    CHECK(a.screenRenderer() != nullptr);
    CHECK(b.screenRenderer() != nullptr);

    session.reset();
    CHECK(a.session == nullptr);
    CHECK(b.session == nullptr);
    CHECK(a.notices == 1);
    CHECK(b.notices == 1);
    CHECK(a.screenRenderer() == nullptr);
    CHECK(b.screenRenderer() == nullptr);
}

TEST_CASE("a host that dies first leaves the session nothing to notify",
          "[cmd][attachment]")
{
    cmd::DeferredSession session(ore::ReplayCaps{});
    {
        TestHost a;
        a.join(&session);
        CHECK(session.attachmentCount() == 1);
        CHECK(session.attachedTargetCount() == 1);
    }
    CHECK(session.attachmentCount() == 0);
    CHECK(session.attachedTargetCount() == 0);
}

TEST_CASE("attach is idempotent and a stranger's detach is a no-op",
          "[cmd][attachment]")
{
    cmd::DeferredSession session(ore::ReplayCaps{});
    TestHost a, stranger;
    a.join(&session);
    session.attach(&a);
    CHECK(session.attachmentCount() == 1);
    session.detach(&stranger);
    CHECK(session.attachmentCount() == 1);
}

TEST_CASE("a host outliving its session attaches a successor cleanly",
          "[cmd][attachment]")
{
    TestHost a;
    auto first = std::make_unique<cmd::DeferredSession>(ore::ReplayCaps{});
    a.join(first.get());
    first.reset();
    CHECK(a.session == nullptr);

    // Whether or not the allocator hands back the same address, the host
    // starts from nothing.
    auto second = std::make_unique<cmd::DeferredSession>(ore::ReplayCaps{});
    a.join(second.get());
    CHECK(a.session == second.get());
    CHECK(second->attachmentCount() == 1);
    CHECK(second->attachedTargetCount() == 1);
    CHECK(a.screenRenderer() != nullptr);
}

TEST_CASE("abandoning every open frame lets the next target close the window",
          "[cmd][attachment]")
{
    cmd::DeferredSession session(ore::ReplayCaps{});
    session.beginTargetFrame(0);
    session.beginTargetFrame(1);
    CHECK(session.openTargetCount() == 2);

    session.abandonOpenTargetFrames();
    CHECK(session.openTargetCount() == 0);

    session.beginTargetFrame(2);
    CHECK(session.endTargetFrame(2));
}

namespace
{
// Records which screen targets a replay opened.
class TargetSink : public deferred_test::TestSink
{
public:
    Renderer* beginScreenFrame(uint64_t target) override
    {
        opened.push_back(target);
        return deferred_test::TestSink::beginScreenFrame(target);
    }
    std::vector<uint64_t> opened;
};
} // namespace

TEST_CASE("an abandoned frame's draws never reach the next target's frame",
          "[cmd][attachment]")
{
    cmd::DeferredSession session(ore::ReplayCaps{});
    uint64_t a = session.acquireScreenTarget();
    uint64_t b = session.acquireScreenTarget();
    auto paint = session.makeRenderPaint();
    auto path = session.makeEmptyRenderPath();

    // A draws, then goes away before finishing its frame.
    session.beginTargetFrame(a);
    session.screenRenderer(a)->drawPath(path.get(), paint.get());
    SECTION("abandoned by target") { session.abandonTargetFrame(a); }
    SECTION("abandoned with every open frame")
    {
        session.abandonOpenTargetFrames();
    }
    session.releaseScreenTarget(a);

    session.beginTargetFrame(b);
    session.screenRenderer(b)->drawPath(path.get(), paint.get());
    REQUIRE(session.endTargetFrame(b));
    session.closeOpenRange();

    cmd::DeferredFrame frame = cmd::snapshotFrame(session);
    TargetSink sink;
    cmd::DeferredReplayer replayer;
    replayer.replayFrame(frame, sink);
    CHECK(sink.opened == std::vector<uint64_t>{b});
}

TEST_CASE("a leaving target's finished frame never replays under its id",
          "[cmd][attachment]")
{
    cmd::DeferredSession session(ore::ReplayCaps{});
    uint64_t a = session.acquireScreenTarget();
    uint64_t b = session.acquireScreenTarget();
    auto paint = session.makeRenderPaint();
    auto path = session.makeEmptyRenderPath();

    // A finishes while B still holds the window open, then leaves.
    session.beginTargetFrame(a);
    session.beginTargetFrame(b);
    session.screenRenderer(a)->drawPath(path.get(), paint.get());
    REQUIRE_FALSE(session.endTargetFrame(a));
    session.discardTargetFrame(a);
    session.releaseScreenTarget(a);
    // Its id goes to the next host to join.
    REQUIRE(session.acquireScreenTarget() == a);

    session.screenRenderer(b)->drawPath(path.get(), paint.get());
    REQUIRE(session.endTargetFrame(b));
    session.closeOpenRange();

    cmd::DeferredFrame frame = cmd::snapshotFrame(session);
    TargetSink sink;
    cmd::DeferredReplayer replayer;
    replayer.replayFrame(frame, sink);
    CHECK(sink.opened == std::vector<uint64_t>{b});
}

#endif // RIVE_CANVAS && RIVE_ORE
