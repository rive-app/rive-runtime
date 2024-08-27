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
    // The Vulkan SDK on Mac gets installed to /usr/local/lib, which is no longer
    // on the library search path after Sonoma.
    if (void* vulkanLib = dlopen("/usr/local/lib/libvulkan.1.dylib", RTLD_NOW | RTLD_LOCAL))
    {
        fp_vkGetInstanceProcAddr =
            reinterpret_cast<PFN_vkGetInstanceProcAddr>(dlsym(vulkanLib, "vkGetInstanceProcAddr"));
    }
#endif
    return VKB_CHECK(vkb::SystemInfo::get_system_info(fp_vkGetInstanceProcAddr));
}

VKAPI_ATTR VkBool32 VKAPI_CALL
default_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                       VkDebugUtilsMessageTypeFlagsEXT messageType,
                       const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                       void* pUserData)
{
    switch (messageType)
    {
        case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
            if (pCallbackData->messageIdNumber == 0 &&
                strcmp(pCallbackData->pMessageIdName, "Loader Message") == 0 &&
                strcmp(pCallbackData->pMessage, "Copying old device 0 into new device 0") == 0)
            {
                // Swiftshader generates this error during
                // vkEnumeratePhysicalDevices. It seems fine to ignore.
                break;
            }
            fprintf(stderr,
                    "PLS Vulkan error: %i: %s: %s\n",
                    pCallbackData->messageIdNumber,
                    pCallbackData->pMessageIdName,
                    pCallbackData->pMessage);
            abort();
        case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
            fprintf(stderr,
                    "PLS Vulkan Validation error: %i: %s: %s\n",
                    pCallbackData->messageIdNumber,
                    pCallbackData->pMessageIdName,
                    pCallbackData->pMessage);
            abort();
        case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
            fprintf(stderr,
                    "PLS Vulkan Performance warning: %i: %s: %s\n",
                    pCallbackData->messageIdNumber,
                    pCallbackData->pMessageIdName,
                    pCallbackData->pMessage);
            break;
    }
    return VK_FALSE;
}

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
std::tuple<vkb::PhysicalDevice, rive::gpu::VulkanFeatures> select_physical_device(
    vkb::PhysicalDeviceSelector& selector,
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
        std::vector<std::string> names = VKB_CHECK(selector.select_device_names());
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
            const char* filterName = gpuNameFilter != nullptr ? gpuNameFilter : "<discrete_gpu>";
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
            fprintf(stderr, "\nPlease update the $RIVE_GPU environment variable\n");
            abort();
        }
        selector.set_name(matches[0]);
    }
    auto selectResult = selector.set_minimum_version(1, 0).allow_any_gpu_device_type(false).select(
        vkb::DeviceSelectionMode::only_fully_suitable);
    if (!selectResult)
    {
        selectResult = selector.allow_any_gpu_device_type(true).select(
            vkb::DeviceSelectionMode::partially_and_fully_suitable);
    }
    auto physicalDevice = VKB_CHECK(selectResult);

    printf("==== Vulkan GPU (%s): %s ====\n",
           rive_vkb::physical_device_type_name(physicalDevice.properties.deviceType),
           physicalDevice.properties.deviceName);

    rive::gpu::VulkanFeatures plsVulkanFeatures;
    physicalDevice.enable_features_if_present({
        .independentBlend = VK_TRUE,
        .fillModeNonSolid = VK_TRUE,
        .fragmentStoresAndAtomics = VK_TRUE,
    });
    printf("PLS Vulkan features: [");
    if (physicalDevice.features.independentBlend)
    {
        plsVulkanFeatures.independentBlend = true;
        printf(" independentBlend");
    }
    if (physicalDevice.features.fillModeNonSolid)
    {
        plsVulkanFeatures.fillModeNonSolid = true;
        printf(" fillModeNonSolid");
    }
    if (physicalDevice.features.fragmentStoresAndAtomics)
    {
        plsVulkanFeatures.fragmentStoresAndAtomics = true;
        printf(" fragmentStoresAndAtomics");
    }
    if (physicalDevice.enable_extension_if_present(
            VK_EXT_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_EXTENSION_NAME) ||
        physicalDevice.enable_extension_if_present("VK_AMD_rasterization_order_attachment_access"))
    {
        constexpr static VkPhysicalDeviceRasterizationOrderAttachmentAccessFeaturesEXT
            rasterOrderFeatures = {
                .sType =
                    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_FEATURES_EXT,
                .rasterizationOrderColorAttachmentAccess = VK_TRUE,
            };
        if (physicalDevice.enable_extension_features_if_present(rasterOrderFeatures))
        {
            plsVulkanFeatures.rasterizationOrderColorAttachmentAccess = true;
            printf(" rasterizationOrderColorAttachmentAccess");
        }
    }
    printf(" ]\n");

    return {physicalDevice, plsVulkanFeatures};
}
} // namespace rive_vkb
