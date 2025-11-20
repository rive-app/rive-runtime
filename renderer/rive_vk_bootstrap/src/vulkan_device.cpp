/*
 * Copyright 2025 Rive
 */

#include <string>
#include "rive/renderer/vulkan/vkutil.hpp"
#include "rive_vk_bootstrap/vulkan_device.hpp"
#include "rive_vk_bootstrap/vulkan_instance.hpp"
#include "logging.hpp"
#include "vulkan_library.hpp"

namespace rive_vkb
{
class VulkanInstance;

static const char* physicalDeviceTypeName(VkPhysicalDeviceType type)
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

VulkanDevice::VulkanDevice(VulkanInstance& instance, const Options& opts)
{
    assert(!opts.headless ||
           opts.presentationSurfaceForDeviceSelection == VK_NULL_HANDLE &&
               "It doesn't make sense to specify a presentation surface for a "
               "headless device");
    const char* nameFilter = opts.gpuNameFilter;
    if (const char* gpuFromEnv = getenv("RIVE_GPU"); gpuFromEnv != nullptr)
    {
        // Override the program's GPU filter with one from the environment if
        // it's set.
        nameFilter = gpuFromEnv;
    }

    auto findResult =
        findCompatiblePhysicalDevice(instance,
                                     nameFilter,
                                     opts.presentationSurfaceForDeviceSelection,
                                     opts.minimumSupportedAPIVersion);
    m_physicalDevice = findResult.physicalDevice;
    m_name = findResult.deviceName;

    DEFINE_AND_LOAD_INSTANCE_FUNC(vkGetPhysicalDeviceFeatures, instance);
    assert(vkGetPhysicalDeviceFeatures != nullptr);

    VkPhysicalDeviceFeatures supportedFeatures;
    vkGetPhysicalDeviceFeatures(m_physicalDevice, &supportedFeatures);

    VkPhysicalDeviceFeatures requestedFeatures = {
        .independentBlend =
            !opts.coreFeaturesOnly && supportedFeatures.independentBlend,
        // We use wireframe for debugging, so enable it even in core mode
        .fillModeNonSolid = supportedFeatures.fillModeNonSolid,

        // Always enable this
        .fragmentStoresAndAtomics = supportedFeatures.fragmentStoresAndAtomics,

        .shaderClipDistance =
            !opts.coreFeaturesOnly && supportedFeatures.shaderClipDistance,
    };

    m_riveVulkanFeatures = {
        .apiVersion = findResult.deviceAPIVersion,
        .independentBlend = bool(requestedFeatures.independentBlend),
        .fillModeNonSolid = bool(requestedFeatures.fillModeNonSolid),
        .fragmentStoresAndAtomics =
            bool(requestedFeatures.fragmentStoresAndAtomics),
        .shaderClipDistance = bool(requestedFeatures.shaderClipDistance),
    };

    DEFINE_AND_LOAD_INSTANCE_FUNC(vkEnumerateDeviceExtensionProperties,
                                  instance);
    assert(vkEnumerateDeviceExtensionProperties != nullptr);

    std::vector<VkExtensionProperties> supportedExtensions;
    {
        uint32_t count;
        vkEnumerateDeviceExtensionProperties(m_physicalDevice,
                                             nullptr,
                                             &count,
                                             nullptr);
        supportedExtensions.resize(count);
        vkEnumerateDeviceExtensionProperties(m_physicalDevice,
                                             nullptr,
                                             &count,
                                             supportedExtensions.data());
    }

    std::vector<const char*> addedExtensions;

    // This extension *must* be enabled if it's supported (it's usually on a
    // device that is not a fully-conforming device)
    if (addExtensionIfSupported("VK_KHR_portability_subset",
                                supportedExtensions,
                                addedExtensions))
    {
        m_riveVulkanFeatures.VK_KHR_portability_subset = true;
    }

    if (!opts.headless)
    {
        if (!addExtensionIfSupported(VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                                     supportedExtensions,
                                     addedExtensions))
        {
            LOG_ERROR_LINE("Cannot create device: %s is not supported.",
                           VK_KHR_SWAPCHAIN_EXTENSION_NAME);
            abort();
        }
    }

    // If these have values we'll use them in the VkDeviceCreateInfo chain
    std::optional<VkPhysicalDeviceRasterizationOrderAttachmentAccessFeaturesEXT>
        rasterOrderFeatures;
    std::optional<VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT>
        interlockFeatures;

    if (!opts.coreFeaturesOnly)
    {
        rasterOrderFeatures = tryEnableRasterOrderFeatures(instance,
                                                           supportedExtensions,
                                                           addedExtensions);
        interlockFeatures = tryEnableInterlockFeatures(instance,
                                                       supportedExtensions,
                                                       addedExtensions);
    }

    // Get our list of queue family properties.
    {
        DEFINE_AND_LOAD_INSTANCE_FUNC(vkGetPhysicalDeviceQueueFamilyProperties,
                                      instance);
        uint32_t count;
        vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice,
                                                 &count,
                                                 nullptr);
        m_queueFamilyProperties.resize(count);
        vkGetPhysicalDeviceQueueFamilyProperties(
            m_physicalDevice,
            &count,
            m_queueFamilyProperties.data());
    }

