/*
 * Copyright 2024 Rive
 */

#include "testing_window.hpp"

#ifndef RIVE_VULKAN

std::unique_ptr<TestingWindow> TestingWindow::MakeVulkanTexture(const char* gpuNameFilter)
{
    return nullptr;
}

#else

#include "rive_vk_bootstrap/rive_vk_bootstrap.hpp"
#include "rive_vk_bootstrap/vulkan_fence_pool.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "rive/renderer/vulkan/render_context_vulkan_impl.hpp"

namespace rive::gpu
{
class CommandBuffer : public vkutil::RenderingResource
{
public:
    CommandBuffer(const vkb::DispatchTable& vkbTable,
                  rcp<VulkanContext> vk,
                  VkCommandBufferAllocateInfo commandBufferAllocateInfo) :
        RenderingResource(std::move(vk)),
        fp_vkFreeCommandBuffers(vkbTable.fp_vkFreeCommandBuffers),
        m_commandPool(commandBufferAllocateInfo.commandPool)
    {
        commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferAllocateInfo.commandBufferCount = 1,
        VK_CHECK(vkbTable.allocateCommandBuffers(&commandBufferAllocateInfo, &m_commandBuffer));

        VkCommandBufferBeginInfo commandBufferBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        };
        vkbTable.beginCommandBuffer(m_commandBuffer, &commandBufferBeginInfo);
    }

    ~CommandBuffer() override
    {
        fp_vkFreeCommandBuffers(m_vk->device, m_commandPool, 1, &m_commandBuffer);
    }

    operator VkCommandBuffer() const { return m_commandBuffer; }
    const VkCommandBuffer* vkCommandBufferAddressOf() const { return &m_commandBuffer; }

private:
    const PFN_vkFreeCommandBuffers fp_vkFreeCommandBuffers;
    const VkCommandPool m_commandPool;
    VkCommandBuffer m_commandBuffer;
};

class TestingWindowVulkanTexture : public TestingWindow
{
public:
    TestingWindowVulkanTexture(const char* gpuNameFilter)
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
        std::tie(m_physicalDevice, vulkanFeatures) =
            rive_vkb::select_physical_device(m_instance, gpuNameFilter);
        m_device = VKB_CHECK(vkb::DeviceBuilder(m_physicalDevice).build());
        m_queue = VKB_CHECK(m_device.get_queue(vkb::QueueType::graphics));
        m_renderContext = RenderContextVulkanImpl::MakeContext(m_instance,
                                                               m_physicalDevice,
                                                               m_device,
                                                               vulkanFeatures,
                                                               m_instance.fp_vkGetInstanceProcAddr,
                                                               m_instance.fp_vkGetDeviceProcAddr);
        m_vkbTable = m_device.make_table();

        VkCommandPoolCreateInfo commandPoolCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .queueFamilyIndex = *m_device.get_queue_index(vkb::QueueType::graphics),
        };
        VK_CHECK(m_vkbTable.createCommandPool(&commandPoolCreateInfo, nullptr, &m_commandPool));

        m_fencePool = make_rcp<VulkanFencePool>(ref_rcp(vk()));
    }

    ~TestingWindowVulkanTexture()
    {
        // Destroy these before destroying the VkDevice.
        m_renderContext.reset();
        m_renderTarget.reset();
        m_pixelReadBuffer.reset();
        m_texture.reset();
        m_fencePool.reset();
        m_frameFence.reset();

        VK_CHECK(m_vkbTable.queueWaitIdle(m_queue));

        m_vkbTable.destroyCommandPool(m_commandPool, nullptr);

        vkb::destroy_device(m_device);
        vkb::destroy_instance(m_instance);
    }

    rive::Factory* factory() override { return m_renderContext.get(); }

    rive::gpu::RenderContext* renderContext() const override { return m_renderContext.get(); }

    std::unique_ptr<rive::Renderer> beginFrame(uint32_t clearColor, bool doClear) override
    {
        if (m_frameFence != nullptr)
        {
            m_frameFence->wait();
        }
        m_frameFence = m_fencePool->makeFence();

        m_commandBuffer = make_rcp<CommandBuffer>(m_vkbTable,
                                                  ref_rcp(vk()),
                                                  VkCommandBufferAllocateInfo{
                                                      .commandPool = m_commandPool,
                                                      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                                  });

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
                .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
                         VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            });
            m_textureCurrentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            m_renderTarget = impl()->makeRenderTarget(m_width, m_height, m_texture->info().format);
            m_renderTarget->setTargetTextureView(vk()->makeTextureView(m_texture));
            m_pixelReadBuffer = vk()->makeBuffer(
                {
                    .size = static_cast<size_t>(m_height) * m_width * 4,
                    .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                },
                vkutil::Mappability::readWrite);
        }

        vk()->insertImageMemoryBarrier(*m_commandBuffer,
                                       *m_texture,
                                       m_textureCurrentLayout,
                                       VK_IMAGE_LAYOUT_GENERAL);
        m_textureCurrentLayout = VK_IMAGE_LAYOUT_GENERAL;

        m_renderContext->flush({
            .renderTarget = m_renderTarget.get(),
            .externalCommandBuffer = *m_commandBuffer,
            .frameCompletionFence = m_frameFence.get(),
        });
    }

    void endFrame(std::vector<uint8_t>* pixelData) override
    {
        flushPLSContext();

        if (pixelData != nullptr)
        {
            // Copy the framebuffer out to a buffer.
            assert(m_pixelReadBuffer->info().size == m_height * m_width * 4);

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

            vk()->insertImageMemoryBarrier(*m_commandBuffer,
                                           *m_texture,
                                           m_textureCurrentLayout,
                                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            m_textureCurrentLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

            m_vkbTable.cmdCopyImageToBuffer(*m_commandBuffer,
                                            *m_texture,
                                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                            *m_pixelReadBuffer,
                                            1,
                                            &imageCopyDesc);

            vk()->insertBufferMemoryBarrier(*m_commandBuffer,
                                            VK_ACCESS_TRANSFER_WRITE_BIT,
                                            VK_ACCESS_HOST_READ_BIT,
                                            *m_pixelReadBuffer);
        }

        VK_CHECK(m_vkbTable.endCommandBuffer(*m_commandBuffer));

        VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo submitInfo = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pWaitDstStageMask = &waitDstStageMask,
            .commandBufferCount = 1,
            .pCommandBuffers = m_commandBuffer->vkCommandBufferAddressOf(),
        };

        VK_CHECK(m_vkbTable.queueSubmit(m_queue, 1, &submitInfo, *m_frameFence));

        if (pixelData != nullptr)
        {
            // Copy the buffer containing the framebuffer contents to pixelData.
            pixelData->resize(m_height * m_width * 4);

            // Wait for all rendering to complete before transferring the
            // framebuffer data to pixelData.
            m_frameFence->wait();

            assert(m_pixelReadBuffer->info().size == m_height * m_width * 4);
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
    VkCommandPool m_commandPool;
    rcp<CommandBuffer> m_commandBuffer;
    rcp<VulkanFencePool> m_fencePool;
    rcp<VulkanFence> m_frameFence;
    rcp<vkutil::Texture> m_texture;
    VkImageLayout m_textureCurrentLayout;
    rcp<RenderTargetVulkan> m_renderTarget;
    rcp<vkutil::Buffer> m_pixelReadBuffer;
};
}; // namespace rive::gpu

std::unique_ptr<TestingWindow> TestingWindow::MakeVulkanTexture(const char* gpuNameFilter)
{
    return std::make_unique<rive::gpu::TestingWindowVulkanTexture>(gpuNameFilter);
}

#endif
