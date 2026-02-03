/*
 * Copyright 2024 Rive
 */

#include "testing_window.hpp"

#if !defined(RIVE_ANDROID) || !defined(RIVE_VULKAN)

TestingWindow* TestingWindow::MakeAndroidVulkan(const BackendParams&,
                                                void* platformWindow)
{
    return nullptr;
}

#else

#include "common/offscreen_render_target.hpp"
#include "rive_vk_bootstrap/vulkan_device.hpp"
#include "rive_vk_bootstrap/vulkan_instance.hpp"
#include "rive_vk_bootstrap/vulkan_swapchain.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "rive/renderer/vulkan/render_context_vulkan_impl.hpp"
#include "rive/renderer/vulkan/render_target_vulkan.hpp"
#include <vulkan/vulkan_android.h>
#include <vk_mem_alloc.h>
#include <android/native_app_glue/android_native_app_glue.h>
#include <android/log.h>
#include <thread>

using namespace rive;
using namespace rive::gpu;

// Send errors to stderr and the Android log, just for redundancy in case one or
// the other gets dropped.
#define LOG_ERROR_LINE(FORMAT, ...)                                            \
    [](auto&&... args) {                                                       \
        fprintf(stderr, FORMAT "\n", std::forward<decltype(args)>(args)...);   \
        __android_log_print(ANDROID_LOG_ERROR,                                 \
                            "rive_android_tests",                              \
                            FORMAT,                                            \
                            std::forward<decltype(args)>(args)...);            \
    }(__VA_ARGS__)

class TestingWindowAndroidVulkan : public TestingWindow
{
public:
    TestingWindowAndroidVulkan(const BackendParams& backendParams,
                               ANativeWindow* window) :
        m_backendParams(backendParams), m_window(window)
    {
        using namespace rive_vkb;

        std::vector<const char*> extensionNames;
        extensionNames.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
        extensionNames.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);

        m_instance = VulkanInstance::Create(VulkanInstance::Options{
            .appName = "Rive Android Test",
            .idealAPIVersion =
                m_backendParams.core ? VK_API_VERSION_1_1 : VK_API_VERSION_1_3,
            .requiredExtensions =
                make_span(extensionNames.data(), extensionNames.size()),
#ifndef NDEBUG
            .desiredValidationType =
                m_backendParams.disableValidationLayers
                    ? VulkanValidationType::none
                    : (m_backendParams.wantVulkanSynchronizationValidation
                           ? VulkanValidationType::synchronization
                           : VulkanValidationType::core),
            .wantDebugCallbacks = !m_backendParams.disableValidationLayers,
#endif
        });
        assert(m_instance != nullptr);

        m_vkDestroySurfaceKHR =
            m_instance->loadInstanceFunc<PFN_vkDestroySurfaceKHR>(
                "vkDestroySurfaceKHR");
        assert(m_vkDestroySurfaceKHR != nullptr);

