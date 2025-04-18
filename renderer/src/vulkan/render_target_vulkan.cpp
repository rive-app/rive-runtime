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

vkutil::TextureView* RenderTargetVulkan::ensureClipTextureView()
{
    if (m_clipTextureView == nullptr)
    {
        m_clipTexture = m_vk->makeTexture({
            .format = VK_FORMAT_R32_UINT,
            .extent = {width(), height(), 1},
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
        });

        m_clipTextureView = m_vk->makeTextureView(m_clipTexture);
    }

    return m_clipTextureView.get();
}

vkutil::TextureView* RenderTargetVulkan::ensureScratchColorTextureView()
{
    if (m_scratchColorTextureView == nullptr)
    {
        m_scratchColorTexture = m_vk->makeTexture({
            .format = m_framebufferFormat,
            .extent = {width(), height(), 1},
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
        });

        m_scratchColorTextureView =
            m_vk->makeTextureView(m_scratchColorTexture);
    }

    return m_scratchColorTextureView.get();
}

vkutil::TextureView* RenderTargetVulkan::ensureCoverageTextureView()
{
    if (m_coverageTextureView == nullptr)
    {
        m_coverageTexture = m_vk->makeTexture({
            .format = VK_FORMAT_R32_UINT,
            .extent = {width(), height(), 1},
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                     VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
        });

        m_coverageTextureView = m_vk->makeTextureView(m_coverageTexture);
    }

    return m_coverageTextureView.get();
}

vkutil::TextureView* RenderTargetVulkan::ensureCoverageAtomicTextureView()
{
    if (m_coverageAtomicTextureView == nullptr)
    {
        m_coverageAtomicTexture = m_vk->makeTexture({
            .format = VK_FORMAT_R32_UINT,
            .extent = {width(), height(), 1},
            .usage =
                VK_IMAGE_USAGE_STORAGE_BIT |
                VK_IMAGE_USAGE_TRANSFER_DST_BIT, // For vkCmdClearColorImage
        });

        m_coverageAtomicTextureView =
            m_vk->makeTextureView(m_coverageAtomicTexture);
    }

    return m_coverageAtomicTextureView.get();
}

vkutil::TextureView* RenderTargetVulkan::ensureDepthStencilTextureView()
{
    if (m_depthStencilTextureView == nullptr)
    {
        m_depthStencilTexture = m_vk->makeTexture({
            .format = vkutil::get_preferred_depth_stencil_format(
                m_vk->supportsD24S8()),
            .extent = {width(), height(), 1},
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        });

        m_depthStencilTextureView =
            m_vk->makeTextureView(m_depthStencilTexture);
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
