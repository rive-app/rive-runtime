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

    // InterlockMode::rasterOrdering.
    vkutil::Texture* clipTextureR32UI();
    vkutil::TextureView* clipTextureViewR32UI();
    vkutil::Texture* scratchColorTexture();
    vkutil::TextureView* scratchColorTextureView();
    vkutil::Texture* coverageTexture();
    vkutil::TextureView* coverageTextureView();

    // InterlockMode::atomics.
    vkutil::Texture* clipTextureRGBA8();
    vkutil::TextureView* clipTextureViewRGBA8();
    vkutil::Texture* coverageAtomicTexture();
    vkutil::TextureView* coverageAtomicTextureView();

    // InterlockMode::msaa.
    vkutil::Texture* depthStencilTexture();
    vkutil::TextureView* depthStencilTextureView();

    const rcp<VulkanContext> m_vk;
    const VkFormat m_framebufferFormat;
    const VkImageUsageFlags m_targetUsageFlags;

    // Used when m_targetTextureView does not have
    // VK_ACCESS_INPUT_ATTACHMENT_READ_BIT
    rcp<vkutil::Texture> m_offscreenColorTexture;
    rcp<vkutil::TextureView> m_offscreenColorTextureView;
    vkutil::TextureAccess m_offscreenLastAccess;

    // InterlockMode::rasterOrdering.
    rcp<vkutil::Texture> m_clipTextureR32UI;
    rcp<vkutil::TextureView> m_clipTextureViewR32UI;
    rcp<vkutil::Texture> m_scratchColorTexture;
    rcp<vkutil::TextureView> m_scratchColorTextureView;
    rcp<vkutil::Texture> m_coverageTexture;
    rcp<vkutil::TextureView> m_coverageTextureView;

    // InterlockMode::atomics.
    rcp<vkutil::Texture> m_clipTextureRGBA8;
    rcp<vkutil::TextureView> m_clipTextureViewRGBA8;
    rcp<vkutil::Texture> m_coverageAtomicTexture;
    rcp<vkutil::TextureView> m_coverageAtomicTextureView;

    // InterlockMode::msaa.
    rcp<vkutil::Texture> m_depthStencilTexture;
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
