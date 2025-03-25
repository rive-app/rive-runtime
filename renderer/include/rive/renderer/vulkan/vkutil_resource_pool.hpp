/*
 * Copyright 2024 Rive
 */

#pragma once

#include "rive/renderer/vulkan/vulkan_context.hpp"

namespace rive::gpu::vkutil
{
// Used by ResourcePool<T> to construct resources.
template <typename T> class ResourceFactory
{
public:
    ResourceFactory(rcp<VulkanContext> vk) : m_vk(std::move(vk)) {}
    VulkanContext* vulkanContext() const { return m_vk.get(); }
    rcp<T> make() { return make_rcp<T>(m_vk); }

private:
    rcp<VulkanContext> m_vk;
};

// Manages a pool of recyclable Vulkan resources. When onRefCntReachedZero() is
// called on the resources owned by this pool, they are captured and recycled
// rather than deleted.
template <typename T, uint32_t MAX_RESOURCES_IN_POOL = 64>
class ResourcePool : public RefCnt<ResourcePool<T>>
{
public:
    template <typename... FactoryArgs>
    ResourcePool(FactoryArgs&&... factoryArgs) :
        m_factory(std::forward<FactoryArgs>(factoryArgs)...)
    {}

    ~ResourcePool()
    {
        m_releasedResources
            .clear(); // Delete resources before freeing the factory.
    }

    rcp<T> make()
    {
        rcp<T> resource;
        if (!m_releasedResources.empty() &&
            m_releasedResources.front().lastFrameNumber <=
                m_factory.vulkanContext()->safeFrameNumber())
        {
            resource = ref_rcp(m_releasedResources.front().resource.release());
            m_releasedResources.pop_front();
            resource->reset();
            purgeExcessExpiredResources();
        }
        else
        {
            resource = m_factory.make();
        }
        resource->m_pool = ref_rcp(static_cast<ResourcePool<T>*>(this));
        assert(resource->debugging_refcnt() == 1);
        return resource;
    }

    void onResourceRefCntReachedZero(const T* resource)
    {
        auto mutableResource = const_cast<T*>(resource);
        assert(mutableResource->debugging_refcnt() == 0);
        assert(mutableResource->m_pool.get() == this);

        // Recycle the resource!
        m_releasedResources.emplace_back(
            mutableResource,
            m_factory.vulkanContext()->currentFrameNumber());
        // Do this last in case it deletes our "this".
        mutableResource->m_pool = nullptr;

        purgeExcessExpiredResources();
    }

    void purgeExcessExpiredResources()
    {
        while (m_releasedResources.size() > MAX_RESOURCES_IN_POOL &&
               m_releasedResources.front().lastFrameNumber <=
                   m_factory.vulkanContext()->safeFrameNumber())
        {
            m_releasedResources.pop_front();
        }
    }

protected:
    ResourceFactory<T> m_factory;

    // Pool of Resources that have been fully released.
    // These can be recycled once their expirationFrameIdx is reached.
    std::deque<vkutil::ZombieResource<T>> m_releasedResources;
};
} // namespace rive::gpu::vkutil
