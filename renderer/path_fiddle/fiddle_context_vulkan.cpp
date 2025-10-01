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

#include "rive_vk_bootstrap/rive_vk_bootstrap.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "rive/renderer/vulkan/render_context_vulkan_impl.hpp"
#include "rive/renderer/vulkan/render_target_vulkan.hpp"
#include "shader_hotload.hpp"
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include <vk_mem_alloc.h>

using namespace rive;
using namespace rive::gpu;

class FiddleContextVulkanPLS : public FiddleContext
{
public:
    FiddleContextVulkanPLS(FiddleContextOptions options) : m_options(options)
    {
        rive_vkb::load_vulkan();

        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        int minorVersionRequested = options.coreFeaturesOnly ? 0 : 3;
        bool enableVulkanValidationLayers =
            m_options.enableVulkanValidationLayers;
        bool disableDebugCallbacks = m_options.disableDebugCallbacks;

        while (true)
        {
            vkb::InstanceBuilder instanceBuilder;
            instanceBuilder.set_app_name("path_fiddle")
                .set_engine_name("Rive Renderer")
                .enable_extensions(glfwExtensionCount, glfwExtensions)
                .require_api_version(1, minorVersionRequested, 0)
                .set_minimum_instance_version(1, 0, 0);
#ifdef DEBUG
            instanceBuilder.enable_validation_layers(
                enableVulkanValidationLayers);
            if (!disableDebugCallbacks)
            {
                instanceBuilder.set_debug_callback(
                    rive_vkb::default_debug_callback);
            }
#endif

            auto instanceResult = instanceBuilder.build();
            if (!instanceResult)
            {
                auto error = static_cast<vkb::InstanceError>(
                    instanceResult.error().value());

                if (error ==
                        vkb::InstanceError::vulkan_version_1_1_unavailable &&
                    minorVersionRequested != 0)
                {
                    // There's a bug in VkBootstrap (due to not properly
                    // handling Vulkan 1.0 not having the
                    // vkEnumerateInstanceVersion function) where it can give a
                    // vulkan_version_1_1_unavailable error even though we've
                    // specified a minimum of 1.0. If we get that error,
                    // request 1.0 directly and try again.
                    fprintf(stderr, "Falling back on Vulkan 1.0.\n");
                    minorVersionRequested = 0;
                    continue;
                }

#ifdef DEBUG
                if (enableVulkanValidationLayers &&
                    error == vkb::InstanceError::requested_layers_not_present)
                {
                    fprintf(stderr,
                            "WARNING: Validation layers not found. Attempting "
                            "to create a Vulkan context again without "
                            "validation layers.\n");
                    enableVulkanValidationLayers = false;
                    continue;
                }

                if (!disableDebugCallbacks &&
                    error == vkb::InstanceError::failed_create_debug_messenger)
                {
                    fprintf(stderr,
                            "WARNING: Debug callbacks not supported. "
                            "Attempting to create a Vulkan context again "
                            "without debug callbacks.");
                    disableDebugCallbacks = true;
                    continue;
                }
#endif
                fprintf(stderr,
                        "ERROR: %s: Failed to build Vulkan instance.",
                        instanceResult.error().message().c_str());
                abort();
            }

            m_instance = *instanceResult;
            break;
        }
        m_instanceDispatchTable = m_instance.make_table();

        VulkanFeatures vulkanFeatures;
        std::tie(m_device, vulkanFeatures) = rive_vkb::select_device(
            vkb::PhysicalDeviceSelector(m_instance)
                .defer_surface_initialization(),
            m_options.coreFeaturesOnly ? rive_vkb::FeatureSet::coreOnly
                                       : rive_vkb::FeatureSet::allAvailable,
            m_options.gpuNameFilter);
        m_renderContext = RenderContextVulkanImpl::MakeContext(
            m_instance,
            m_device.physical_device,
            m_device,
            vulkanFeatures,
            m_instance.fp_vkGetInstanceProcAddr,
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
            m_instanceDispatchTable.destroySurfaceKHR(m_windowSurface, nullptr);
        }

        vkb::destroy_device(m_device);
        vkb::destroy_instance(m_instance);
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
            m_instanceDispatchTable.destroySurfaceKHR(m_windowSurface, nullptr);
        }

