/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/gpu_resource.hpp"

namespace rive::gpu
{
GPUResource::~GPUResource() {}

void GPUResource::onRefCntReachedZero() const
{
    // GPUResourceManager will hold off on deleting "this" until its safe frame
    // number has been reached.
    m_manager->onRenderingResourceReleased(const_cast<GPUResource*>(this));
}

GPUResourceManager::~GPUResourceManager()
{
    // Call shutdown() before destroying the resource manager.
    assert(m_currentFrameNumber == SHUTDOWN_FRAME_NUMBER);
    assert(m_safeFrameNumber == SHUTDOWN_FRAME_NUMBER);
    assert(m_resourcePurgatory.empty());
}

void GPUResourceManager::advanceFrameNumber(uint64_t nextFrameNumber,
                                            uint64_t safeFrameNumber)
{
    assert(nextFrameNumber >= m_currentFrameNumber);
    assert(safeFrameNumber >= m_safeFrameNumber);
    assert(safeFrameNumber <= nextFrameNumber);

    m_currentFrameNumber = nextFrameNumber;
    m_safeFrameNumber = safeFrameNumber;

    // Delete all resources whose safe frame number has been reached.
    while (!m_resourcePurgatory.empty() &&
           m_resourcePurgatory.front().lastFrameNumber <= m_safeFrameNumber)
    {
        assert(m_resourcePurgatory.front().resource->debugging_refcnt() == 0);
        m_resourcePurgatory.pop_front();
    }
}

void GPUResourceManager::onRenderingResourceReleased(GPUResource* resource)
{
    assert(resource->manager() == this);
    if (m_currentFrameNumber > m_safeFrameNumber)
    {
        // Hold this resource until its safe frame number is reached.
        assert(resource->debugging_refcnt() == 0);
        assert(m_resourcePurgatory.empty() ||
               m_currentFrameNumber >=
                   m_resourcePurgatory.back().lastFrameNumber);
        m_resourcePurgatory.emplace_back(resource, m_currentFrameNumber);
    }
    else
    {
        // We're in a shutdown cycle. Delete immediately.
        delete resource;
    }
}

void GPUResourceManager::shutdown()
{
    advanceFrameNumber(SHUTDOWN_FRAME_NUMBER, SHUTDOWN_FRAME_NUMBER);
}

rcp<GPUResource> GPUResourcePool::acquire()
{
    rcp<GPUResource> resource;
    if (!m_pool.empty() &&
        m_pool.front().lastFrameNumber <= m_manager->safeFrameNumber())
    {
        // Recycle the oldest buffer in the pool.
        resource = rcp(m_pool.front().resource.release());
        m_pool.pop_front();

        // Trim the pool in case it's grown out of control (meaning it was
        // advanced multiple times in a single frame).
        while (m_pool.size() > m_maxPoolCount &&
               m_pool.front().lastFrameNumber <= m_manager->safeFrameNumber())
        {
            m_pool.pop_front();
        }
    }
    assert(resource == nullptr || resource->debugging_refcnt() == 1);
    return resource;
}

void GPUResourcePool::recycle(rcp<GPUResource> resource)
{
    if (resource != nullptr)
    {
        // Return the current buffer to the pool.
        assert(resource->debugging_refcnt() == 1);
        assert(m_pool.empty() || m_manager->currentFrameNumber() >=
                                     m_pool.back().lastFrameNumber);
        m_pool.emplace_back(resource.release(),
                            m_manager->currentFrameNumber());
    }
}
} // namespace rive::gpu
