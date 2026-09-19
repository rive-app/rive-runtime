/*
 * Copyright 2026 Rive
 */

// A host keeps a raw DeferredSession pointer across frames. Whichever side
// dies first, the survivor must never reach freed memory.

#if defined(RIVE_CANVAS) && defined(RIVE_ORE)

#include "rive/renderer/cmd/deferred_session.hpp"

#include <catch.hpp>
#include <memory>

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

#endif // RIVE_CANVAS && RIVE_ORE
