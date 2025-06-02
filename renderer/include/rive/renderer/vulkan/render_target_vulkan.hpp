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

    // InterlockMode::rasterOrdering.
    vkutil::Texture2D* clipTextureR32UI();
    vkutil::Texture2D* scratchColorTexture();
    vkutil::Texture2D* coverageTexture();

    // InterlockMode::atomics.
    vkutil::Texture2D* clipTextureRGBA8();
    vkutil::Texture2D* coverageAtomicTexture();

    // InterlockMode::msaa.
    vkutil::Texture2D* depthStencilTexture();
    vkutil::ImageView* depthStencilTextureView();

    const rcp<VulkanContext> m_vk;
    const VkFormat m_framebufferFormat;
    const VkImageUsageFlags m_targetUsageFlags;

    // Used when m_targetTextureView does not have
    // VK_ACCESS_INPUT_ATTACHMENT_READ_BIT
    rcp<vkutil::Texture2D> m_offscreenColorTexture;

    // InterlockMode::rasterOrdering.
    rcp<vkutil::Texture2D> m_clipTextureR32UI;
    rcp<vkutil::Texture2D> m_scratchColorTexture;
    rcp<vkutil::Texture2D> m_coverageTexture;

    // InterlockMode::atomics.
    rcp<vkutil::Texture2D> m_clipTextureRGBA8;
    rcp<vkutil::Texture2D> m_coverageAtomicTexture;

    // InterlockMode::msaa.
    rcp<vkutil::Texture2D> m_depthStencilTexture;
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
