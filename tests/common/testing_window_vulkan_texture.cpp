/*
 * Copyright 2024 Rive
 */

#include "testing_window.hpp"

#ifndef RIVE_VULKAN

std::unique_ptr<TestingWindow> TestingWindow::MakeVulkanTexture(bool coreFeaturesOnly,
                                                                const char* gpuNameFilter)
{
    return nullptr;
}

#else

#include "rive_vk_bootstrap/rive_vk_bootstrap.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "rive/renderer/vulkan/render_context_vulkan_impl.hpp"
#include "rive/renderer/vulkan/vkutil_resource_pool.hpp"

namespace rive::gpu
{
class TestingWindowVulkanTexture : public TestingWindow
{
public:
    TestingWindowVulkanTexture(bool coreFeaturesOnly, const char* gpuNameFilter)
    {
        rive_vkb::load_vulkan();

        m_instance = VKB_CHECK(vkb::InstanceBuilder()
                                   .set_app_name("rive_tools")
                                   .set_engine_name("Rive Renderer")
                                   .set_headless(true)
                                   .enable_validation_layers()
                                   .set_debug_callback(rive_vkb::default_debug_callback)
                                   .build());

        VulkanFeatures vulkanFeatures;
        std::tie(m_physicalDevice, vulkanFeatures) = rive_vkb::select_physical_device(
            m_instance,
            coreFeaturesOnly ? rive_vkb::FeatureSet::coreOnly : rive_vkb::FeatureSet::allAvailable,
            gpuNameFilter);
        m_device = VKB_CHECK(vkb::DeviceBuilder(m_physicalDevice).build());
        m_queue = VKB_CHECK(m_device.get_queue(vkb::QueueType::graphics));
        m_renderContext = RenderContextVulkanImpl::MakeContext(m_instance,
                                                               m_physicalDevice,
                                                               m_device,
                                                               vulkanFeatures,
                                                               m_instance.fp_vkGetInstanceProcAddr,
                                                               m_instance.fp_vkGetDeviceProcAddr);
        m_vkbTable = m_device.make_table();

        m_semaphorePool = make_rcp<vkutil::ResourcePool<vkutil::Semaphore>>(ref_rcp(vk()));
        m_fencePool = make_rcp<vkutil::ResourcePool<vkutil::Fence>>(ref_rcp(vk()));
        m_commandBufferPool = make_rcp<vkutil::ResourcePool<vkutil::CommandBuffer>>(
            ref_rcp(vk()),
            *m_device.get_queue_index(vkb::QueueType::graphics));

        m_textureUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                              VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                              (coreFeaturesOnly ? VK_IMAGE_USAGE_TRANSFER_DST_BIT
                                                : VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);
    }

    ~TestingWindowVulkanTexture()
    {
        // Destroy these before destroying the VkDevice.
        m_renderContext.reset();
        m_renderTarget.reset();
        m_texture.reset();
        m_pixelReadBuffer.reset();
        m_lastFrameSemaphore.reset();
        m_lastFrameFence.reset();

        VK_CHECK(m_vkbTable.queueWaitIdle(m_queue));

        m_semaphorePool.reset();
        m_fencePool.reset();
        m_commandBufferPool.reset();

        vkb::destroy_device(m_device);
        vkb::destroy_instance(m_instance);
    }

    rive::Factory* factory() override { return m_renderContext.get(); }

    rive::gpu::RenderContext* renderContext() const override { return m_renderContext.get(); }

    std::unique_ptr<rive::Renderer> beginFrame(uint32_t clearColor, bool doClear) override
    {
        if (m_lastFrameFence != nullptr)
        {
            m_lastFrameFence->wait();
        }
        m_lastFrameFence = m_fencePool->make();

        m_commandBuffer = m_commandBufferPool->make();
        VkCommandBufferBeginInfo commandBufferBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        };
        m_vkbTable.beginCommandBuffer(*m_commandBuffer, &commandBufferBeginInfo);

        rive::gpu::RenderContext::FrameDescriptor frameDescriptor = {
            .renderTargetWidth = m_width,
            .renderTargetHeight = m_height,
            .loadAction = doClear ? rive::gpu::LoadAction::clear
                                  : rive::gpu::LoadAction::preserveRenderTarget,
            .clearColor = clearColor,
        };
        m_renderContext->beginFrame(frameDescriptor);
        return std::make_unique<RiveRenderer>(m_renderContext.get());
    }

    void flushPLSContext() final
    {
        if (m_renderTarget == nullptr || m_renderTarget->height() != m_height ||
            m_renderTarget->width() != m_width)
        {
            m_texture = vk()->makeTexture({
                .format = VK_FORMAT_B8G8R8A8_UNORM,
                .extent = {static_cast<uint32_t>(m_width), static_cast<uint32_t>(m_height), 1},
                .usage = m_textureUsageFlags,
            });
            m_pixelReadBuffer = vk()->makeBuffer(
                {
                    .size = static_cast<size_t>(m_height) * m_width * 4,
                    .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                },
                vkutil::Mappability::readWrite);
            m_renderTarget = impl()->makeRenderTarget(m_width, m_height, m_texture->info().format);
            m_renderTarget->setTargetTextureView(vk()->makeTextureView(m_texture), {});
        }

        m_renderContext->flush({
            .renderTarget = m_renderTarget.get(),
            .externalCommandBuffer = *m_commandBuffer,
            .frameCompletionFence = m_lastFrameFence.get(),
        });
    }

