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
        const vkutil::TextureAccess& dstAccess,
        vkutil::TextureAccessAction =
            vkutil::TextureAccessAction::preserveContents) = 0;

    // Returns the target image view, with its image in the requested layout,
    // performing a pipeline barrier if necessary.
    virtual VkImageView accessTargetImageView(
        VkCommandBuffer,
        const vkutil::TextureAccess& dstAccess,
        vkutil::TextureAccessAction =
            vkutil::TextureAccessAction::preserveContents) = 0;

protected:
    friend class RenderContextVulkanImpl;

    RenderTargetVulkan(rcp<VulkanContext> vk,
                       uint32_t width,
                       uint32_t height,
                       VkFormat framebufferFormat,
                       VkImageUsageFlags targetUsageFlags);

    // Returns the offscreen image in the requested layout, performing a
    // pipeline barrier if necessary.
    VkImage accessOffscreenColorTexture(
        VkCommandBuffer,
        const vkutil::TextureAccess& dstAccess,
        vkutil::TextureAccessAction =
            vkutil::TextureAccessAction::preserveContents);

    // Returns the offscreen image view, with its image in the requested layout,
    // performing a pipeline barrier if necessary.
    VkImageView accessOffscreenColorTextureView(
        VkCommandBuffer,
        const vkutil::TextureAccess& dstAccess,
        vkutil::TextureAccessAction =
            vkutil::TextureAccessAction::preserveContents);

    VkImage coverageTexture() const { return *m_coverageTexture; }
    VkImage clipTexture() const { return *m_clipTexture; }
    VkImage scratchColorTexture() const { return *m_scratchColorTexture; }
    VkImage coverageAtomicTexture() const { return *m_coverageAtomicTexture; }
    VkImage depthStencilTexture() const { return *m_depthStencilTexture; }

    // getters that lazy load if needed.

    vkutil::TextureView* ensureClipTextureView();
    vkutil::TextureView* ensureScratchColorTextureView();
    vkutil::TextureView* ensureCoverageTextureView();
    vkutil::TextureView* ensureCoverageAtomicTextureView();
    vkutil::TextureView* ensureDepthStencilTextureView();

    const rcp<VulkanContext> m_vk;
    const VkFormat m_framebufferFormat;
    const VkImageUsageFlags m_targetUsageFlags;

    // Used when m_targetTextureView does not have
    // VK_ACCESS_INPUT_ATTACHMENT_READ_BIT
    rcp<vkutil::Texture> m_offscreenColorTexture;
    vkutil::TextureAccess m_offscreenLastAccess;

    rcp<vkutil::Texture> m_coverageTexture; // InterlockMode::rasterOrdering.
    rcp<vkutil::Texture> m_clipTexture;
    rcp<vkutil::Texture> m_scratchColorTexture;
    rcp<vkutil::Texture> m_coverageAtomicTexture; // InterlockMode::atomics.
    rcp<vkutil::Texture> m_depthStencilTexture;

    rcp<vkutil::TextureView> m_offscreenColorTextureView;
    rcp<vkutil::TextureView> m_coverageTextureView;
    rcp<vkutil::TextureView> m_clipTextureView;
    rcp<vkutil::TextureView> m_scratchColorTextureView;
    rcp<vkutil::TextureView> m_coverageAtomicTextureView;
    rcp<vkutil::TextureView> m_depthStencilTextureView;
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
                            vkutil::TextureAccess targetLastAccess)
    {
        m_targetImageView = imageView;
        m_targetImage = image;
        m_targetLastAccess = targetLastAccess;
    }

    const vkutil::TextureAccess& targetLastAccess() const
    {
        return m_targetLastAccess;
    }

    VkImage accessTargetImage(
        VkCommandBuffer,
        const vkutil::TextureAccess& dstAccess,
        vkutil::TextureAccessAction =
            vkutil::TextureAccessAction::preserveContents) override;

    VkImageView accessTargetImageView(
        VkCommandBuffer,
        const vkutil::TextureAccess& dstAccess,
        vkutil::TextureAccessAction =
            vkutil::TextureAccessAction::preserveContents) override;

private:
    VkImageView m_targetImageView = VK_NULL_HANDLE;
    VkImage m_targetImage = VK_NULL_HANDLE;
    vkutil::TextureAccess m_targetLastAccess;
};
} // namespace rive::gpu
