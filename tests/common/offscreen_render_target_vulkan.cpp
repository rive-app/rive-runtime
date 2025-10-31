/*
 * Copyright 2025 Rive
 */

#include "offscreen_render_target.hpp"

#ifndef RIVE_VULKAN

namespace rive_tests
{
rive::rcp<OffscreenRenderTarget> OffscreenRenderTarget::MakeVulkan(
    rive::gpu::VulkanContext*,
    uint32_t width,
    uint32_t height,
    bool riveRenderable)
{
    return nullptr;
}
} // namespace rive_tests

#else

#include "rive/renderer/rive_render_image.hpp"
#include "rive/renderer/vulkan/vulkan_context.hpp"
#include "rive/renderer/vulkan/render_target_vulkan.hpp"

namespace rive_tests
{
class OffscreenRenderTargetVulkan : public OffscreenRenderTarget
{
public:
    OffscreenRenderTargetVulkan(rive::rcp<rive::gpu::VulkanContext> vk,
                                uint32_t width,
                                uint32_t height,
                                bool riveRenderable) :
        m_textureTarget(rive::make_rcp<TextureTarget>(std::move(vk),
                                                      width,
                                                      height,
                                                      riveRenderable))
    {}

    rive::RenderImage* asRenderImage() override
    {
        return m_textureTarget->renderImage();
    }

    rive::gpu::RenderTarget* asRenderTarget() override
    {
        return m_textureTarget.get();
    }

private:
    class TextureTarget : public rive::gpu::RenderTargetVulkan
    {
    public:
        TextureTarget(rive::rcp<rive::gpu::VulkanContext> vk,
                      uint32_t width,
                      uint32_t height,
                      bool riveRenderable) :
            RenderTargetVulkan(
                std::move(vk),
                width,
                height,
                // BGRA is not riveRenderable when using storage textures for
                // PLS (like in clockwise mode) because storage textures have to
                // be RGBA8. Let's test both formats, but make sure to use RGBA
                // for the riveRenderable case.
                riveRenderable ? VK_FORMAT_R8G8B8A8_UNORM
                               : VK_FORMAT_B8G8R8A8_UNORM,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                    VK_IMAGE_USAGE_SAMPLED_BIT |
                    // Rendering scenarios that use an offscreen color buffer
                    // sometimes require "transfer" usages for copying the
                    // contents back and forth from the main render target.
                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                    VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                    (riveRenderable ? VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
                                          VK_IMAGE_USAGE_STORAGE_BIT
                                    : VK_IMAGE_USAGE_TRANSFER_DST_BIT)),
            m_renderImage(
                rive::make_rcp<rive::RiveRenderImage>(m_vk->makeTexture2D({
                    .format = framebufferFormat(),
                    .extent = {width, height},
                    .usage = targetUsageFlags(),
                })))
        {}

        rive::RiveRenderImage* renderImage() const
        {
            return m_renderImage.get();
        }

        VkImage accessTargetImage(
            VkCommandBuffer commandBuffer,
            const rive::gpu::vkutil::ImageAccess& dstAccess,
            rive::gpu::vkutil::ImageAccessAction accessAction =
                rive::gpu::vkutil::ImageAccessAction::preserveContents) override
        {
            texture()->barrier(commandBuffer, dstAccess, accessAction);
            return texture()->vkImage();
        }

        VkImageView accessTargetImageView(
            VkCommandBuffer commandBuffer,
            const rive::gpu::vkutil::ImageAccess& dstAccess,
            rive::gpu::vkutil::ImageAccessAction accessAction =
                rive::gpu::vkutil::ImageAccessAction::preserveContents) override
        {
            texture()->barrier(commandBuffer, dstAccess, accessAction);
            return texture()->vkImageView();
        }

    private:
        rive::gpu::vkutil::Texture2D* texture()
        {
            return static_cast<rive::gpu::vkutil::Texture2D*>(
                m_renderImage->getTexture());
        }

        rive::rcp<rive::RiveRenderImage> m_renderImage;
    };

    rive::rcp<TextureTarget> m_textureTarget;
};

rive::rcp<OffscreenRenderTarget> OffscreenRenderTarget::MakeVulkan(
    rive::gpu::VulkanContext* vk,
    uint32_t width,
    uint32_t height,
    bool riveRenderable)
{
    return rive::make_rcp<OffscreenRenderTargetVulkan>(ref_rcp(vk),
                                                       width,
                                                       height,
                                                       riveRenderable);
}
}; // namespace rive_tests

#endif