    void endFrame(std::vector<uint8_t>* pixelData) override
    {
        flushPLSContext();

        // Copy the framebuffer out to a buffer.
        m_renderTarget->setTargetLastAccess(
            vk()->simpleImageMemoryBarrier(*m_commandBuffer,
                                           m_renderTarget->targetLastAccess(),
                                           {
                                               .pipelineStages = VK_PIPELINE_STAGE_TRANSFER_BIT,
                                               .accessMask = VK_ACCESS_TRANSFER_READ_BIT,
                                               .layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                           },
                                           *m_texture));

        VkBufferImageCopy imageCopyDesc = {
            .imageSubresource =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = 0,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
            .imageExtent = {m_width, m_height, 1},
        };

        m_vkbTable.cmdCopyImageToBuffer(*m_commandBuffer,
                                        *m_texture,
                                        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                        *m_pixelReadBuffer,
                                        1,
                                        &imageCopyDesc);

        vk()->bufferMemoryBarrier(*m_commandBuffer,
                                  VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  VK_PIPELINE_STAGE_HOST_BIT,
                                  0,
                                  {
                                      .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                                      .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
                                      .buffer = *m_pixelReadBuffer,
                                  });

        VK_CHECK(m_vkbTable.endCommandBuffer(*m_commandBuffer));

        auto nextFrameSemaphore = m_semaphorePool->make();
        VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

        VkSubmitInfo submitInfo = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pWaitDstStageMask = &waitDstStageMask,
            .commandBufferCount = 1,
            .pCommandBuffers = m_commandBuffer->vkCommandBufferAddressOf(),
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = nextFrameSemaphore->vkSemaphoreAddressOf(),
        };
        if (m_lastFrameSemaphore != nullptr)
        {
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = m_lastFrameSemaphore->vkSemaphoreAddressOf();
        }

        VK_CHECK(m_vkbTable.queueSubmit(m_queue, 1, &submitInfo, *m_lastFrameFence));

        m_lastFrameSemaphore = std::move(nextFrameSemaphore);

        if (pixelData != nullptr)
        {
            // Wait for all rendering to complete before transferring the
            // framebuffer data to pixelData.
            m_lastFrameFence->wait();
            m_pixelReadBuffer->invalidateContents();

            // Copy the buffer containing the framebuffer contents to pixelData.
            pixelData->resize(m_height * m_width * 4);
            for (uint32_t y = 0; y < m_height; ++y)
            {
                auto src =
                    static_cast<const uint8_t*>(m_pixelReadBuffer->contents()) + m_width * 4 * y;
                uint8_t* dst = pixelData->data() + (m_height - y - 1) * m_width * 4;
                memcpy(dst, src, m_width * 4);
                if (m_texture->info().format == VK_FORMAT_B8G8R8A8_UNORM)
                {
                    // Reverse bgr -> rgb.
                    for (uint32_t x = 0; x < m_width * 4; x += 4)
                    {
                        std::swap(dst[x], dst[x + 2]);
                    }
                }
            }
        }

        m_commandBuffer = nullptr;
    }

private:
    RenderContextVulkanImpl* impl() const
    {
        return m_renderContext->static_impl_cast<RenderContextVulkanImpl>();
    }

    VulkanContext* vk() const { return impl()->vulkanContext(); }

    vkb::Instance m_instance;
    vkb::PhysicalDevice m_physicalDevice;
    vkb::Device m_device;
    VkQueue m_queue;
    std::unique_ptr<RenderContext> m_renderContext;
    vkb::DispatchTable m_vkbTable;
    rcp<vkutil::ResourcePool<vkutil::CommandBuffer>> m_commandBufferPool;
    rcp<vkutil::CommandBuffer> m_commandBuffer;
    rcp<vkutil::ResourcePool<vkutil::Semaphore>> m_semaphorePool;
    rcp<vkutil::Semaphore> m_lastFrameSemaphore;
    rcp<vkutil::ResourcePool<vkutil::Fence>> m_fencePool;
    rcp<vkutil::Fence> m_lastFrameFence;
    VkImageUsageFlags m_textureUsageFlags;
    rcp<vkutil::Texture> m_texture;
    rcp<vkutil::Buffer> m_pixelReadBuffer;
    rcp<RenderTargetVulkan> m_renderTarget;
};
}; // namespace rive::gpu

std::unique_ptr<TestingWindow> TestingWindow::MakeVulkanTexture(bool coreFeaturesOnly,
                                                                const char* gpuNameFilter)
{
    return std::make_unique<rive::gpu::TestingWindowVulkanTexture>(coreFeaturesOnly, gpuNameFilter);
}

#endif
