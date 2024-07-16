/*
 * Copyright 2024 Rive
 */

#pragma once

#include "rive/pls/pls.hpp"
#include <vulkan/vulkan.h>

namespace rive::pls
{
class VulkanFencePool;

// Wraps a VkFence created specifically for a PLS flush.
// Recycles the VulkanFence in its pool when its ref count reaches zero.
class VulkanFence : public CommandBufferCompletionFence
{
public:
    VulkanFence(rcp<VulkanFencePool>);
    ~VulkanFence() override;

    operator VkFence() const { return m_vkFence; }

    void wait() override;

private:
    // Recycles the fence in the VulkanFencePool.
    void onRefCntReachedZero() const override;

    friend class VulkanFencePool;
    const VkDevice m_device;
    rcp<VulkanFencePool> m_pool;
    VkFence m_vkFence;
};

// Manages a pool of recyclable VulkanFences.
class VulkanFencePool : public RefCnt<VulkanFencePool>
{
public:
    VulkanFencePool(VkDevice device) : m_device(device) {}

    rcp<VulkanFence> makeFence();

private:
    friend class VulkanFence;
    const VkDevice m_device;
    std::vector<std::unique_ptr<VulkanFence>> m_fences;
};
} // namespace rive::pls