    // Now find the graphics queue in the list.
    m_graphicsQueueFamilyIndex = std::numeric_limits<uint32_t>::max();
    for (uint32_t i = 0; i < m_queueFamilyProperties.size(); i++)
    {
        if ((m_queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) !=
            0)
        {
            m_graphicsQueueFamilyIndex = i;
            break;
        }
    }

    assert(m_graphicsQueueFamilyIndex != std::numeric_limits<uint32_t>::max() &&
           "Could not find graphics queue index");

    // We're going to create a single queue (the graphics queue) with a priority
    // of 1.
    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = m_graphicsQueueFamilyIndex,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority,
    };

    // Finally create the actual device.
    VkDeviceCreateInfo deviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = nullptr,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCreateInfo,
        .enabledExtensionCount = uint32_t(addedExtensions.size()),
        .ppEnabledExtensionNames = addedExtensions.data(),
        .pEnabledFeatures = &requestedFeatures,
    };
    if (rasterOrderFeatures.has_value())
    {
        rasterOrderFeatures.value().pNext =
            const_cast<void*>(deviceCreateInfo.pNext);
        deviceCreateInfo.pNext = &rasterOrderFeatures.value();
    }
    if (interlockFeatures.has_value())
    {
        interlockFeatures.value().pNext =
            const_cast<void*>(deviceCreateInfo.pNext);
        deviceCreateInfo.pNext = &interlockFeatures.value();
    }

    DEFINE_AND_LOAD_INSTANCE_FUNC(vkCreateDevice, instance);
    VK_CHECK(vkCreateDevice(m_physicalDevice,
                            &deviceCreateInfo,
                            nullptr,
                            &m_device));

    LOAD_REQUIRED_MEMBER_INSTANCE_FUNC(vkGetDeviceProcAddr, instance);
    LOAD_REQUIRED_MEMBER_INSTANCE_FUNC(vkDeviceWaitIdle, instance);

    m_vkGetDeviceQueue = reinterpret_cast<PFN_vkGetDeviceQueue>(
        m_vkGetDeviceProcAddr(m_device, "vkGetDeviceQueue"));
    m_vkDestroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(
        m_vkGetDeviceProcAddr(m_device, "vkDestroyDevice"));

    LOAD_MEMBER_INSTANCE_FUNC(vkGetPhysicalDeviceSurfaceFormatsKHR, instance);
    LOAD_MEMBER_INSTANCE_FUNC(vkGetPhysicalDeviceSurfacePresentModesKHR,
                              instance);
    LOAD_MEMBER_INSTANCE_FUNC(vkGetPhysicalDeviceSurfaceCapabilitiesKHR,
                              instance);

    printf("==== Vulkan %i.%i.%i GPU (%s): %s [ ",
           VK_API_VERSION_MAJOR(m_riveVulkanFeatures.apiVersion),
           VK_API_VERSION_MINOR(m_riveVulkanFeatures.apiVersion),
           VK_API_VERSION_PATCH(m_riveVulkanFeatures.apiVersion),
           physicalDeviceTypeName(findResult.deviceType),
           findResult.deviceName.c_str());
    struct CommaSeparator
    {
        const char* m_separator = "";
        const char* operator*() { return std::exchange(m_separator, ", "); }
    } commaSeparator;
    if (m_riveVulkanFeatures.independentBlend)
        printf("%sindependentBlend", *commaSeparator);
    if (m_riveVulkanFeatures.fillModeNonSolid)
        printf("%sfillModeNonSolid", *commaSeparator);
    if (m_riveVulkanFeatures.fragmentStoresAndAtomics)
        printf("%sfragmentStoresAndAtomics", *commaSeparator);
    if (m_riveVulkanFeatures.shaderClipDistance)
        printf("%sshaderClipDistance", *commaSeparator);
    if (m_riveVulkanFeatures.rasterizationOrderColorAttachmentAccess)
        printf("%srasterizationOrderColorAttachmentAccess", *commaSeparator);
    if (m_riveVulkanFeatures.fragmentShaderPixelInterlock)
        printf("%sfragmentShaderPixelInterlock", *commaSeparator);
    if (m_riveVulkanFeatures.VK_KHR_portability_subset)
        printf("%sVK_KHR_portability_subset", *commaSeparator);
    printf(" ] ====\n");
}

