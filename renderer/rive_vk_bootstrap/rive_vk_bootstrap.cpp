/*
 * Copyright 2024 Rive
 */

#include "rive_vk_bootstrap/rive_vk_bootstrap.hpp"

#ifdef __APPLE__
#include <dlfcn.h>
#endif

namespace rive_vkb
{
vkb::SystemInfo load_vulkan()
{
    PFN_vkGetInstanceProcAddr fp_vkGetInstanceProcAddr = nullptr;
#ifdef __APPLE__
    // The Vulkan SDK on Mac gets installed to /usr/local/lib, which is no
    // longer on the library search path after Sonoma.
    if (void* vulkanLib =
            dlopen("/usr/local/lib/libvulkan.1.dylib", RTLD_NOW | RTLD_LOCAL))
    {
        fp_vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
            dlsym(vulkanLib, "vkGetInstanceProcAddr"));
    }
#endif
    return VKB_CHECK(
        vkb::SystemInfo::get_system_info(fp_vkGetInstanceProcAddr));
}

#ifdef DEBUG
static std::array<const char*, 2> s_ignoredValidationMsgList = {
    // Swiftshader generates this error during
    // vkEnumeratePhysicalDevices. It seems fine to ignore.
    "Copying old device 0 into new device 0",
    // Cirrus Ubuntu runner w/ Nvidia gpu reports this but it seems harmless.
    "terminator_CreateInstance: Received return code -3 from call to "
    "vkCreateInstance in ICD /usr/lib/x86_64-linux-gnu/libvulkan_virtio.so. "
    "Skipping this driver.",
};

VKAPI_ATTR VkBool32 VKAPI_CALL default_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    bool shouldAbort = true;

    switch (messageType)
    {
        case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
            for (const char* msg : s_ignoredValidationMsgList)
            {
                if (strcmp(pCallbackData->pMessage, msg) == 0)
                {
                    shouldAbort = false;
                    break;
                }
            }

            fprintf(stderr, "Rive Vulkan error");
            if (!shouldAbort)
            {
                fprintf(stderr, " (error ignored)");
            }
            fprintf(stderr,
                    ": %i: %s: %s\n",
                    pCallbackData->messageIdNumber,
                    pCallbackData->pMessageIdName,
                    pCallbackData->pMessage);

            if (shouldAbort)
            {
                abort();
            }
            break;

        case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
            fprintf(stderr,
                    "Rive Vulkan Validation error: %i: %s: %s\n",
                    pCallbackData->messageIdNumber,
                    pCallbackData->pMessageIdName,
                    pCallbackData->pMessage);
            abort();
        case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
            fprintf(stderr,
                    "Rive Vulkan Performance warning: %i: %s: %s\n",
                    pCallbackData->messageIdNumber,
                    pCallbackData->pMessageIdName,
                    pCallbackData->pMessage);
            break;
    }
    return VK_TRUE;
}
#endif

static const char* physical_device_type_name(VkPhysicalDeviceType type)
{
    switch (type)
    {
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
            return "Integrated";
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
            return "Discrete";
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
            return "Virtual";
        case VK_PHYSICAL_DEVICE_TYPE_CPU:
            return "CPU";
        default:
            return "Other";
    }
}

