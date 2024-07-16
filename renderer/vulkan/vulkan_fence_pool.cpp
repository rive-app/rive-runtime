/*
 * Copyright 2024 Rive
 */

#include "rive/pls/vulkan/vulkan_fence_pool.hpp"

#include "rive/pls/vulkan/vkutil.hpp"

namespace rive::pls
{
VulkanFence::VulkanFence(rcp<VulkanFencePool> pool) :
    m_device(pool->m_device), m_pool(std::move(pool))
{
    VkFenceCreateInfo fenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    };
    VK_CHECK(vkCreateFence(m_device, &fenceInfo, nullptr, &m_vkFence));
}

VulkanFence::~VulkanFence() { vkDestroyFence(m_device, m_vkFence, nullptr); }

void VulkanFence::wait() { vkWaitForFences(m_device, 1, &m_vkFence, VK_TRUE, VK_WHOLE_SIZE); }

void VulkanFence::onRefCntReachedZero() const
{
    constexpr static uint32_t kMaxFencesInPool = 32;

    if (m_pool->m_fences.size() < kMaxFencesInPool)
    {
        // Recycle the fence!
        vkResetFences(m_device, 1, &m_vkFence);
        auto mutableThis = const_cast<VulkanFence*>(this);
        m_pool->m_fences.push_back(std::unique_ptr<VulkanFence>(mutableThis));
        mutableThis->m_pool = nullptr;
        assert(debugging_refcnt() == 0);
        return;
    }

    delete this;
}

rcp<VulkanFence> VulkanFencePool::makeFence()
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
} // namespace rive::pls