        VkAndroidSurfaceCreateInfoKHR androidSurfaceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
            .window = m_window,
        };
        auto pfnvkCreateAndroidSurfaceKHR =
            m_instance->loadInstanceFunc<PFN_vkCreateAndroidSurfaceKHR>(
                "vkCreateAndroidSurfaceKHR");
        assert(pfnvkCreateAndroidSurfaceKHR != nullptr);
        VK_CHECK(pfnvkCreateAndroidSurfaceKHR(m_instance->vkInstance(),
                                              &androidSurfaceCreateInfo,
                                              nullptr,
                                              &m_windowSurface));
    }

    ~TestingWindowAndroidVulkan()
    {
        destroyRenderContext();

        if (m_windowSurface != VK_NULL_HANDLE)
        {
            m_vkDestroySurfaceKHR(m_instance->vkInstance(),
                                  m_windowSurface,
                                  nullptr);
        }
    }

    Factory* factory() override
    {
        // Some GMs call factory() during construction (before the call to
        // resize will rebuild the device), so this function also needs to build
        // the render context
        if (m_renderContext == nullptr)
        {
            makeDeviceAndRenderContext();
        }

        return m_renderContext.get();
    }

    rive::gpu::RenderContext* renderContext() const override
    {
        assert(m_renderContext != nullptr);
        return m_renderContext.get();
    }

    rive::gpu::RenderTarget* renderTarget() const override
    {
        assert(m_renderTarget != nullptr);
        return m_renderTarget.get();
    }

    void resize(int width, int height) override
    {
        TestingWindow::resize(width, height);

        if (m_renderContext == nullptr)
        {
            makeDeviceAndRenderContext();
        }

        VkImageUsageFlags renderTargetUsageFlags =
            m_swapchain->imageUsageFlags();
        if (m_alwaysUseOffscreenTexture || m_width > m_androidWindowWidth ||
            m_height > m_androidWindowHeight)
        {
            // Some ARM Mali GPUs experience a device loss when rendering to the
            // overflow texture as an input attachment. The current assumption
            // is that it has to do with some combination of input attachment
            // usage and pixel readbacks. For now, just don't enable the input
            // attachment flag on the overflow texture.
            // This also helps us get stronger testing coverage on the codepath
            // that works without input attachments.
            renderTargetUsageFlags &= ~VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
            m_overflowTexture = vk()->makeTexture2D(
                {
                    .format = m_swapchain->imageFormat(),
                    .extent = {static_cast<uint32_t>(m_width),
                               static_cast<uint32_t>(m_height)},
                    .usage = renderTargetUsageFlags,
                },
                "overflow texture");
        }
        else
        {
            m_overflowTexture.reset();
        }
        m_renderTarget = impl()->makeRenderTarget(m_width,
                                                  m_height,
                                                  m_swapchain->imageFormat(),
                                                  renderTargetUsageFlags);
    }

    rcp<rive_tests::OffscreenRenderTarget> makeOffscreenRenderTarget(
        uint32_t width,
        uint32_t height,
        bool riveRenderable) const override
    {
        return rive_tests::OffscreenRenderTarget::MakeVulkan(vk(),
                                                             width,
                                                             height,
                                                             riveRenderable);
    }

    std::unique_ptr<rive::Renderer> beginFrame(
        const FrameOptions& options) override
    {
        if (m_renderContext == nullptr)
        {
            makeDeviceAndRenderContext();
        }

        m_renderContext->beginFrame(RenderContext::FrameDescriptor{
            .renderTargetWidth = m_width,
            .renderTargetHeight = m_height,
            .loadAction = options.doClear
                              ? gpu::LoadAction::clear
                              : gpu::LoadAction::preserveRenderTarget,
            .clearColor = options.clearColor,
            .msaaSampleCount = m_backendParams.msaa ? 4u : 0u,
            .disableRasterOrdering = options.disableRasterOrdering,
            .wireframe = options.wireframe,
            .fillsDisabled = options.fillsDisabled,
            .strokesDisabled = options.strokesDisabled,
            .clockwiseFillOverride =
                m_backendParams.clockwise || options.clockwiseFillOverride,
            .synthesizedFailureType = options.synthesizedFailureType,
        });

        return std::make_unique<RiveRenderer>(m_renderContext.get());
    }

    void flushPLSContext(RenderTarget* offscreenRenderTarget) override
    {
        if (!m_swapchain->isFrameStarted())
        {
            VK_CHECK(m_swapchain->beginFrame());

            if (m_overflowTexture != nullptr)
            {
                m_renderTarget->setTargetImageView(
                    m_overflowTexture->vkImageView(),
                    m_overflowTexture->vkImage(),
                    m_overflowTexture->lastAccess());
            }
            else
            {
                m_renderTarget->setTargetImageView(
                    m_swapchain->currentVkImageView(),
                    m_swapchain->currentVkImage(),
                    m_swapchain->currentLastAccess());
            }
        }
        m_renderContext->flush({
            .renderTarget = offscreenRenderTarget != nullptr
                                ? offscreenRenderTarget
                                : m_renderTarget.get(),
            .externalCommandBuffer = m_swapchain->currentCommandBuffer(),
            .currentFrameNumber = m_swapchain->currentFrameNumber(),
            .safeFrameNumber = m_swapchain->safeFrameNumber(),
        });
    }

    void endFrame(std::vector<uint8_t>* pixelData) override
    {
        flushPLSContext(nullptr);
        vkutil::ImageAccess swapchainLastAccess;
        if (m_overflowTexture == nullptr)
        {
            // We rendered directly to the window. Submit and read back
            // normally.
            swapchainLastAccess = m_renderTarget->targetLastAccess();
            if (pixelData != nullptr)
            {
                m_swapchain->queueImageCopy(&swapchainLastAccess,
                                            IAABB::MakeWH(m_width, m_height));
            }
        }
        else
        {
            if (m_allowBlitOffscreenToScreen)
            {
                // Blit the overflow texture onto the screen in order to give
                // some visual feedback.
                vkutil::ImageAccess swapchainLastAccess =
                    vk()->simpleImageMemoryBarrier(
                        m_swapchain->currentCommandBuffer(),
                        m_swapchain->currentLastAccess(),
                        {
                            .pipelineStages = VK_PIPELINE_STAGE_TRANSFER_BIT,
                            .accessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                            .layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        },
                        m_swapchain->currentVkImage());
                vk()->blitSubRect(
                    m_swapchain->currentCommandBuffer(),
                    m_renderTarget->accessTargetImage(
                        m_swapchain->currentCommandBuffer(),
                        {
                            .pipelineStages = VK_PIPELINE_STAGE_TRANSFER_BIT,
                            .accessMask = VK_ACCESS_TRANSFER_READ_BIT,
                            .layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                        }),
                    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    m_swapchain->currentVkImage(),
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    IAABB{0,
                          0,
                          std::min<int>(m_width, m_androidWindowWidth),
                          std::min<int>(m_height, m_androidWindowHeight)});
            }

            // TODO: find  the right way to do this (and ideally merge it into
            // the memory barrier in swapchain::endFrame), but this avoids a
            // write-after-write hazard.
            vk()->memoryBarrier(
                m_swapchain->currentCommandBuffer(),
                VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
                VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
                0,
                {
                    .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                    .dstAccessMask = VK_ACCESS_NONE,
                });
            m_overflowTexture->lastAccess() =
                m_renderTarget->targetLastAccess();

            if (pixelData != nullptr)
            {
                m_swapchain->queueImageCopy(m_overflowTexture->vkImage(),
                                            m_swapchain->imageFormat(),
                                            &m_overflowTexture->lastAccess(),
                                            IAABB::MakeWH(m_width, m_height));
            }
        }

        VK_CHECK(m_swapchain->endFrame(swapchainLastAccess));
        if (pixelData != nullptr)
        {
            VK_CHECK(m_swapchain->getPixelsFromLastImageCopy(pixelData));
        }
    }

    void onceAfterGM() override
    {
        // Mali devices with a driver version before 50 have been known to run
        // out of memory, so for these devices, tear down the device between
        // GMs.
        // Adreno devices pre 1.3 (specifically Galaxy A11, Adreno 506, running
        // Vulkan 1.1) also have sporadic crashes, and tearing down the device
        // appears to help.
        if (m_device != nullptr &&
            ((strstr(m_device->name().c_str(), "Mali") != nullptr &&
              m_device->driverVersion().major < 50) ||
             (strstr(m_device->name().c_str(), "Adreno (TM) 5") != nullptr &&
              m_device->vulkanFeatures().apiVersion < VK_API_VERSION_1_3)))
        {
            destroyRenderContext();
        }
    }

