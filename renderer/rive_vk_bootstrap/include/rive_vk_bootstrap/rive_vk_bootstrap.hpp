/*
 * Copyright 2024 Rive
 */

#include <VkBootstrap.h>
#include "rive/renderer/vulkan/vulkan_context.hpp"

namespace rive_vkb
{
template <typename T>
T vkb_check(vkb::Result<T> result, const char* code, int line, const char* file)
{
    if (!result)
    {
        fprintf(stderr,
                "%s:%u: Error: %s: %s\n",
                file,
                line,
                code,
                result.error().message().c_str());
        abort();
    }
    return *result;
}

#define VKB_CHECK(RESULT)                                                      \
    ::rive_vkb::vkb_check(RESULT, #RESULT, __LINE__, __FILE__)

vkb::SystemInfo load_vulkan();

#ifdef DEBUG
VKAPI_ATTR VkBool32 VKAPI_CALL
default_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT,
                       VkDebugUtilsMessageTypeFlagsEXT,
                       const VkDebugUtilsMessengerCallbackDataEXT*,
                       void* pUserData);
#endif

enum class FeatureSet
{
    coreOnly,
    allAvailable,
};

// Select a GPU, using 'gpuNameFilter' or 'getenv("RIVE_GPU")', otherwise
// preferring discrete. Abort if the filter matches more than one name.
std::tuple<vkb::PhysicalDevice, rive::gpu::VulkanFeatures>
select_physical_device(vkb::PhysicalDeviceSelector& selector,
                       FeatureSet,
                       const char* gpuNameFilter = nullptr);

inline std::tuple<vkb::PhysicalDevice, rive::gpu::VulkanFeatures>
select_physical_device(vkb::Instance instance,
                       FeatureSet featureSet,
                       const char* gpuNameFilter = nullptr)
{
    vkb::PhysicalDeviceSelector selector(instance);
    return select_physical_device(selector, featureSet, gpuNameFilter);
}
} // namespace rive_vkb