VulkanDevice::~VulkanDevice() { m_vkDestroyDevice(m_device, nullptr); }

VkSurfaceCapabilitiesKHR VulkanDevice::getSurfaceCapabilities(
    VkSurfaceKHR surface) const
{
    VkSurfaceCapabilitiesKHR caps;
    VK_CHECK(m_vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice,
                                                         surface,
                                                         &caps));
    return caps;
}

bool VulkanDevice::hasSupportedDevice(VulkanInstance& instance,
                                      uint32_t minimumSupportedAPIVersion)
{
    std::vector<VkPhysicalDevice> physicalDevices;
    {
        DEFINE_AND_LOAD_INSTANCE_FUNC(vkEnumeratePhysicalDevices, instance);
        assert(vkEnumeratePhysicalDevices != nullptr);

        uint32_t count;
        VK_CHECK(
            vkEnumeratePhysicalDevices(instance.vkInstance(), &count, nullptr));
        physicalDevices.resize(count);
        VK_CHECK(vkEnumeratePhysicalDevices(instance.vkInstance(),
                                            &count,
                                            physicalDevices.data()));
    }

    DEFINE_AND_LOAD_INSTANCE_FUNC(vkGetPhysicalDeviceProperties, instance);
    assert(vkGetPhysicalDeviceProperties != nullptr);

    for (auto& device : physicalDevices)
    {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(device, &props);
        if (props.apiVersion >= minimumSupportedAPIVersion)
        {
            return true;
        }
    }

    return false;
}

