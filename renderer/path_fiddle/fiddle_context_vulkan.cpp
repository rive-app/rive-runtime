/*
 * Copyright 2024 Rive
 */

#include "fiddle_context.hpp"

#if !defined(RIVE_VULKAN) || defined(RIVE_TOOLS_NO_GLFW)

std::unique_ptr<FiddleContext> FiddleContext::MakeVulkanPLS(
    FiddleContextOptions options)
{
    return nullptr;
}

#else

#include "rive_vk_bootstrap/vulkan_device.hpp"
#include "rive_vk_bootstrap/vulkan_instance.hpp"
#include "rive_vk_bootstrap/vulkan_swapchain.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "rive/renderer/vulkan/render_context_vulkan_impl.hpp"
#include "rive/renderer/vulkan/render_target_vulkan.hpp"
#include "shader_hotload.hpp"
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <vulkan/vulkan.h>

using namespace rive;
using namespace rive::gpu;

class FiddleContextVulkanPLS : public FiddleContext
{
public:
    FiddleContextVulkanPLS(FiddleContextOptions options) : m_options(options)
    {
        using namespace rive_vkb;

        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        m_instance = std::make_unique<VulkanInstance>(VulkanInstance::Options{
            .appName = "path_fiddle",
            .idealAPIVersion = options.coreFeaturesOnly ? VK_API_VERSION_1_0
                                                        : VK_API_VERSION_1_3,
            .requiredExtensions = make_span(glfwExtensions, glfwExtensionCount),
#ifndef NDEBUG
            .desiredValidationType =
                options.enableVulkanCoreValidationLayers
                    ? VulkanValidationType::core
                    : (options.enableVulkanSynchronizationValidationLayers
                           ? VulkanValidationType::synchronization
                           : VulkanValidationType::none),
            .wantDebugCallbacks = !options.disableDebugCallbacks,
#endif
        });

        m_vkDestroySurfaceKHR =
            m_instance->loadInstanceFunc<PFN_vkDestroySurfaceKHR>(
                "vkDestroySurfaceKHR");
        assert(m_vkDestroySurfaceKHR != nullptr);

        m_device = std::make_unique<VulkanDevice>(
            *m_instance,
            VulkanDevice::Options{
                .coreFeaturesOnly = options.coreFeaturesOnly,
                .gpuNameFilter = options.gpuNameFilter,
            });

        m_renderContext = RenderContextVulkanImpl::MakeContext(
            m_instance->vkInstance(),
            m_device->vkPhysicalDevice(),
            m_device->vkDevice(),
            m_device->vulkanFeatures(),
            m_instance->getVkGetInstanceProcAddrPtr(),
            {
                .forceAtomicMode = options.disableRasterOrdering,
                .shaderCompilationMode = m_options.shaderCompilationMode,
            });
    }

    ~FiddleContextVulkanPLS()
    {
        // Destroy the swapchain first because it synchronizes for in-flight
        // command buffers.
        m_swapchain = nullptr;

        m_renderContext.reset();
        m_renderTarget.reset();

        if (m_windowSurface != VK_NULL_HANDLE)
        {
            m_vkDestroySurfaceKHR(m_instance->vkInstance(),
                                  m_windowSurface,
                                  nullptr);
        }
    }

    float dpiScale(GLFWwindow* window) const final
    {
#ifdef __APPLE__
        return 2;
#else
        return 1;
#endif
    }

    Factory* factory() final { return m_renderContext.get(); }

    rive::gpu::RenderContext* renderContextOrNull() final
    {
        return m_renderContext.get();
    }

    rive::gpu::RenderContextVulkanImpl* renderContextVulkanImpl() const final
    {
        return m_renderContext->static_impl_cast<RenderContextVulkanImpl>();
    }

    rive::gpu::RenderTarget* renderTargetOrNull() final
    {
        return m_renderTarget.get();
    }

    void onSizeChanged(GLFWwindow* window,
                       int width,
                       int height,
                       uint32_t sampleCount) final
    {
        uint64_t currentFrameNumber = 0;
        if (m_swapchain != nullptr)
        {
            currentFrameNumber = m_swapchain->currentFrameNumber();
            m_swapchain = nullptr;
        }

        if (m_windowSurface != VK_NULL_HANDLE)
        {
            m_vkDestroySurfaceKHR(m_instance->vkInstance(),
                                  m_windowSurface,
                                  nullptr);
        }

        VK_CHECK(glfwCreateWindowSurface(m_instance->vkInstance(),
                                         window,
                                         nullptr,
                                         &m_windowSurface));

        auto vkGetPhysicalDeviceSurfaceCapabilitiesKHR =
            m_instance->loadInstanceFunc<
                PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(
                "vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
        VkSurfaceCapabilitiesKHR windowCapabilities{};
        VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            m_device->vkPhysicalDevice(),
            m_windowSurface,
            &windowCapabilities));