private:
    void makeDeviceAndRenderContext()
    {
        using namespace rive_vkb;

        m_device =
            VulkanDevice::Create(*m_instance,
                                 VulkanDevice::Options{
                                     .coreFeaturesOnly = m_backendParams.core,
                                     .printInitializationMessage =
                                         m_printDeviceInitializationMessage,
                                 });
        assert(m_device != nullptr);

        // Only want to print the device initialization message the first time
        m_printDeviceInitializationMessage = false;

        m_renderContext = RenderContextVulkanImpl::MakeContext(
            m_instance->vkInstance(),
            m_device->vkPhysicalDevice(),
            m_device->vkDevice(),
            m_device->vulkanFeatures(),
            m_instance->getVkGetInstanceProcAddrPtr(),
            {.forceAtomicMode = m_backendParams.atomic});

        VkSurfaceCapabilitiesKHR windowCapabilities;
        VK_CHECK(m_device->getSurfaceCapabilities(m_windowSurface,
                                                  &windowCapabilities));

        auto swapOpts = VulkanSwapchain::Options{
            .formatPreferences =
                {
                    {
                        .format = m_backendParams.srgb
                                      ? VK_FORMAT_R8G8B8A8_SRGB
                                      : VK_FORMAT_R8G8B8A8_UNORM,
                        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
                    },
                    // Fall back to either ordering of ARGB
                    {
                        .format = VK_FORMAT_R8G8B8A8_UNORM,
                        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
                    },
                    {
                        .format = VK_FORMAT_B8G8R8A8_UNORM,
                        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
                    },
                },
            .presentModePreferences =
                {
                    VK_PRESENT_MODE_IMMEDIATE_KHR,
                    VK_PRESENT_MODE_FIFO_KHR,
                },
            .imageUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                               VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                               VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        };

        if ((windowCapabilities.supportedUsageFlags &
             VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT) != 0)
        {
            swapOpts.imageUsageFlags |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
        }

        m_swapchain = rive_vkb::VulkanSwapchain::Create(*m_instance,
                                                        *m_device,
                                                        ref_rcp(vk()),
                                                        m_windowSurface,
                                                        swapOpts);
        assert(m_swapchain != nullptr);

        m_androidWindowWidth = m_swapchain->width();
        m_androidWindowHeight = m_swapchain->height();

        if (m_width == 0)
        {
            m_width = m_androidWindowWidth;
        }
        if (m_height == 0)
        {
            m_height = m_androidWindowHeight;
        }

        m_renderTarget =
            impl()->makeRenderTarget(m_width,
                                     m_height,
                                     m_swapchain->imageFormat(),
                                     m_swapchain->imageUsageFlags());

        if (m_device->name() == "Mali-G76" || m_device->name() == "Mali-G72")
        {
            // These devices (like the Huawei P30 or Galaxy S10) will end up
            // with a DEVICE_LOST error if we blit to the screen, so don't.
            m_allowBlitOffscreenToScreen = false;
        }

        if (m_device->name() == "PowerVR Rogue GM9446")
        {
            // These devices (like the Oppo Reno 3 Pro) only give correct
            // results when rendering to an off-screen texture.
            m_alwaysUseOffscreenTexture = true;
        }
    }

    void destroyRenderContext()
    {
        if (m_device != nullptr)
        {
            m_device->waitUntilIdle();

            m_swapchain = nullptr;
            m_renderTarget = nullptr;
            m_overflowTexture = nullptr;
            m_renderContext = nullptr;
            m_device = nullptr;
        }
    }

    RenderContextVulkanImpl* impl() const
    {
        return m_renderContext->static_impl_cast<RenderContextVulkanImpl>();
    }

    VulkanContext* vk() const { return impl()->vulkanContext(); }

    ANativeWindow* m_window = nullptr;
    BackendParams m_backendParams;
    uint32_t m_androidWindowWidth;
    uint32_t m_androidWindowHeight;
    std::unique_ptr<rive_vkb::VulkanInstance> m_instance;
    std::unique_ptr<rive_vkb::VulkanDevice> m_device;
    VkSurfaceKHR m_windowSurface = VK_NULL_HANDLE;
    std::unique_ptr<rive_vkb::VulkanSwapchain> m_swapchain;
    std::unique_ptr<RenderContext> m_renderContext;
    rcp<RenderTargetVulkanImpl> m_renderTarget;
    rcp<vkutil::Texture2D> m_overflowTexture; // Used when the desired render
                                              // size doesn't fit in the window.
    PFN_vkDestroySurfaceKHR m_vkDestroySurfaceKHR = nullptr;

    bool m_printDeviceInitializationMessage = true;
    bool m_allowBlitOffscreenToScreen = true;
    bool m_alwaysUseOffscreenTexture = false;
};

TestingWindow* TestingWindow::MakeAndroidVulkan(
    const BackendParams& backendParams,
    void* platformWindow)
{
    return new TestingWindowAndroidVulkan(
        backendParams,
        reinterpret_cast<ANativeWindow*>(platformWindow));
}

#endif