VulkanDevice::FindDeviceResult VulkanDevice::findCompatiblePhysicalDevice(
    VulkanInstance& instance,
    const char* nameFilter,
    VkSurfaceKHR optionalSurfaceForValidation,
    uint32_t minimumSupportedAPIVersion)
{
    if (nameFilter != nullptr && nameFilter[0] == '\0')
    {
        // Clear name filter if it's empty string.
        nameFilter = nullptr;
    }

    auto desiredDeviceType = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
    if (nameFilter != nullptr &&
        (strcmp(nameFilter, "integrated") == 0 || strcmp(nameFilter, "i") == 0))
    {
        // "i" and "integrated" are special-case nameFilters that mean "use the
        // integrated GPU".
        desiredDeviceType = VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
        nameFilter = nullptr;
    }

    std::vector<VkPhysicalDevice> physicalDevices;
    {
        DEFINE_AND_LOAD_INSTANCE_FUNC(vkEnumeratePhysicalDevices, instance);
        assert(vkEnumeratePhysicalDevices != nullptr);

        uint32_t count;
        VK_CHECK(
            vkEnumeratePhysicalDevices(instance.vkInstance(), &count, nullptr));
        physicalDevices.resize(count);
        VK_CHECK(vkEnumeratePhysicalDevices(instance.vkInstance(),
                                            &count,
                                            physicalDevices.data()));
    }

    DEFINE_AND_LOAD_INSTANCE_FUNC(vkGetPhysicalDeviceProperties, instance);
    assert(vkGetPhysicalDeviceProperties != nullptr);

    DEFINE_AND_LOAD_INSTANCE_FUNC(vkGetPhysicalDeviceSurfaceFormatsKHR,
                                  instance);
    DEFINE_AND_LOAD_INSTANCE_FUNC(vkGetPhysicalDeviceSurfacePresentModesKHR,
                                  instance);

    auto IsSurfaceSupported = [&](VkPhysicalDevice device) {
        uint32_t surfaceFormatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device,
                                             optionalSurfaceForValidation,
                                             &surfaceFormatCount,
                                             nullptr);
        uint32_t presentModeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device,
                                                  optionalSurfaceForValidation,
                                                  &presentModeCount,
                                                  nullptr);

        // We don't care *what* formats/modes are returned, we just need to
        // know that there would be any that are supported.
        return surfaceFormatCount > 0 && presentModeCount > 0;
    };

    if (nameFilter != nullptr)
    {
        // Find a device containing the given filter
        FindDeviceResult matchResult = {
            .physicalDevice = VK_NULL_HANDLE,
        };
        std::vector<std::string> matchedDeviceNames;
        for (const auto& device : physicalDevices)
        {
            if (optionalSurfaceForValidation != VK_NULL_HANDLE &&
                !IsSurfaceSupported(device))
            {
                // This device does not support the surface we want to
                // present with.
                continue;
            }

            VkPhysicalDeviceProperties props{};
            vkGetPhysicalDeviceProperties(device, &props);

            if (props.apiVersion < minimumSupportedAPIVersion)
            {
                // This device is below our minimum supported API version
                continue;
            }

            if (strstr(props.deviceName, nameFilter) != nullptr)
            {
                matchResult = {
                    .physicalDevice = device,
                    .deviceName = props.deviceName,
                    .deviceType = props.deviceType,
                    .deviceAPIVersion = props.apiVersion,
                };
                matchedDeviceNames.push_back(std::string{props.deviceName});
            }
        }

        if (matchedDeviceNames.size() > 1)
        {
            LOG_ERROR_LINE("Cannot create device: Too many GPU matches for "
                           "filter '%s':",
                           nameFilter);
            for (auto& matchName : matchedDeviceNames)
            {
                LOG_ERROR_LINE("    '%s'", matchName.c_str());
            }
            LOG_ERROR_LINE("Please update the RIVE_GPU environment variable.");
            abort();
        }

        if (matchResult.physicalDevice != VK_NULL_HANDLE)
        {
            return matchResult;
        }

        LOG_ERROR_LINE(
            "Cannot create device: No GPU matches for filter '%s'.\nPlease "
            "update the RIVE_GPU environment variable.",
            nameFilter);
        abort();
    }
    else
    {
        // Without a nameFilter we are going to search for any device, but do a
        // first pass looking for the desired type of devices only.
        for (auto onlyAcceptDesiredType : {true, false})
        {
            for (const auto& device : physicalDevices)
            {
                VkPhysicalDeviceProperties props{};
                vkGetPhysicalDeviceProperties(device, &props);

                if (props.apiVersion < minimumSupportedAPIVersion)
                {
                    // This device is below our minimum supported API version
                    continue;
                }

                if (optionalSurfaceForValidation != VK_NULL_HANDLE &&
                    !IsSurfaceSupported(device))
                {
                    // This device does not support the surface we want to
                    // present with.
                    continue;
                }

                if (!onlyAcceptDesiredType ||
                    props.deviceType == desiredDeviceType)
                {
                    return {
                        .physicalDevice = device,
                        .deviceName = props.deviceName,
                        .deviceType = props.deviceType,
                        .deviceAPIVersion = props.apiVersion,
                    };
                }
            }
        }

        LOG_ERROR_LINE(
            "Cannot create device: no supported GPU devices detected.");
        abort();
    }
}

