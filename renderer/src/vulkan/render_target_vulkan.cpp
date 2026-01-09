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

VkImage RenderTargetVulkanImpl::accessTargetImage(
    VkCommandBuffer commandBuffer,
    const vkutil::ImageAccess& dstAccess,
    vkutil::ImageAccessAction imageAccessAction)
{
    m_targetLastAccess = m_vk->simpleImageMemoryBarrier(commandBuffer,
                                                        m_targetLastAccess,
                                                        dstAccess,
                                                        m_targetImage,
                                                        imageAccessAction);
    return m_targetImage;
}

VkImageView RenderTargetVulkanImpl::accessTargetImageView(
    VkCommandBuffer commandBuffer,
    const vkutil::ImageAccess& dstAccess,
    vkutil::ImageAccessAction imageAccessAction)
{
    accessTargetImage(commandBuffer, dstAccess, imageAccessAction);
    return m_targetImageView;
}

VkImageView RenderTargetVulkan::clearTargetImageView(
    VkCommandBuffer commandBuffer,
    ColorInt clearColor,
    const vkutil::ImageAccess& dstAccessAfterClear)
{
    m_vk->clearColorImage(
        commandBuffer,
        clearColor,
        accessTargetImage(commandBuffer,
                          {
                              .pipelineStages = VK_PIPELINE_STAGE_TRANSFER_BIT,
                              .accessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                              .layout = VK_IMAGE_LAYOUT_GENERAL,
                          },
                          vkutil::ImageAccessAction::invalidateContents),
        VK_IMAGE_LAYOUT_GENERAL);

    return accessTargetImageView(commandBuffer, dstAccessAfterClear);
}

vkutil::Texture2D* RenderTargetVulkan::accessOffscreenColorTexture(
    VkCommandBuffer commandBuffer,
    const vkutil::ImageAccess& dstAccess,
    vkutil::ImageAccessAction imageAccessAction)
{
    if (m_offscreenColorTexture == nullptr)
    {
        m_offscreenColorTexture = m_vk->makeTexture2D(
            {
                .format = m_framebufferFormat,
                .extent = {width(), height()},
                .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                         VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
                         VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                         VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            },
            "offscreen color texture");
    }

    m_offscreenColorTexture->barrier(commandBuffer,
                                     dstAccess,
                                     imageAccessAction);

    return m_offscreenColorTexture.get();
}

vkutil::Texture2D* RenderTargetVulkan::copyTargetImageToOffscreenColorTexture(
    VkCommandBuffer commandBuffer,
    const vkutil::ImageAccess& dstAccessAfterCopy,
    const IAABB& copyBounds)
{
    m_vk->blitSubRect(
        commandBuffer,
        accessTargetImage(commandBuffer,
                          {
                              .pipelineStages = VK_PIPELINE_STAGE_TRANSFER_BIT,
                              .accessMask = VK_ACCESS_TRANSFER_READ_BIT,
                              .layout = VK_IMAGE_LAYOUT_GENERAL,
                          }),
        VK_IMAGE_LAYOUT_GENERAL,
        accessOffscreenColorTexture(
            commandBuffer,
            {
                .pipelineStages = VK_PIPELINE_STAGE_TRANSFER_BIT,
                .accessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .layout = VK_IMAGE_LAYOUT_GENERAL,
            },
            vkutil::ImageAccessAction::invalidateContents)
            ->vkImage(),
        VK_IMAGE_LAYOUT_GENERAL,
        copyBounds);

    return accessOffscreenColorTexture(commandBuffer, dstAccessAfterCopy);
}

vkutil::Texture2D* RenderTargetVulkan::msaaColorTexture()
{
    if (m_msaaColorTexture == nullptr)
    {
        m_msaaColorTexture = m_vk->makeTexture2D(
            {
                .format = m_framebufferFormat,
                .extent = {width(), height(), 1},
                .samples = VK_SAMPLE_COUNT_4_BIT,
                .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                         VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                         VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
            },
            "MSAA Color Texture");
    }
    return m_msaaColorTexture.get();
}

vkutil::Texture2D* RenderTargetVulkan::msaaDepthStencilTexture()
{
    if (m_msaaDepthStencilTexture == nullptr)
    {
        m_msaaDepthStencilTexture = m_vk->makeTexture2D(
            {
                .format = vkutil::get_preferred_depth_stencil_format(
                    m_vk->supportsD24S8()),
                .extent = {width(), height(), 1},
                .samples = VK_SAMPLE_COUNT_4_BIT,
                .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                         VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT,
            },
            "MSAA Depth/Stencil Texture");
    }
    return m_msaaDepthStencilTexture.get();
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
