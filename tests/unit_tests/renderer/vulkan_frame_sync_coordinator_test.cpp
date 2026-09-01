#include <catch.hpp>

// Before including the frame sync coordinator header, declare a unit test
// synchronizer class and the RIVE_UNIT_TEST_FRAME_SYNC_COORDINATOR, which
// allows us to use this mock version instead of the real one.

namespace rive::tests
{
class VulkanFrameSynchronizer
{
public:
    // Implement the minimum surface area that the coordinator cares about.
    bool checkMostRecentFrameCompletion() { return m_isMostRecentFrameDone; }
    uint64_t currentFrameNumber() const { return m_currentFrameNumber; }
    uint64_t safeFrameNumber() const { return m_safeFrameNumber; }

    // Testing functions (not standard VulkanFrameSynchronizer functions)
    void tickFrameAndSafe()
    {
        m_isMostRecentFrameDone = false;
        m_currentFrameNumber++;
        m_safeFrameNumber++;
    }

    bool m_isMostRecentFrameDone = false;
    uint64_t m_currentFrameNumber = 0;
    uint64_t m_safeFrameNumber = 0;
};
} // namespace rive::tests

using namespace rive::tests;

#define RIVE_UNIT_TEST_FRAME_SYNC_COORDINATOR
#include "rive_vk_bootstrap/vulkan_frame_sync_coordinator.hpp"

using namespace rive_vkb;

TEST_CASE("Single Frame Sync", "[vulkan_frame_sync_coordinator]")
{
    VulkanFrameSynchronizer sync;
    VulkanFrameSyncCoordinator coordinator;

    sync.m_currentFrameNumber = 2;

    coordinator.addFrameSynchronizer(&sync);

    sync.tickFrameAndSafe();
    coordinator.onFrameStart(&sync);
    CHECK(coordinator.currentFrameNumber() == 1);

    // On a new frame synchronizer the safe frame should be no later than the
    // frame before the current frame (otherwise the renderer will clean up the
    // assets used during the current frame which is decidedly not what we want)
    CHECK(coordinator.safeFrameNumber() == 0);

    sync.tickFrameAndSafe();
    coordinator.onFrameStart(&sync);
    CHECK(coordinator.currentFrameNumber() == 2);
    CHECK(coordinator.safeFrameNumber() == 0);

    // The next frame is when the safe frame should finally move forward, as
    // we've now incremented up past the first frame we submitted to the
    // coordinator.
    for (auto f = 3u; f < 20u; f++)
    {
        sync.tickFrameAndSafe();
        coordinator.onFrameStart(&sync);
        CHECK(coordinator.currentFrameNumber() == f);
        CHECK(coordinator.safeFrameNumber() == f - 2);
    }
}

TEST_CASE("Replaced Frame Sync", "[vulkan_frame_sync_coordinator]")
{
    VulkanFrameSynchronizer syncA;
    syncA.m_currentFrameNumber = 1;
    VulkanFrameSyncCoordinator coordinator;

    coordinator.addFrameSynchronizer(&syncA);

    syncA.tickFrameAndSafe();
    coordinator.onFrameStart(&syncA);
    CHECK(coordinator.currentFrameNumber() == 1);
    CHECK(coordinator.safeFrameNumber() == 0);

    syncA.tickFrameAndSafe();
    coordinator.onFrameStart(&syncA);
    CHECK(coordinator.currentFrameNumber() == 2);
    CHECK(coordinator.safeFrameNumber() == 1);

    VulkanFrameSynchronizer syncB;
    // Pick arbitrarily different frame numbers
    syncB.m_currentFrameNumber = 1000;
    syncB.m_safeFrameNumber = 999;

    coordinator.removeFrameSynchronizer(&syncA);
    coordinator.addFrameSynchronizer(&syncB);

    coordinator.onFrameStart(&syncB);
    // After the replacement, the new current frame should increase (separate
    // from the value from the sync) and, as stated above, the safe frame should
    // end up as the frame *before* the current frame
    CHECK(coordinator.currentFrameNumber() == 3);
    CHECK(coordinator.safeFrameNumber() == 2);

    syncB.tickFrameAndSafe();
    coordinator.onFrameStart(&syncB);
    CHECK(coordinator.currentFrameNumber() == 4);
    CHECK(coordinator.safeFrameNumber() == 3);

    syncB.tickFrameAndSafe();
    coordinator.onFrameStart(&syncB);
    CHECK(coordinator.currentFrameNumber() == 5);
    CHECK(coordinator.safeFrameNumber() == 4);
}

