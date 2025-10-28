/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/renderer/render_target.hpp"
#include "rive/renderer/vulkan/vulkan_context.hpp"
#include <vulkan/vulkan.h>

namespace rive::gpu
{
class RenderTargetVulkan : public RenderTarget
{
public:
    const VkFormat framebufferFormat() const { return m_framebufferFormat; }
    const VkImageUsageFlags targetUsageFlags() const
    {
        return m_targetUsageFlags;
    }

    // Returns the target image in the requested layout, performing a pipeline
    // barrier if necessary.
    virtual VkImage accessTargetImage(
        VkCommandBuffer,
        const vkutil::ImageAccess& dstAccess,
        vkutil::ImageAccessAction =
            vkutil::ImageAccessAction::preserveContents) = 0;

    // Returns the target image view, with its image in the requested layout,
    // performing a pipeline barrier if necessary.
    virtual VkImageView accessTargetImageView(
        VkCommandBuffer,
        const vkutil::ImageAccess& dstAccess,
        vkutil::ImageAccessAction =
            vkutil::ImageAccessAction::preserveContents) = 0;

protected:
    friend class RenderContextVulkanImpl;

    RenderTargetVulkan(rcp<VulkanContext> vk,
                       uint32_t width,
                       uint32_t height,
                       VkFormat framebufferFormat,
                       VkImageUsageFlags targetUsageFlags);

    // Returns the offscreen texture in the requested layout, performing a
    // pipeline barrier if necessary.
    vkutil::Texture2D* accessOffscreenColorTexture(
        VkCommandBuffer,
        const vkutil::ImageAccess& dstAccess,
        vkutil::ImageAccessAction =
            vkutil::ImageAccessAction::preserveContents);

    // Copies the target image into the offscreen color texture (for the
    // intended purpose of supporting gpu::LoadAction::preserveRenderTarget).
    // Returns the offscreen texture in the requested layout, performing a
    // pipeline barrier if necessary.
    vkutil::Texture2D* copyTargetImageToOffscreenColorTexture(
        VkCommandBuffer,
        const vkutil::ImageAccess& dstAccess,
        const IAABB& copyBounds);

    // InterlockMode::msaa.
    vkutil::Texture2D* msaaColorTexture();
    vkutil::Texture2D* msaaDepthStencilTexture();

    const rcp<VulkanContext> m_vk;
    const VkFormat m_framebufferFormat;
    const VkImageUsageFlags m_targetUsageFlags;

    // Used when m_targetTextureView does not have
    // VK_ACCESS_INPUT_ATTACHMENT_READ_BIT
    rcp<vkutil::Texture2D> m_offscreenColorTexture;

    // InterlockMode::msaa.
    rcp<vkutil::Texture2D> m_msaaColorTexture;
    rcp<vkutil::Texture2D> m_msaaDepthStencilTexture;
};

class RenderTargetVulkanImpl : public RenderTargetVulkan
{
public:
    RenderTargetVulkanImpl(rcp<VulkanContext> vk,
                           uint32_t width,
                           uint32_t height,
                           VkFormat framebufferFormat,
                           VkImageUsageFlags targetUsageFlags) :
        RenderTargetVulkan(std::move(vk),
                           width,
                           height,
                           framebufferFormat,
                           targetUsageFlags)
    {}

    void setTargetImageView(VkImageView imageView,
                            VkImage image,
                            vkutil::ImageAccess targetLastAccess)
    {
        m_targetImageView = imageView;
        m_targetImage = image;
        m_targetLastAccess = targetLastAccess;
    }

    const vkutil::ImageAccess& targetLastAccess() const
    {
        return m_targetLastAccess;
    }

    VkImage accessTargetImage(
        VkCommandBuffer,
        const vkutil::ImageAccess& dstAccess,
        vkutil::ImageAccessAction =
            vkutil::ImageAccessAction::preserveContents) override;

    VkImageView accessTargetImageView(
        VkCommandBuffer,
        const vkutil::ImageAccess& dstAccess,
        vkutil::ImageAccessAction =
            vkutil::ImageAccessAction::preserveContents) override;

private:
    VkImageView m_targetImageView = VK_NULL_HANDLE;
    VkImage m_targetImage = VK_NULL_HANDLE;
    vkutil::ImageAccess m_targetLastAccess;
};
} // namespace rive::gpu
