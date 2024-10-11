/*
 * Copyright 2024 Rive
 */

#include "rive/renderer/vulkan/vkutil_resource_pool.hpp"

namespace rive::gpu::vkutil
{
ResourceFactory<CommandBuffer>::ResourceFactory(rcp<VulkanContext> vk,
                                                uint32_t queueFamilyIndex) :
    m_vk(std::move(vk))
{
    VkCommandPoolCreateInfo commandPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VkCommandPoolCreateFlagBits::
            VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queueFamilyIndex,
    };
    VK_CHECK(m_vk->CreateCommandPool(m_vk->device,
                                     &commandPoolCreateInfo,
                                     nullptr,
                                     &m_vkCommandPool));
}

ResourceFactory<CommandBuffer>::~ResourceFactory()
{
    m_vk->DestroyCommandPool(m_vk->device, m_vkCommandPool, nullptr);
}

CommandBuffer::CommandBuffer(rcp<VulkanContext> vk,
                             VkCommandPool vkCommandPool) :
    m_vk(std::move(vk)), m_vkCommandPool(vkCommandPool)
{
    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = m_vkCommandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };
    VK_CHECK(m_vk->AllocateCommandBuffers(m_vk->device,
                                          &commandBufferAllocateInfo,
                                          &m_vkCommandBuffer));
}

CommandBuffer::~CommandBuffer()
{
    m_vk->FreeCommandBuffers(m_vk->device,
                             m_vkCommandPool,
                             1,
                             &m_vkCommandBuffer);
}

void CommandBuffer::reset() { m_vk->ResetCommandBuffer(m_vkCommandBuffer, {}); }

Semaphore::Semaphore(rcp<VulkanContext> vk) : m_vk(std::move(vk))
{
    VkSemaphoreCreateInfo semaphoreCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };
    VK_CHECK(m_vk->CreateSemaphore(m_vk->device,
                                   &semaphoreCreateInfo,
                                   nullptr,
                                   &m_vkSemaphore));
}

Semaphore::~Semaphore()
{
    m_vk->DestroySemaphore(m_vk->device, m_vkSemaphore, nullptr);
}

void Semaphore::reset()
{
    // Semaphores get reset automatically afrer a wait operation:
    //
    // "semaphore wait operations set the semaphores created with a
    //  VkSemaphoreType of VK_SEMAPHORE_TYPE_BINARY to the unsignaled state"
}

Fence::Fence(rcp<VulkanContext> vk) : m_vk(std::move(vk))
{
    VkFenceCreateInfo fenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    };
    VK_CHECK(m_vk->CreateFence(m_vk->device, &fenceInfo, nullptr, &m_vkFence));
}

Fence::~Fence() { m_vk->DestroyFence(m_vk->device, m_vkFence, nullptr); }

void Fence::reset() { m_vk->ResetFences(m_vk->device, 1, &m_vkFence); }

void Fence::wait()
{
    while (m_vk->WaitForFences(m_vk->device, 1, &m_vkFence, VK_TRUE, 1000) ==
           VK_TIMEOUT)
    {
        // Keep waiting.
    }
}
} // namespace rive::gpu::vkutil
