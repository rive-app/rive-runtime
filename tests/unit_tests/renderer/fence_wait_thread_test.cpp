/*
 * Copyright 2026 Rive
 *
 * Regression test for the Windows fence-wait teardown race (PR #13248).
 * FenceWaitThread is the handshake WindowsContextPLS uses between the render
 * thread and the background thread that waits on the frame fence and presents.
 * Before the fix, destroying the context while a frame was in flight either
 * ran the present callback into freed embedder objects (field crash: access
 * violation at 0xffffffffffffffff) or the wait thread overwrote the terminate
 * sentinel and join() hung the destructor forever. Against the old protocol,
 * "teardown drains an in-flight frame" deadlocks and fails. The invariants
 * pinned here: teardown drains the in-flight present before proceeding, the
 * callback never runs after the destructor returns, and the join always
 * completes.
 */

#include "rive_native/fence_wait_thread.hpp"
#include <catch.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

using namespace std::chrono_literals;

namespace
{
// Stands in for ID3D11Fence::SetEventOnCompletion; the test decides when the
// "GPU" finishes.
class FakeGpu
{
public:
    void waitForCompletion()
    {
        std::unique_lock<std::mutex> lock(m_mutex);
        while (!m_done)
        {
            m_cond.wait(lock);
        }
    }

    void completeFrame()
    {
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_done = true;
        }
        m_cond.notify_all();
    }

private:
    std::mutex m_mutex;
    std::condition_variable m_cond;
    bool m_done = false;
};

bool waitFor(const std::atomic<bool>& flag, std::chrono::milliseconds timeout)
{
    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!flag)
    {
        if (std::chrono::steady_clock::now() > deadline)
        {
            return false;
        }
        std::this_thread::sleep_for(1ms);
    }
    return true;
}
} // namespace

TEST_CASE("fence wait thread pumps frames and tears down idle",
          "[fence_wait_thread]")
{
    std::atomic<int> presents(0);
    {
        FenceWaitThread thread([&](uint64_t) { ++presents; });
        for (uint64_t frame = 1; frame <= 100; ++frame)
        {
            thread.waitForIdle();
            thread.kick(frame);
        }
        thread.waitForIdle();
        CHECK(presents == 100);
    }
    CHECK(presents == 100);
}

TEST_CASE("teardown drains an in-flight frame", "[fence_wait_thread]")
{
    FakeGpu gpu;
    std::atomic<int> presents(0);
    auto thread = std::make_unique<FenceWaitThread>([&](uint64_t) {
        gpu.waitForCompletion();
        ++presents;
    });

    thread->kick(1);
    std::this_thread::sleep_for(50ms);

    std::atomic<bool> destroyed(false);
    std::thread destroyer([&] {
        thread.reset();
        destroyed = true;
    });

    // With the GPU still busy, teardown must block rather than proceed while
    // the present callback can still fire.
    std::this_thread::sleep_for(100ms);
    CHECK(!destroyed);
    CHECK(presents == 0);

    gpu.completeFrame();

    // Old protocol: the wait thread clobbers the terminate sentinel and the
    // join hangs forever. Bound the wait so a regression is a test failure
    // rather than a hung CI job. Threads are settled before any assertion so
    // a failure cannot unwind past a joinable thread into std::terminate.
    bool drained = waitFor(destroyed, 10s);
    if (drained)
    {
        destroyer.join();
    }
    else
    {
        destroyer.detach();
    }
    int presentsAfterTeardown = presents;
    std::this_thread::sleep_for(50ms);
    int presentsSettled = presents;

    REQUIRE(drained);
    CHECK(presentsAfterTeardown == 1);

    // Nothing may present after the destructor has returned.
    CHECK(presentsSettled == presentsAfterTeardown);
}

TEST_CASE("teardown races frame completion", "[fence_wait_thread]")
{
    for (int i = 0; i < 300; ++i)
    {
        FakeGpu gpu;
        std::atomic<int> presents(0);
        auto thread = std::make_unique<FenceWaitThread>([&](uint64_t) {
            gpu.waitForCompletion();
            ++presents;
        });

        thread->kick(1);

        // Completion lands at a varying moment relative to the destructor to
        // sweep the race window.
        std::thread gpuThread([&, i] {
            std::this_thread::sleep_for(std::chrono::microseconds(i % 60));
            gpu.completeFrame();
        });

        std::atomic<bool> destroyed(false);
        std::thread destroyer([&] {
            thread.reset();
            destroyed = true;
        });

        bool drained = waitFor(destroyed, 10s);
        gpuThread.join();
        if (drained)
        {
            destroyer.join();
        }
        else
        {
            destroyer.detach();
        }

        REQUIRE(drained);
        REQUIRE(presents == 1);
    }
}
