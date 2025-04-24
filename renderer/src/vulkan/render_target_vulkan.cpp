/*
 * Copyright 2023 Rive
 */

#include "rive/renderer/vulkan/render_target_vulkan.hpp"

#include "rive/renderer/vulkan/render_context_vulkan_impl.hpp"

namespace rive::gpu
{
RenderTargetVulkan::RenderTargetVulkan(rcp<VulkanContext> vk,
                                       uint32_t width,
                                       uint32_t height,
                                       VkFormat framebufferFormat,
                                       VkImageUsageFlags targetUsageFlags) :
    RenderTarget(width, height),
    m_vk(std::move(vk)),
    m_framebufferFormat(framebufferFormat),
    m_targetUsageFlags(targetUsageFlags)
{
#ifndef NDEBUG
    // In order to implement blend modes, the target texture needs to either
    // support input attachment usage (ideal), or else transfers.
    constexpr static VkImageUsageFlags TRANSFER_SRC_AND_DST =
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    assert((m_targetUsageFlags & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT) ||
           (m_targetUsageFlags & TRANSFER_SRC_AND_DST) == TRANSFER_SRC_AND_DST);
#endif
}

static void barrier(VulkanContext* vk,
                    VkCommandBuffer commandBuffer,
                    vkutil::TextureAccess* srcAccess,
                    const vkutil::TextureAccess& dstAccess,
                    vkutil::TextureAccessAction textureAccessAction,
                    VkImage image)
{
    if (image != VK_NULL_HANDLE && dstAccess != *srcAccess)
    {
        if (textureAccessAction ==
            vkutil::TextureAccessAction::invalidateContents)
        {
            srcAccess->layout = VK_IMAGE_LAYOUT_UNDEFINED;
        }
        vk->simpleImageMemoryBarrier(commandBuffer,
                                     *srcAccess,
                                     dstAccess,
                                     image);
        *srcAccess = dstAccess;
    }
}

VkImage RenderTargetVulkanImpl::accessTargetImage(
    VkCommandBuffer commandBuffer,
    const vkutil::TextureAccess& dstAccess,
    vkutil::TextureAccessAction textureAccessAction)
{
    barrier(m_vk.get(),
            commandBuffer,
            &m_targetLastAccess,
            dstAccess,
            textureAccessAction,
            m_targetImage);
    return m_targetImage;
}

VkImageView RenderTargetVulkanImpl::accessTargetImageView(
    VkCommandBuffer commandBuffer,
    const vkutil::TextureAccess& dstAccess,
    vkutil::TextureAccessAction textureAccessAction)
{
    accessTargetImage(commandBuffer, dstAccess, textureAccessAction);
    return m_targetImageView;
}

VkImage RenderTargetVulkan::accessOffscreenColorTexture(
    VkCommandBuffer commandBuffer,
    const vkutil::TextureAccess& dstAccess,
    vkutil::TextureAccessAction textureAccessAction)
{
    if (m_offscreenColorTextureView == nullptr)
    {
        m_offscreenColorTexture = m_vk->makeTexture({
            .format = m_framebufferFormat,
            .extent = {width(), height(), 1},
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                     VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        });

        m_offscreenLastAccess = {
            .pipelineStages = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
            .accessMask = VK_ACCESS_NONE,
            .layout = VK_IMAGE_LAYOUT_UNDEFINED,
        };

        m_offscreenColorTextureView =
            m_vk->makeTextureView(m_offscreenColorTexture);
    }

    barrier(m_vk.get(),
            commandBuffer,
            &m_offscreenLastAccess,
            dstAccess,
            textureAccessAction,
            *m_offscreenColorTexture);

    return *m_offscreenColorTexture;
}

VkImageView RenderTargetVulkan::accessOffscreenColorTextureView(
    VkCommandBuffer commandBuffer,
    const vkutil::TextureAccess& dstAccess,
    vkutil::TextureAccessAction textureAccessAction)
{
    accessOffscreenColorTexture(commandBuffer, dstAccess, textureAccessAction);
    return *m_offscreenColorTextureView;
}

vkutil::Texture* RenderTargetVulkan::clipTextureR32UI()
{
    if (m_clipTextureR32UI == nullptr)
    {
        m_clipTextureR32UI = m_vk->makeTexture({
            .format = VK_FORMAT_R32_UINT,
            .extent = {width(), height(), 1},
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
        });
    }
    return m_clipTextureR32UI.get();
}

vkutil::TextureView* RenderTargetVulkan::clipTextureViewR32UI()
{
    if (m_clipTextureViewR32UI == nullptr)
    {
        m_clipTextureViewR32UI =
            m_vk->makeTextureView(ref_rcp(clipTextureR32UI()));
    }
    return m_clipTextureViewR32UI.get();
}

vkutil::Texture* RenderTargetVulkan::scratchColorTexture()
{
    if (m_scratchColorTexture == nullptr)
    {
        m_scratchColorTexture = m_vk->makeTexture({
            .format = m_framebufferFormat,
            .extent = {width(), height(), 1},
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
        });
    }
    return m_scratchColorTexture.get();
}

vkutil::TextureView* RenderTargetVulkan::scratchColorTextureView()
{
    if (m_scratchColorTextureView == nullptr)
    {
        m_scratchColorTextureView =
            m_vk->makeTextureView(ref_rcp(scratchColorTexture()));
    }
    return m_scratchColorTextureView.get();
}

vkutil::Texture* RenderTargetVulkan::coverageTexture()
{
    if (m_coverageTexture == nullptr)
    {
        m_coverageTexture = m_vk->makeTexture({
            .format = VK_FORMAT_R32_UINT,
            .extent = {width(), height(), 1},
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
        });
    }
    return m_coverageTexture.get();
}

vkutil::TextureView* RenderTargetVulkan::coverageTextureView()
{
    if (m_coverageTextureView == nullptr)
    {
        m_coverageTextureView =
            m_vk->makeTextureView(ref_rcp(coverageTexture()));
    }
    return m_coverageTextureView.get();
}

vkutil::Texture* RenderTargetVulkan::clipTextureRGBA8()
{
    if (m_clipTextureRGBA8 == nullptr)
    {
        m_clipTextureRGBA8 = m_vk->makeTexture({
            .format = VK_FORMAT_R8G8B8A8_UNORM,
            .extent = {width(), height(), 1},
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
        });
    }
    return m_clipTextureRGBA8.get();
}

vkutil::TextureView* RenderTargetVulkan::clipTextureViewRGBA8()
{
    if (m_clipTextureViewRGBA8 == nullptr)
    {
        m_clipTextureViewRGBA8 =
            m_vk->makeTextureView(ref_rcp(clipTextureRGBA8()));
    }
    return m_clipTextureViewRGBA8.get();
}

vkutil::Texture* RenderTargetVulkan::coverageAtomicTexture()
{
    if (m_coverageAtomicTexture == nullptr)
    {
        m_coverageAtomicTexture = m_vk->makeTexture({
            .format = VK_FORMAT_R32_UINT,
            .extent = {width(), height(), 1},
            .usage =
                VK_IMAGE_USAGE_STORAGE_BIT |
                VK_IMAGE_USAGE_TRANSFER_DST_BIT, // For vkCmdClearColorImage
        });
    }
    return m_coverageAtomicTexture.get();
}

vkutil::TextureView* RenderTargetVulkan::coverageAtomicTextureView()
{
    if (m_coverageAtomicTextureView == nullptr)
    {
        m_coverageAtomicTextureView =
            m_vk->makeTextureView(ref_rcp(coverageAtomicTexture()));
    }
    return m_coverageAtomicTextureView.get();
}

vkutil::Texture* RenderTargetVulkan::depthStencilTexture()
{
    if (m_depthStencilTexture == nullptr)
    {
        m_depthStencilTexture = m_vk->makeTexture({
            .format = vkutil::get_preferred_depth_stencil_format(
                m_vk->supportsD24S8()),
            .extent = {width(), height(), 1},
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        });
    }
    return m_depthStencilTexture.get();
}

vkutil::TextureView* RenderTargetVulkan::depthStencilTextureView()
{
    if (m_depthStencilTextureView == nullptr)
    {
        m_depthStencilTextureView =
            m_vk->makeTextureView(ref_rcp(depthStencilTexture()));
    }
    return m_depthStencilTextureView.get();
}

rcp<RenderTargetVulkanImpl> RenderContextVulkanImpl::makeRenderTarget(
    uint32_t width,
    uint32_t height,
    VkFormat framebufferFormat,
    VkImageUsageFlags targetUsageFlags)
{
    return rcp(new RenderTargetVulkanImpl(m_vk,
                                          width,
                                          height,
                                          framebufferFormat,
                                          targetUsageFlags));
}
} // namespace rive::gpu