// Select a GPU name if it contains the substring 'filter' or '$RIVE_GPU'.
// Return false if 'filter' and '$RIVE_GPU' are both null.
// Abort if the filter matches more than one name.
std::tuple<vkb::Device, rive::gpu::VulkanFeatures> select_device(
    vkb::PhysicalDeviceSelector& selector,
    FeatureSet featureSet,
    const char* gpuNameFilter)
{
    if (const char* rive_gpu = getenv("RIVE_GPU"))
    {
        gpuNameFilter = rive_gpu;
    }
    if (gpuNameFilter == nullptr)
    {
        // No active filter. Go with a discrete GPU.
        selector.allow_any_gpu_device_type(false).prefer_gpu_device_type(
            vkb::PreferredDeviceType::discrete);
    }
    else
    {
        std::vector<std::string> names =
            VKB_CHECK(selector.select_device_names());
        std::vector<std::string> matches;
        for (const std::string& name : names)
        {
            if (strstr(name.c_str(), gpuNameFilter) != nullptr)
            {
                matches.push_back(name);
            }
        }
        if (matches.size() != 1)
        {
            const char* filterName =
                gpuNameFilter != nullptr ? gpuNameFilter : "<discrete_gpu>";
            const std::vector<std::string>* devicePrintList;
            if (matches.size() > 1)
            {
                fprintf(stderr,
                        "Cannot select GPU\nToo many matches for filter "
                        "'%s'.\nMatches:\n",
                        filterName);
                devicePrintList = &matches;
            }
            else
            {
                fprintf(stderr,
                        "Cannot select GPU.\nNo matches for filter "
                        "'%s'.\nAvailable GPUs:\n",
                        filterName);
                devicePrintList = &names;
            }
            for (const std::string& name : *devicePrintList)
            {
                fprintf(stderr, "  %s\n", name.c_str());
            }
            fprintf(stderr,
                    "\nPlease update the $RIVE_GPU environment variable\n");
            abort();
        }
        selector.set_name(matches[0]);
    }
    auto selectResult =
        selector.set_minimum_version(1, 0)
            .allow_any_gpu_device_type(false)
            .select(vkb::DeviceSelectionMode::only_fully_suitable);
    if (!selectResult)
    {
        selectResult = selector.allow_any_gpu_device_type(true).select(
            vkb::DeviceSelectionMode::partially_and_fully_suitable);
    }
    auto physicalDevice = VKB_CHECK(selectResult);

    physicalDevice.enable_features_if_present({
        .independentBlend = featureSet != FeatureSet::coreOnly,
        .fillModeNonSolid = VK_TRUE,
        .fragmentStoresAndAtomics = VK_TRUE,
    });

    rive::gpu::VulkanFeatures riveVulkanFeatures = {
        .independentBlend =
            static_cast<bool>(physicalDevice.features.independentBlend),
        .fillModeNonSolid =
            static_cast<bool>(physicalDevice.features.fillModeNonSolid),
        .fragmentStoresAndAtomics =
            static_cast<bool>(physicalDevice.features.fragmentStoresAndAtomics),
    };

    if (featureSet != FeatureSet::coreOnly &&
        (physicalDevice.enable_extension_if_present(
             VK_EXT_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_EXTENSION_NAME) ||
         physicalDevice.enable_extension_if_present(
             "VK_AMD_rasterization_order_attachment_access")))
    {
        constexpr static VkPhysicalDeviceRasterizationOrderAttachmentAccessFeaturesEXT
            rasterOrderFeatures = {
                .sType =
                    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_FEATURES_EXT,
                .rasterizationOrderColorAttachmentAccess = VK_TRUE,
            };
        if (physicalDevice.enable_extension_features_if_present(
                rasterOrderFeatures))
        {
            riveVulkanFeatures.rasterizationOrderColorAttachmentAccess = true;
        }
    }

    vkb::Device device = VKB_CHECK(vkb::DeviceBuilder(physicalDevice).build());
    riveVulkanFeatures.apiVersion = device.instance_version,

    printf("==== Vulkan %i.%i.%i GPU (%s): %s [ ",
           VK_API_VERSION_MAJOR(riveVulkanFeatures.apiVersion),
           VK_API_VERSION_MINOR(riveVulkanFeatures.apiVersion),
           VK_API_VERSION_PATCH(riveVulkanFeatures.apiVersion),
           physical_device_type_name(physicalDevice.properties.deviceType),
           physicalDevice.properties.deviceName);
    struct CommaSeparator
    {
        const char* m_separator = "";
        const char* operator*() { return std::exchange(m_separator, ", "); }
    } commaSeparator;
    if (riveVulkanFeatures.independentBlend)
        printf("%sindependentBlend", *commaSeparator);
    if (riveVulkanFeatures.fillModeNonSolid)
        printf("%sfillModeNonSolid", *commaSeparator);
    if (riveVulkanFeatures.fragmentStoresAndAtomics)
        printf("%sfragmentStoresAndAtomics", *commaSeparator);
    if (riveVulkanFeatures.rasterizationOrderColorAttachmentAccess)
        printf("%srasterizationOrderColorAttachmentAccess", *commaSeparator);
#if 0
    printf("Extensions:\n");
    for (const auto& ext : physicalDevice.get_available_extensions())
    {
        printf("  %s\n", ext.c_str());
    }
#endif
    printf(" ] ====\n");

    return {device, riveVulkanFeatures};
}