TEST_CASE("Two Frame Syncs", "[vulkan_frame_sync_coordinator]")
{
    VulkanFrameSynchronizer syncA;
    VulkanFrameSynchronizer syncB;

    VulkanFrameSyncCoordinator coordinator;

    coordinator.addFrameSynchronizer(&syncA);
    coordinator.addFrameSynchronizer(&syncB);

    syncA.m_currentFrameNumber = 1000;
    syncA.m_safeFrameNumber = syncA.m_currentFrameNumber - 2;

    syncB.m_currentFrameNumber = 500000;
    syncB.m_safeFrameNumber = syncB.m_currentFrameNumber - 1;

    // coordinator frame 1 is syncA frame 1001 (with its safe frame of 999)
    syncA.tickFrameAndSafe();
    coordinator.onFrameStart(&syncA);

    CHECK(coordinator.currentFrameNumber() == 1);
    CHECK(coordinator.safeFrameNumber() == 0);

    // Tick syncB a few times - note that the safe frame won't update for this
    // because it's being held back by sync A.
    // This gets the coordinator's frame up to 5.
    for (auto f = 2u; f <= 5u; f++)
    {
        syncB.tickFrameAndSafe();
        coordinator.onFrameStart(&syncB);
        CHECK(coordinator.currentFrameNumber() == f);
        CHECK(coordinator.safeFrameNumber() == 0);
    }

    // syncA's next frame is 6, but internally it's 1002 (and safe is now 1000,
    // which is still before coordinated frame 1), then after that is 7 which is
    // internally 1003/1001, which finally moves the safe frame up by 1.
    // The safe frame counter is tested in the loop as f - 6 (which is the
    // above-mentioned 0 then 1)
    for (auto f = 6u; f <= 7u; f++)
    {
        syncA.tickFrameAndSafe();
        coordinator.onFrameStart(&syncA);
        CHECK(coordinator.currentFrameNumber() == f);
        CHECK(coordinator.safeFrameNumber() == (f - 6));
    }

    // Ticking syncA forward again now moves its safe frame past syncB, so now
    // syncB's sync frame takes over
    syncA.tickFrameAndSafe();
    coordinator.onFrameStart(&syncA);
    CHECK(coordinator.currentFrameNumber() == 8);
    CHECK(coordinator.safeFrameNumber() == 4);

    // Now we'll simulate the scenario where syncB has fallen so far behind that
    // it has actually rendered all the way through its most recent frame, which
    // should spring the safe frame forward to be within 2 of current (as syncA
    // is now the only deciding factor)
    syncB.m_isMostRecentFrameDone = true;
    syncA.tickFrameAndSafe();
    coordinator.onFrameStart(&syncA);
    CHECK(coordinator.currentFrameNumber() == 9);
    CHECK(coordinator.safeFrameNumber() == 7);

    // Now if syncB ticks again it will start to matter again (but syncA will
    // hold the safe frame back)
    for (auto f = 10u; f <= 15u; f++)
    {
        syncB.tickFrameAndSafe();
        coordinator.onFrameStart(&syncB);
        CHECK(coordinator.currentFrameNumber() == f);
        CHECK(coordinator.safeFrameNumber() == 7);
    }

    // ticking Sync A forward twice will move the safe frame twice as well (as
    // it's still pinned by A's previous ticks)
    syncA.tickFrameAndSafe();
    coordinator.onFrameStart(&syncA);
    CHECK(coordinator.currentFrameNumber() == 16);
    CHECK(coordinator.safeFrameNumber() == 8);

    syncA.tickFrameAndSafe();
    coordinator.onFrameStart(&syncA);
    CHECK(coordinator.currentFrameNumber() == 17);
    CHECK(coordinator.safeFrameNumber() == 9);

    // ticking Sync A a third time will now move it past sync B's safe frame,
    // which will take over as the new current safe frame.
    syncA.tickFrameAndSafe();
    coordinator.onFrameStart(&syncA);
    CHECK(coordinator.currentFrameNumber() == 18);
    CHECK(coordinator.safeFrameNumber() == 14);

    // Now if we remove sync B, the coordinator's safe frame will only rely on A
    // again
    coordinator.removeFrameSynchronizer(&syncB);
    syncA.tickFrameAndSafe();
    coordinator.onFrameStart(&syncA);
    CHECK(coordinator.currentFrameNumber() == 19);
    CHECK(coordinator.safeFrameNumber() == 17);

    // Adding sync B but not ticking it means that it won't affect anything
    //  (note that in practice we're not expecting synchronizers to get removed
    //  and re-added - in the real runtime only *new* synchronizers should be
    //  getting added - but for testing, this is fine)
    coordinator.addFrameSynchronizer(&syncB);
    for (auto f = 20u; f <= 25u; f++)
    {
        syncA.tickFrameAndSafe();
        coordinator.onFrameStart(&syncA);
        CHECK(coordinator.currentFrameNumber() == f);
        CHECK(coordinator.safeFrameNumber() == f - 2);
    }

    // Now if we tick syncB and remove syncA, its next frame will be the
    // baseline (but the safe frame should be the frame before the new current
    // frame)
    coordinator.removeFrameSynchronizer(&syncA);
    syncB.tickFrameAndSafe();
    coordinator.onFrameStart(&syncB);
    CHECK(coordinator.currentFrameNumber() == 26);
    CHECK(coordinator.safeFrameNumber() == 25);
}

