/*
 * Copyright 2026 Rive
 */

// Two lifetime guarantees on the session's resource maps: a recycled address
// resolves through whatever object holds it now, and a session's teardown may
// run off the recording thread.

#include "rive/renderer/ore/cmd/ore_deferred_context.hpp"
#include "rive/renderer/ore/cmd/ore_deferred_resource.hpp"

#include <catch.hpp>

#include <memory>
#include <thread>
#include <unordered_set>
#include <vector>

using namespace rive;
using namespace rive::ore;
using namespace rive::ore::cmd;

// Skip under AddressSanitizer: its quarantine holds freed blocks back, so the
// allocator never hands a dead resource's address to the next one and the
// aliasing this test exists to pin cannot be set up. The test REQUIREs the
// recycle it depends on, so it fails loudly rather than passing vacuously
// wherever the premise does not hold.
#ifndef __has_feature
#define __has_feature(x) 0
#endif
#if !defined(__SANITIZE_ADDRESS__) && !__has_feature(address_sanitizer)
TEST_CASE("a recycled address resolves through the object holding it now",
          "[ore_deferred_alias]")
{
    // A DeferredResource's destructor only queues its destroy, so the
    // allocator can hand a dead resource's address to a new one long before
    // anything that recorded that address has been cleaned up. One session's
    // dead resource leaves an address behind and the object that lands there
    // next belongs to a different session, so nothing the creator does can
    // reach the first session's memory of it. Only the object itself can
    // answer for the address, which is why the lookup asks the object.
    DeferredOreContext a(nullptr);
    ShaderModuleDesc smDesc{};

    // Dropping these queues destroys that nothing drains, so their ids and
    // their addresses are both loose while a still recorded them.
    constexpr int kCount = 32;
    std::unordered_set<const rive::gpu::GPUResource*> dead;
    {
        std::vector<rcp<ShaderModule>> sessionMods;
        for (int i = 0; i < kCount; ++i)
        {
            sessionMods.push_back(a.makeShaderModule(smDesc));
            dead.insert(sessionMods.back().get());
        }
    }

    // b creates the modules that reclaim those addresses.
    DeferredOreContext b(nullptr);
    ShaderModule* recycled = nullptr;
    std::vector<rcp<ShaderModule>> bMods;
    for (int i = 0; i < kCount && recycled == nullptr; ++i)
    {
        bMods.push_back(b.makeShaderModule(smDesc));
        ShaderModule* mod = bMods.back().get();
        recycled = dead.count(mod) != 0 ? mod : nullptr;
    }
    REQUIRE(recycled != nullptr);

    // b records it, so b names it by the id it created it under.
    ResourceHandle own =
        static_cast<DeferredShaderModule*>(recycled)->clientHandle();
    CHECK((own & kRealResourceFlag) == 0);
    CHECK(b.handleFor(recycled) == own);

    // a must not reuse that id: it indexes the table a's own stream feeds,
    // where the same number names something else. A deferred object that
    // records into a foreign stream takes the real resource path instead.
    ResourceHandle foreign = a.handleFor(recycled);
    CHECK(foreign != own);
    CHECK((foreign & kRealResourceFlag) != 0);

    // And a pipeline a builds over it carries that same real reference, so
    // replay resolves it from the retained side table rather than binding
    // whatever a holds at b's id.
    PipelineDesc pDesc{};
    pDesc.vertexModule = recycled;
    REQUIRE(a.makePipeline(pDesc) != nullptr);
    CHECK(a.handleFor(recycled) == foreign);
}
#endif

TEST_CASE("session teardown off the recording thread stays quiet",
          "[ore_deferred_alias]")
{
    // The recording thread assertion must not fire on the paths the deferred
    // design puts off thread on purpose. Dart finalizers release resources on
    // GC threads, and on threaded wasm riveDeleteDeferredSession posts the
    // delete to the replay worker, so a session's last destroy drain runs
    // there rather than on the thread that recorded it.
    auto d = std::make_unique<DeferredOreContext>(nullptr);
    BufferDesc bufDesc{};
    bufDesc.size = 16;
    auto live = d->makeBuffer(bufDesc);

    // A finalizer thread dropping the last reference while the session is
    // still recording: the destroy queues under the destroy mutex.
    {
        auto doomed = d->makeBuffer(bufDesc);
        std::thread finalizer([&] { doomed = nullptr; });
        finalizer.join();
    }

    // Teardown on a third thread, which drains that queue and records the
    // destroys into a stream it never appended to before.
    live = nullptr;
    std::thread worker([&] { d = nullptr; });
    worker.join();
    CHECK(d == nullptr);
}