std::optional<VkPhysicalDeviceRasterizationOrderAttachmentAccessFeaturesEXT>
VulkanDevice::tryEnableRasterOrderFeatures(
    VulkanInstance& instance,
    const std::vector<VkExtensionProperties>& supportedExtensions,
    std::vector<const char*>& extensions)
{
    // Attempt to enable rasterization order attachment access, both by its
    // standard name and by the AMD-specific name
    for (auto* ext :
         {VK_EXT_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_EXTENSION_NAME,
          "VK_AMD_rasterization_order_attachment_access"})
    {
        if (addExtensionIfSupported(ext, supportedExtensions, extensions))
        {
            constexpr static VkPhysicalDeviceRasterizationOrderAttachmentAccessFeaturesEXT
                requestedRasterOrderFeatures = {
                    .sType =
                        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RASTERIZATION_ORDER_ATTACHMENT_ACCESS_FEATURES_EXT,
                    .rasterizationOrderColorAttachmentAccess = VK_TRUE,
                };

            auto testedRasterOrderFeatures = requestedRasterOrderFeatures;

            // Test to see if this is supported
            VkPhysicalDeviceFeatures2 features = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
                .pNext = &testedRasterOrderFeatures,
            };

            if (instance.tryGetPhysicalDeviceFeatures2(m_physicalDevice,
                                                       &features) &&
                testedRasterOrderFeatures
                    .rasterizationOrderColorAttachmentAccess)
            {
                // The query came back with the requested flag set so return the
                // feature set we want, it's supported!
                m_riveVulkanFeatures.rasterizationOrderColorAttachmentAccess =
                    true;
                return requestedRasterOrderFeatures;
            }
            break;
        }
    }

    return std::nullopt;
}

std::optional<VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT> VulkanDevice::
    tryEnableInterlockFeatures(
        VulkanInstance& instance,
        const std::vector<VkExtensionProperties>& supportedExtensions,
        std::vector<const char*>& extensions)
{
    if (addExtensionIfSupported(VK_EXT_FRAGMENT_SHADER_INTERLOCK_EXTENSION_NAME,
                                supportedExtensions,
                                extensions))
    {
        constexpr static VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT
            requestedInterlockFeatures = {
                .sType =
                    VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_INTERLOCK_FEATURES_EXT,
                .fragmentShaderPixelInterlock = VK_TRUE,
            };

        auto testedInterlockFeatures = requestedInterlockFeatures;

        // Test to see if this is supported
        VkPhysicalDeviceFeatures2 features = {
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
            .pNext = &testedInterlockFeatures,
        };

        if (instance.tryGetPhysicalDeviceFeatures2(m_physicalDevice,
                                                   &features) &&
            testedInterlockFeatures.fragmentShaderPixelInterlock)
        {
            // The query came back with the requested flag set so return the
            // feature set we want, it's supported!
            m_riveVulkanFeatures.fragmentShaderPixelInterlock = true;
            return requestedInterlockFeatures;
        }
    }

    return std::nullopt;
}

bool VulkanDevice::addExtensionIfSupported(
    const char* name,
    const std::vector<VkExtensionProperties>& supportedExtensions,
    std::vector<const char*>& extensions)
{
    for (const auto& ext : supportedExtensions)
    {
        if (strcmp(ext.extensionName, name) == 0)
        {
            extensions.push_back(name);
            return true;
        }
    }

    return false;
}

std::vector<VkSurfaceFormatKHR> VulkanDevice::getSurfaceFormats(
    VkSurfaceKHR surface)
{
    std::vector<VkSurfaceFormatKHR> formats;
    uint32_t count;
    m_vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice,
                                           surface,
                                           &count,
                                           nullptr);
    formats.resize(count);
    m_vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice,
                                           surface,
                                           &count,
                                           formats.data());
    return formats;
}

std::vector<VkPresentModeKHR> VulkanDevice::getSurfacePresentModes(
    VkSurfaceKHR surface)
{
    std::vector<VkPresentModeKHR> modes;
    uint32_t count;
    m_vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice,
                                                surface,
                                                &count,
                                                nullptr);
    modes.resize(count);
    m_vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice,
                                                surface,
                                                &count,
                                                modes.data());
    return modes;
}

void VulkanDevice::waitUntilIdle() const { m_vkDeviceWaitIdle(m_device); }

} // namespace rive_vkb
