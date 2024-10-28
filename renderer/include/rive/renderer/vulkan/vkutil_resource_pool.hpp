/*
 * Copyright 2024 Rive
 */

#pragma once

#include "rive/renderer/gpu.hpp"
#include "rive/renderer/vulkan/vulkan_context.hpp"

namespace rive::gpu::vkutil
{
class CommandBuffer;
class Fence;
class Semaphore;

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

// ResourceFactory<> specialization for command buffers, which also require a
// VkCommandPool.
template <> class ResourceFactory<CommandBuffer>
{
public:
    ResourceFactory(rcp<VulkanContext> vk, uint32_t queueFamilyIndex);
    ~ResourceFactory();

    VulkanContext* vulkanContext() const { return m_vk.get(); }
    rcp<CommandBuffer> make()
    {
        return make_rcp<CommandBuffer>(m_vk, m_vkCommandPool);
    }
    operator VkCommandPool() const { return m_vkCommandPool; }

private:
    rcp<VulkanContext> m_vk;
    VkCommandPool m_vkCommandPool = VK_NULL_HANDLE;
};

// Manages a pool of recyclable Vulkan resources. When onRefCntReachedZero() is
// called on the resources owned by this pool, they are captured and recycled
// rather than deleted.
template <typename T, uint32_t MaxResourcesInPool = 64>
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
            m_factory.vulkanContext()->currentFrameIdx() >=
                m_releasedResources.front().expirationFrameIdx)
        {
            resource = ref_rcp(m_releasedResources.front().resource.release());
            m_releasedResources.pop_front();
            resource->reset();
            cleanExcessExpiredResources();
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
            m_factory.vulkanContext()->currentFrameIdx());
        // Do this last in case it deletes our "this".
        mutableResource->m_pool = nullptr;

        cleanExcessExpiredResources();
    }

    void cleanExcessExpiredResources()
    {
        while (m_releasedResources.size() > MaxResourcesInPool &&
               m_factory.vulkanContext()->currentFrameIdx() >=
                   m_releasedResources.front().expirationFrameIdx)
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

// VkCommandBuffer that lives in a ResourcePool.
class CommandBuffer : public RefCnt<CommandBuffer>
{
public:
    CommandBuffer(rcp<VulkanContext>, VkCommandPool);
    ~CommandBuffer();

    void reset();
    operator VkCommandBuffer() const { return m_vkCommandBuffer; }
    const VkCommandBuffer* vkCommandBufferAddressOf() const
    {
        return &m_vkCommandBuffer;
    }

private:
    friend class RefCnt<CommandBuffer>;
    friend class ResourcePool<CommandBuffer>;

    void onRefCntReachedZero() const
    {
        m_pool->onResourceRefCntReachedZero(this);
    }

    const rcp<VulkanContext> m_vk;
    const VkCommandPool m_vkCommandPool;
    rcp<ResourcePool<CommandBuffer>> m_pool;
    VkCommandBuffer m_vkCommandBuffer;
};

// VkSemaphore wrapper that lives in a ResourcePool.
class Semaphore : public RefCnt<Semaphore>
{
public:
    Semaphore(rcp<VulkanContext>);
    ~Semaphore();

    void reset();

    operator VkSemaphore() const { return m_vkSemaphore; }
    const VkSemaphore* vkSemaphoreAddressOf() const { return &m_vkSemaphore; }

private:
    friend class RefCnt<Semaphore>;
    friend class ResourcePool<Semaphore>;

    void onRefCntReachedZero() const
    {
        m_pool->onResourceRefCntReachedZero(this);
    }

    const rcp<VulkanContext> m_vk;
    rcp<ResourcePool<Semaphore>> m_pool;
    VkSemaphore m_vkSemaphore;
};

// VkFence wrapper that lives in a ResourcePool.
class Fence : public CommandBufferCompletionFence
{
public:
    Fence(rcp<VulkanContext>);
    ~Fence() override;

    void reset();
    void wait() override;

    operator VkFence() const { return m_vkFence; }

private:
    friend class ResourcePool<Fence>;

    void onRefCntReachedZero() const override
    {
        m_pool->onResourceRefCntReachedZero(this);
    }

    const rcp<VulkanContext> m_vk;
    rcp<ResourcePool<Fence>> m_pool;
    VkFence m_vkFence;
};
} // namespace rive::gpu::vkutil