TEST_CASE("Resumed synchronizer", "[vulkan_frame_sync_coordinator]")
{
    VulkanFrameSynchronizer syncA;
    VulkanFrameSyncCoordinator coordinator;
    coordinator.addFrameSynchronizer(&syncA);

    // syncA's frame 2 is coordinator frame 1 (safe frame is still 0)
    syncA.m_currentFrameNumber = 2;
    coordinator.onFrameStart(&syncA);

    // Mark it as fully completed (emulating its frames have fully run through)
    syncA.m_isMostRecentFrameDone = true;

    // Temporarily add a second synchronizer and tick it once to let the
    // coordinator catch that syncA is fully completed. This will increase the
    // coordinator frame to 2 (and its safe frame to 1).
    {
        VulkanFrameSynchronizer syncB;
        coordinator.addFrameSynchronizer(&syncB);
        syncB.m_currentFrameNumber = 1;
        coordinator.onFrameStart(&syncB);
        CHECK(coordinator.safeFrameNumber() == 1);
        coordinator.removeFrameSynchronizer(&syncB);
    }

    // Now A ticks again, which should move the safe frame up to 2 (the frame
    // before the current frame number of 3)
    syncA.tickFrameAndSafe();
    coordinator.onFrameStart(&syncA);
    CHECK(coordinator.safeFrameNumber() == 2);

    // Run another frame: the safe frame should still be behind the first
    // resumed frame (as syncA's safe frame has not caught up to that frame yet)
    syncA.tickFrameAndSafe();
    coordinator.onFrameStart(&syncA);
    CHECK(coordinator.safeFrameNumber() == 2);

    // Finally ensure that the next frame *does* tick the safe frame forward (as
    // we have now gotten syncA's safe frame past the first resumed frame)
    syncA.tickFrameAndSafe();
    coordinator.onFrameStart(&syncA);
    CHECK(coordinator.safeFrameNumber() == 3);
}