        VK_CHECK(glfwCreateWindowSurface(m_instance,
                                         window,
                                         nullptr,
                                         &m_windowSurface));

        VkSurfaceCapabilitiesKHR windowCapabilities;
        VK_CHECK(m_instanceDispatchTable
                     .fp_vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
                         m_device.physical_device,
                         m_windowSurface,
                         &windowCapabilities));

        vkb::SwapchainBuilder swapchainBuilder(m_device, m_windowSurface);
        swapchainBuilder
            .set_desired_format({
                // Swap the target format in "vkcore" mode, just for fun so we
                // test both configurations.
                .format = m_options.srgb ? VK_FORMAT_R8G8B8A8_SRGB
                          : m_options.coreFeaturesOnly
                              ? VK_FORMAT_R8G8B8A8_UNORM
                              : VK_FORMAT_B8G8R8A8_UNORM,
                .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
            })
            .add_fallback_format({
                .format = m_options.coreFeaturesOnly ? VK_FORMAT_B8G8R8A8_UNORM
                                                     : VK_FORMAT_R8G8B8A8_UNORM,
                .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
            })
            .set_desired_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR)
            .add_fallback_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)
            .add_fallback_present_mode(VK_PRESENT_MODE_FIFO_RELAXED_KHR)
            .add_fallback_present_mode(VK_PRESENT_MODE_FIFO_KHR);
        if (!m_options.coreFeaturesOnly &&
            (windowCapabilities.supportedUsageFlags &
             VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT))
        {
            swapchainBuilder.add_image_usage_flags(
                VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);
            if (m_options.enableReadPixels)
            {
                swapchainBuilder.add_image_usage_flags(
                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
            }
        }
        else
        {
            swapchainBuilder
                .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_SRC_BIT)
                .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT);
        }
        m_swapchain = std::make_unique<rive_vkb::Swapchain>(
            m_device,
            ref_rcp(vk()),
            width,
            height,
            VKB_CHECK(swapchainBuilder.build()),
            currentFrameNumber);

        m_renderTarget = renderContextVulkanImpl()->makeRenderTarget(
            width,
            height,
            m_swapchain->imageFormat(),
            m_swapchain->imageUsageFlags());
    }

    void toggleZoomWindow() final {}

    void hotloadShaders() final
    {
        m_swapchain->dispatchTable().deviceWaitIdle();
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
        m_renderContext->beginFrame(std::move(frameDescriptor));
    }

    void flushPLSContext(RenderTarget* offscreenRenderTarget) final
    {
        const rive_vkb::SwapchainImage* swapchainImage =
            m_swapchain->currentImage();
        if (swapchainImage == nullptr)
        {
            swapchainImage = m_swapchain->acquireNextImage();
            m_renderTarget->setTargetImageView(swapchainImage->imageView,
                                               swapchainImage->image,
                                               swapchainImage->imageLastAccess);
        }

        m_renderContext->flush({
            .renderTarget = offscreenRenderTarget != nullptr
                                ? offscreenRenderTarget
                                : m_renderTarget.get(),
            .externalCommandBuffer = swapchainImage->commandBuffer,
            .currentFrameNumber = swapchainImage->currentFrameNumber,
            .safeFrameNumber = swapchainImage->safeFrameNumber,
        });
    }

    void end(GLFWwindow* window, std::vector<uint8_t>* pixelData) final
    {
        flushPLSContext(nullptr);
        m_swapchain->submit(m_renderTarget->targetLastAccess(), pixelData);
    }

private:
    VulkanContext* vk() const
    {
        return renderContextVulkanImpl()->vulkanContext();
    }

    const FiddleContextOptions m_options;
    vkb::Instance m_instance;
    vkb::InstanceDispatchTable m_instanceDispatchTable;
    vkb::Device m_device;

    VkSurfaceKHR m_windowSurface = VK_NULL_HANDLE;
    std::unique_ptr<rive_vkb::Swapchain> m_swapchain;

    std::unique_ptr<RenderContext> m_renderContext;
    rcp<RenderTargetVulkanImpl> m_renderTarget;
};

std::unique_ptr<FiddleContext> FiddleContext::MakeVulkanPLS(
    FiddleContextOptions options)
{
    return std::make_unique<FiddleContextVulkanPLS>(options);
}

#endif