// Vulkan native swapchain.
Swapchain::Swapchain(const vkb::Device& device,
                     rive::rcp<rive::gpu::VulkanContext> vk,
                     uint32_t width,
                     uint32_t height,
                     vkb::Swapchain&& vkbSwapchain,
                     uint64_t currentFrameNumber) :
    m_dispatchTable(device.make_table()),
    m_queue(VKB_CHECK(device.get_queue(vkb::QueueType::graphics))),
    m_vk(vk),
    m_width(width),
    m_height(height),
    m_imageFormat(vkbSwapchain.image_format),
    m_imageUsageFlags(vkbSwapchain.image_usage_flags),
    m_vkbSwapchain(std::move(vkbSwapchain)),
    m_currentFrameNumber(currentFrameNumber)
{
    assert(m_vkbSwapchain.swapchain != nullptr);
    init(device, *m_vkbSwapchain.get_images());
}

// Offscreen texture.
Swapchain::Swapchain(const vkb::Device& device,
                     rive::rcp<rive::gpu::VulkanContext> vk,
                     uint32_t width,
                     uint32_t height,
                     VkFormat imageFormat,
                     VkImageUsageFlags additionalUsageFlags,
                     uint64_t currentFrameNumber) :
    m_dispatchTable(device.make_table()),
    m_queue(VKB_CHECK(device.get_queue(vkb::QueueType::graphics))),
    m_vk(vk),
    m_width(width),
    m_height(height),
    m_imageFormat(imageFormat),
    m_imageUsageFlags(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | additionalUsageFlags),
    m_offscreenTexture(m_vk->makeTexture({
        .format = m_imageFormat,
        .extent = {m_width, m_height, 1},
        .usage = m_imageUsageFlags,
    })),
    m_currentFrameNumber(currentFrameNumber)
{
    init(device, {*m_offscreenTexture});

    // Signal the frame completion semaphore so we can blindly wait for it on
    // the first frame, just like all the other frames.
    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &m_swapchainImages[0].frameCompleteSemaphore,
    };
    VK_CHECK(
        m_dispatchTable.queueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE));
}

void Swapchain::init(const vkb::Device& device,
                     const std::vector<VkImage>& images)
{
    // In order to implement blend modes, the target texture needs to either
    // support input attachment usage (ideal), or else transfers.
    constexpr static VkImageUsageFlags transferSrcAndDst =
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    assert((m_imageUsageFlags & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT) ||
           (m_imageUsageFlags & transferSrcAndDst) == transferSrcAndDst);

    constexpr static VkSemaphoreCreateInfo semaphoreCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    constexpr static VkFenceCreateInfo fenceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    VkCommandPoolCreateInfo commandPoolCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VkCommandPoolCreateFlagBits::
            VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = *device.get_queue_index(vkb::QueueType::graphics),
    };

    VK_CHECK(m_dispatchTable.createCommandPool(&commandPoolCreateInfo,
                                               nullptr,
                                               &m_commandPool));

    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = m_commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    VK_CHECK(m_dispatchTable.createSemaphore(&semaphoreCreateInfo,
                                             nullptr,
                                             &m_nextAcquireSemaphore));

    m_swapchainImages.resize(images.size());
    for (uint32_t i = 0; i < images.size(); ++i)
    {
        SwapchainImage& swapchainImage = m_swapchainImages[i];
        swapchainImage.externalImage = images[i];

        swapchainImage.imageView = m_vk->makeExternalTextureView(
            m_imageUsageFlags,
            {
                .image = swapchainImage.externalImage,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = m_imageFormat,
                .subresourceRange =
                    {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .levelCount = 1,
                        .layerCount = 1,
                    },
            });

        VK_CHECK(m_dispatchTable.createFence(&fenceCreateInfo,
                                             nullptr,
                                             &swapchainImage.fence));

        VK_CHECK(m_dispatchTable.createSemaphore(
            &semaphoreCreateInfo,
            nullptr,
            &swapchainImage.frameBeginSemaphore));

        VK_CHECK(m_dispatchTable.createSemaphore(
            &semaphoreCreateInfo,
            nullptr,
            &swapchainImage.frameCompleteSemaphore));

        VK_CHECK(m_dispatchTable.allocateCommandBuffers(
            &commandBufferAllocateInfo,
            &swapchainImage.commandBuffer));

        swapchainImage.currentFrameNumber = swapchainImage.safeFrameNumber =
            m_currentFrameNumber;
    }
}

