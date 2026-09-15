#pragma once

// Only include this file if we aren't building unit tests (the unit tests have
// a custom VulkanFrameSynchronizer type so that we can test without needing an
// actual vulkan-enabled build)
//
// Note that this is also why this file's implementation was done inline vs in
// a cpp file - for ease of unit testing
#ifndef RIVE_UNIT_TEST_FRAME_SYNC_COORDINATOR
#include "rive_vk_bootstrap/vulkan_frame_synchronizer.hpp"
#endif

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <deque>
#include <vector>

namespace rive_vkb
{

class VulkanFrameSyncCoordinator
{
public:
    // Add a newly-created frame synchronizer that we want to coordinate with
    // others.
    void addFrameSynchronizer(VulkanFrameSynchronizer* sync);

    // Remove a frame synchronizer that is about to be destroyed so it no longer
    // factors into the coordination.
    void removeFrameSynchronizer(VulkanFrameSynchronizer* sync);

    // Whenever a frame synchronizer begins a new frame, this should be called
    // afterwards to track the overall current and safe frames across all
    // synchronizers.
    void onFrameStart(VulkanFrameSynchronizer* synchronizer);

    uint64_t currentFrameNumber() const { return m_currentCoordinatedFrame; }
    uint64_t safeFrameNumber() const { return m_coordinatedSafeFrame; }

private:
    struct FramePair
    {
        uint64_t synchronizerFrame;
        uint64_t coordinatedFrame;
    };

    struct SynchronizerData
    {
        VulkanFrameSynchronizer* synchronizer;

        // using `deque` instead of `queue` because it has a clear() method.
        std::deque<FramePair> frames;
    };

    std::vector<SynchronizerData>::iterator findSyncData(
        VulkanFrameSynchronizer* s)
    {
        auto found = std::find_if(m_synchronizers.begin(),
                                  m_synchronizers.end(),
                                  [&](auto& d) { return d.synchronizer == s; });
        return found;
    }

    std::vector<SynchronizerData> m_synchronizers;
    uint64_t m_currentCoordinatedFrame = 0;
    uint64_t m_coordinatedSafeFrame = 0;
};

inline void VulkanFrameSyncCoordinator::addFrameSynchronizer(
    VulkanFrameSynchronizer* sync)
{
    assert(sync != nullptr);

#ifndef NDEBUG
    // The synchronizer being added should not already be in the list.
    auto&& found = findSyncData(sync);
    assert(found == m_synchronizers.end());
#endif

    m_synchronizers.push_back({.synchronizer = sync});
}

inline void VulkanFrameSyncCoordinator::removeFrameSynchronizer(
    VulkanFrameSynchronizer* sync)
{
    assert(sync != nullptr);

    auto&& found = findSyncData(sync);
    assert(found != m_synchronizers.end());

    m_synchronizers.erase(found);
}

inline void VulkanFrameSyncCoordinator::onFrameStart(
    VulkanFrameSynchronizer* synchronizer)
{
    auto synchronizerCurrentFrame = synchronizer->currentFrameNumber();
    auto synchronizerSafeFrame = synchronizer->safeFrameNumber();

    // First up is updating the current synchronizer.
    //  Note that this could be done as a map instead of a vector but in
    //  practice this is likely to be updated infrequently and small, so a
    //  linear search will likely be as fast or faster than a map lookup.
    auto&& found = findSyncData(synchronizer);
    assert(found != m_synchronizers.end());

    assert(found->frames.empty() ||
           found->frames.back().synchronizerFrame < synchronizerCurrentFrame);
    // We no longer need to track any frames older than the safe frame. Also
    // update the safe frame tracking to start at this synchronizer's safe
    // frame (we'll back it up to the earliest needed one later)
    if (found->frames.empty() &&
        synchronizerSafeFrame != synchronizerCurrentFrame)
    {
        // This is a synchronizer we don't already have tracking info for. Since
        // the logic of this class assumes we have a frame entry for every frame
        // from a synchronizer, add an entry for every frame from the
        // synchronizer's safe frame to just before its current frame, pinning
        // each of them to the frame before the one we're starting (so that they
        // all have to roll out of this synchronizer before we move the safe
        // frame up to point at what is now the newest frame)
        for (auto frame = synchronizerSafeFrame;
             frame < synchronizerCurrentFrame;
             frame++)
        {
            found->frames.push_back({
                .synchronizerFrame = frame,
                .coordinatedFrame = m_currentCoordinatedFrame,
            });
        }
    }
    else
    {
        while (!found->frames.empty() &&
               found->frames.front().synchronizerFrame < synchronizerSafeFrame)
        {
            found->frames.pop_front();
        }
    }

    m_currentCoordinatedFrame++;

    // Add our new frame to the list for tracking.
    found->frames.push_back({
        .synchronizerFrame = synchronizerCurrentFrame,
        .coordinatedFrame = m_currentCoordinatedFrame,
    });

    m_coordinatedSafeFrame = found->frames.front().coordinatedFrame;

    // Now check all the other synchronizers and find the earliest safe
    // frame
    for (auto& s : m_synchronizers)
    {
        if (s.frames.empty() ||
            s.frames.front().coordinatedFrame >= m_coordinatedSafeFrame)
        {
            // Either no frames have rendered yet or the safe frame for this
            // synchronizer is newer than the one we're already tracking as
            // the earliest. Either way, this synchronizer won't change the
            // minimum safe frame.
            continue;
        }

        if (s.synchronizer->checkMostRecentFrameCompletion())
        {
            // If the most-recent frame for this synchronizer is complete,
            // then all frames for it are safe and we can clear the
            // tracking as an optimization.
            s.frames.clear();
        }
        else
        {
            // The synchronizer hasn't run through its queued frames so
            // honor its safe frame.
            m_coordinatedSafeFrame = s.frames.front().coordinatedFrame;
        }
    }

    assert(m_currentCoordinatedFrame > m_coordinatedSafeFrame);
}

} // namespace rive_vkb