        auto swapOpts = rive_vkb::VulkanSwapchain::Options{
            .formatPreferences =
                {
                    {
                        .format = m_options.srgb ? VK_FORMAT_R8G8B8A8_SRGB
                                  : m_options.coreFeaturesOnly
                                      ? VK_FORMAT_B8G8R8A8_UNORM
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
                    VK_PRESENT_MODE_MAILBOX_KHR,
                    VK_PRESENT_MODE_FIFO_RELAXED_KHR,
                    VK_PRESENT_MODE_FIFO_KHR,
                },
            .imageUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                               VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                               VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            .initialFrameNumber = currentFrameNumber,
        };

        if (!m_options.coreFeaturesOnly &&
            (windowCapabilities.supportedUsageFlags &
             VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT))
        {
            swapOpts.imageUsageFlags |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
        }

        if (!m_options.coreFeaturesOnly &&
            (windowCapabilities.supportedUsageFlags &
             VK_IMAGE_USAGE_STORAGE_BIT))
        {
            swapOpts.imageUsageFlags |= VK_IMAGE_USAGE_STORAGE_BIT;
        }

        m_swapchain =
            std::make_unique<rive_vkb::VulkanSwapchain>(*m_instance,
                                                        *m_device,
                                                        ref_rcp(vk()),
                                                        m_windowSurface,
                                                        swapOpts);

        m_renderTarget = renderContextVulkanImpl()->makeRenderTarget(
            width,
            height,
            m_swapchain->imageFormat(),
            m_swapchain->imageUsageFlags());
    }

    void toggleZoomWindow() final {}

    void hotloadShaders() final
    {
        m_device->waitUntilIdle();
        rive::Span<const uint32_t> newShaderBytecodeData =
            loadNewShaderFileData();
        if (newShaderBytecodeData.size() > 0)
        {
            renderContextVulkanImpl()->hotloadShaders(newShaderBytecodeData);
        }
    }

    std::unique_ptr<Renderer> makeRenderer(int width, int height) final
    {
        return std::make_unique<RiveRenderer>(m_renderContext.get());
    }

    void begin(const RenderContext::FrameDescriptor& frameDescriptor) final
    {
        m_renderContext->beginFrame(frameDescriptor);
    }

    void flushPLSContext(RenderTarget* offscreenRenderTarget) final
    {
        if (!m_swapchain->isFrameStarted())
        {
            m_swapchain->beginFrame();

            m_renderTarget->setTargetImageView(
                m_swapchain->currentVkImageView(),
                m_swapchain->currentVkImage(),
                m_swapchain->currentLastAccess());
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

    void end(GLFWwindow*, std::vector<uint8_t>* pixelData) final
    {
        flushPLSContext(nullptr);

        auto lastAccess = m_renderTarget->targetLastAccess();
        if (pixelData != nullptr)
        {
            m_swapchain->queueImageCopy(
                &lastAccess,
                rive::IAABB::MakeWH(m_renderTarget->width(),
                                    m_renderTarget->height()));
        }
        m_swapchain->endFrame(lastAccess);

        if (pixelData != nullptr)
        {
            m_swapchain->getPixelsFromLastImageCopy(pixelData);
        }
    }

private:
    VulkanContext* vk() const
    {
        return renderContextVulkanImpl()->vulkanContext();
    }

    const FiddleContextOptions m_options;
    std::unique_ptr<rive_vkb::VulkanInstance> m_instance;
    std::unique_ptr<rive_vkb::VulkanDevice> m_device;
    std::unique_ptr<rive_vkb::VulkanSwapchain> m_swapchain;

    VkSurfaceKHR m_windowSurface = VK_NULL_HANDLE;

    std::unique_ptr<RenderContext> m_renderContext;
    rcp<RenderTargetVulkanImpl> m_renderTarget;

    PFN_vkDestroySurfaceKHR m_vkDestroySurfaceKHR = nullptr;
};

std::unique_ptr<FiddleContext> FiddleContext::MakeVulkanPLS(
    FiddleContextOptions options)
{
    return std::make_unique<FiddleContextVulkanPLS>(options);
}

#endif