Swapchain::~Swapchain()
{
    m_dispatchTable.queueWaitIdle(m_queue);
    for (SwapchainImage& swapchainImage : m_swapchainImages)
    {
        swapchainImage.imageView = nullptr;
        m_dispatchTable.destroyFence(swapchainImage.fence, nullptr);
        m_dispatchTable.destroySemaphore(swapchainImage.frameBeginSemaphore,
                                         nullptr);
        m_dispatchTable.destroySemaphore(swapchainImage.frameCompleteSemaphore,
                                         nullptr);
        m_dispatchTable.freeCommandBuffers(m_commandPool,
                                           1,
                                           &swapchainImage.commandBuffer);
    }
    m_dispatchTable.destroySemaphore(m_nextAcquireSemaphore, nullptr);
    m_dispatchTable.destroyCommandPool(m_commandPool, nullptr);
    vkb::destroy_swapchain(m_vkbSwapchain);
}

static void wait_fence(const vkb::DispatchTable& DispatchTable, VkFence fence)
{
    while (DispatchTable.waitForFences(1, &fence, VK_TRUE, 1000) == VK_TIMEOUT)
    {
        // Keep waiting.
    }
}

const SwapchainImage* Swapchain::acquireNextImage()
{
    SwapchainImage* swapchainImage;
    if (m_vkbSwapchain.swapchain != nullptr)
    {
        m_dispatchTable.acquireNextImageKHR(m_vkbSwapchain,
                                            UINT64_MAX,
                                            m_nextAcquireSemaphore,
                                            VK_NULL_HANDLE,
                                            &m_currentImageIndex);
        swapchainImage = &m_swapchainImages[m_currentImageIndex];
        std::swap(swapchainImage->frameBeginSemaphore, m_nextAcquireSemaphore);
    }
    else
    {
        m_currentImageIndex = 0;
        swapchainImage = &m_swapchainImages[0];
        std::swap(swapchainImage->frameBeginSemaphore,
                  swapchainImage->frameCompleteSemaphore);
    }

    wait_fence(m_dispatchTable, swapchainImage->fence);
    m_dispatchTable.resetFences(1, &swapchainImage->fence);

    m_dispatchTable.resetCommandBuffer(swapchainImage->commandBuffer, {});
    VkCommandBufferBeginInfo commandBufferBeginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    };
    m_dispatchTable.beginCommandBuffer(swapchainImage->commandBuffer,
                                       &commandBufferBeginInfo);

    // Now that we've waited for the fence, resources from 'currentFrameNumber'
    // are safe to be released or recycled.
    swapchainImage->safeFrameNumber = swapchainImage->currentFrameNumber;
    swapchainImage->currentFrameNumber = ++m_currentFrameNumber;
    return swapchainImage;
}

