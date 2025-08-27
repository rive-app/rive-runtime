/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/refcnt.hpp"

#include <deque>

namespace rive::gpu
{
class GPUResourceManager;

// Base class for a GPU resource that needs to be kept alive until any in-flight
// command buffers that reference it have completed.
class GPUResource : public RefCnt<GPUResource>
{
public:
    virtual ~GPUResource();

    GPUResourceManager* manager() const { return m_manager.get(); }

protected:
    GPUResource(rcp<GPUResourceManager> manager) : m_manager(std::move(manager))
    {}

    const rcp<GPUResourceManager> m_manager;

private:
    friend class RefCnt<GPUResource>;

    // Don't delete GPUResources immediately when their ref count reaches zero;
    // instead wait until their safe frame number is reached.
    void onRefCntReachedZero() const;
};

// A GPUResource that has been fully released, but whose underlying native
// resource may still be referenced by an in-flight command buffer.
struct ZombieResource
{
    ZombieResource(GPUResource* resource_, uint64_t lastFrameNumber_) :
        resource(resource_), lastFrameNumber(lastFrameNumber_)
    {}

    std::unique_ptr<GPUResource> resource;
    const uint64_t lastFrameNumber; // Frame number at which the underlying
                                    // native resource was last used.
};

// Manages the lifecycle of GPUResources. When the refcount reaches zero, rather
// than deleting it immediately, this class places it in "purgatory" until its
// safe frame number is reached.
class GPUResourceManager : public RefCnt<GPUResourceManager>
{
public:
    virtual ~GPUResourceManager();

    // Resource lifetime counters. Resources last used on or before
    // 'safeFrameNumber' are safe to be released or recycled.
    uint64_t currentFrameNumber() const { return m_currentFrameNumber; }
    uint64_t safeFrameNumber() const { return m_safeFrameNumber; }

    // Purges released resources whose lastFrameNumber is on or before
    // safeFrameNumber, and updates the monotonically increasing
    // m_currentFrameNumber.
    void advanceFrameNumber(uint64_t nextFrameNumber, uint64_t safeFrameNumber);

    // Called when a GPUResource has been fully released (refcount reaches 0).
    // The underlying native resource won't actually be deleted until the
    // resource's safe frame number is reached.
    void onRenderingResourceReleased(GPUResource*);

    // Called prior to the client beginning its shutdown cycle, and after all
    // command buffers from all frames have finished executing. After shutting
    // down, we delete GPUResources immediately instead of going
    // through m_resourcePurgatory.
    void shutdown();

private:
    // An m_currentFrameNumber of this value indicates we're in a shutdown cycle
    // and resources should be deleted immediately upon release instead of going
    // through m_resourcePurgatory.
    constexpr static uint64_t SHUTDOWN_FRAME_NUMBER = 0xffffffffffffffff;

    // Resource lifetime counters. Resources last used on or before
    // 'safeFrameNumber' are safe to be released or recycled.
    uint64_t m_currentFrameNumber = 0;
    uint64_t m_safeFrameNumber = 0;

    // Temporary container for GPUResource instances that have been fully
    // released, but need to persist until their safe frame number is reached.
    std::deque<ZombieResource> m_resourcePurgatory;
};

// Manual GPUResource lifecycle manager. Rather than allowing GPUResourceManager
// to vaccuum up resources, they may be placed in a pool instead, which will
// recycle them once their safeFrameNumber is reached.
class GPUResourcePool : public GPUResource
{
public:
    GPUResourcePool(rcp<GPUResourceManager> manager, size_t maxPoolSize) :
        GPUResource(std::move(manager)), m_maxPoolCount(maxPoolSize)
    {}

    // Returns a previously-recycled resource whose safe frame number has been
    // reached, or null of no such resource exists.
    rcp<GPUResource> acquire();

    // Places the given resouce back in the pool, where it waits until its safe
    // frame number is reached. Refcount must be 1.
    void recycle(rcp<GPUResource>);

private:
    const size_t m_maxPoolCount;
    std::deque<ZombieResource> m_pool;
};
} // namespace rive::gpu
