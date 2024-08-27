/*
 * Copyright 2024 Rive
 */

#pragma once

#include "rive/renderer/gpu.hpp"
#include "rive/renderer/vulkan/vulkan_context.hpp"

namespace rive::gpu
{
class VulkanFence;

// Manages a pool of recyclable VulkanFences.
class VulkanFencePool : public RefCnt<VulkanFencePool>
{
public:
    VulkanFencePool(rcp<VulkanContext> vk) : m_vk(std::move(vk)) {}

    rcp<VulkanFence> makeFence();

private:
    friend class VulkanFence;
    const rcp<VulkanContext> m_vk;
    std::vector<std::unique_ptr<VulkanFence>> m_fences;
};

// Wraps a VkFence created specifically for a PLS flush.
// Recycles the VulkanFence in its pool when its ref count reaches zero.
class VulkanFence : public CommandBufferCompletionFence
{
public:
    VulkanFence(rcp<VulkanFencePool> pool) : m_vk(pool->m_vk), m_pool(std::move(pool))
    {
        VkFenceCreateInfo fenceInfo = {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        };
        VK_CHECK(m_vk->CreateFence(m_vk->device, &fenceInfo, nullptr, &m_vkFence));
    }

    ~VulkanFence() override { m_vk->DestroyFence(m_vk->device, m_vkFence, nullptr); }

    operator VkFence() const { return m_vkFence; }

    void wait() override
    {
        m_vk->WaitForFences(m_vk->device, 1, &m_vkFence, VK_TRUE, VK_WHOLE_SIZE);
    }

private:
    // Recycles the fence in the VulkanFencePool.
    void onRefCntReachedZero() const override
    {
        constexpr static uint32_t kMaxFencesInPool = 32;

        if (m_pool->m_fences.size() < kMaxFencesInPool)
        {
            // Recycle the fence!
            m_vk->ResetFences(m_vk->device, 1, &m_vkFence);
            auto mutableThis = const_cast<VulkanFence*>(this);
            rcp<VulkanFencePool> pool = std::move(mutableThis->m_pool);
            assert(mutableThis->m_pool == nullptr);
            pool->m_fences.push_back(std::unique_ptr<VulkanFence>(mutableThis));
            assert(debugging_refcnt() == 0);
            return;
        }

        delete this;
    }

    friend class VulkanFencePool;
    const rcp<VulkanContext> m_vk;
    rcp<VulkanFencePool> m_pool;
    VkFence m_vkFence;
};

inline rcp<VulkanFence> VulkanFencePool::makeFence()
{
    rcp<VulkanFence> fence;
    if (!m_fences.empty())
    {
        fence = ref_rcp(m_fences.back().release());
        fence->m_pool = ref_rcp(this);
        m_fences.pop_back();
    }
    else
    {
        fence.reset(new VulkanFence(ref_rcp(this)));
    }
    assert(fence->debugging_refcnt() == 1);
    return fence;
}
} // namespace rive::gpu