rive::gpu::VulkanContext::TextureAccess Swapchain::submit(
    rive::gpu::VulkanContext::TextureAccess lastAccess,
    std::vector<uint8_t>* pixelData)
{
    const SwapchainImage* swapchainImage =
        &m_swapchainImages[m_currentImageIndex];

    if (pixelData != nullptr)
    {
        // Copy the framebuffer out to a buffer.
        if (m_pixelReadBuffer == nullptr)
        {
            m_pixelReadBuffer = m_vk->makeBuffer(
                {
                    .size = m_height * m_width * 4,
                    .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                },
                rive::gpu::vkutil::Mappability::readWrite);
        }
        assert(m_pixelReadBuffer->info().size == m_height * m_width * 4);

        lastAccess = m_vk->simpleImageMemoryBarrier(
            swapchainImage->commandBuffer,
            lastAccess,
            {
                .pipelineStages = VK_PIPELINE_STAGE_TRANSFER_BIT,
                .accessMask = VK_ACCESS_TRANSFER_READ_BIT,
                .layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            },
            swapchainImage->externalImage);

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

        m_dispatchTable.cmdCopyImageToBuffer(
            swapchainImage->commandBuffer,
            swapchainImage->externalImage,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            *m_pixelReadBuffer,
            1,
            &imageCopyDesc);

        m_vk->bufferMemoryBarrier(
            swapchainImage->commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_HOST_BIT,
            0,
            {
                .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .dstAccessMask = VK_ACCESS_HOST_READ_BIT,
                .buffer = *m_pixelReadBuffer,
            });
    }

    if (m_vkbSwapchain.swapchain != nullptr)
    {
        lastAccess = m_vk->simpleImageMemoryBarrier(
            swapchainImage->commandBuffer,
            lastAccess,
            {
                .pipelineStages = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                .accessMask = VK_ACCESS_NONE,
                .layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            },
            swapchainImage->externalImage);
    }

    VK_CHECK(m_dispatchTable.endCommandBuffer(swapchainImage->commandBuffer));

    VkPipelineStageFlags waitDstStageMask =
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &swapchainImage->frameBeginSemaphore,
        .pWaitDstStageMask = &waitDstStageMask,
        .commandBufferCount = 1,
        .pCommandBuffers = &swapchainImage->commandBuffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &swapchainImage->frameCompleteSemaphore,
    };

    VK_CHECK(m_dispatchTable.queueSubmit(m_queue,
                                         1,
                                         &submitInfo,
                                         swapchainImage->fence));

    if (pixelData != nullptr)
    {
        // Wait for all rendering to complete before transferring the
        // framebuffer data to pixelData.
        wait_fence(m_dispatchTable, swapchainImage->fence);
        m_pixelReadBuffer->invalidateContents();

        // Copy the buffer containing the framebuffer contents to pixelData.
        pixelData->resize(m_height * m_width * 4);

        assert(m_pixelReadBuffer->info().size == m_height * m_width * 4);
        for (uint32_t y = 0; y < m_height; ++y)
        {
            auto src =
                static_cast<const uint8_t*>(m_pixelReadBuffer->contents()) +
                m_width * 4 * y;
            uint8_t* dst = pixelData->data() + (m_height - y - 1) * m_width * 4;
            memcpy(dst, src, m_width * 4);
            if (m_imageFormat == VK_FORMAT_B8G8R8A8_UNORM)
            {
                // Reverse bgr -> rgb.
                for (uint32_t x = 0; x < m_width * 4; x += 4)
                {
                    std::swap(dst[x], dst[x + 2]);
                }
            }
        }
    }

    if (m_vkbSwapchain.swapchain != nullptr)
    {
        VkPresentInfoKHR presentInfo = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &swapchainImage->frameCompleteSemaphore,
            .swapchainCount = 1,
            .pSwapchains = &m_vkbSwapchain.swapchain,
            .pImageIndices = &m_currentImageIndex,
        };

        m_dispatchTable.queuePresentKHR(m_queue, &presentInfo);
    }

    m_currentImageIndex = INVALID_IMAGE_INDEX;
    return lastAccess;
}
} // namespace rive_vkb
