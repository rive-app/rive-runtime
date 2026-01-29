/*
 * Copyright 2025 Rive
 */

#include <string>
#include <sstream>
#include "rive/renderer/vulkan/vkutil.hpp"
#include "rive_vk_bootstrap/vulkan_device.hpp"
#include "rive_vk_bootstrap/vulkan_instance.hpp"
#include "shaders/constants.glsl"
#include "logging.hpp"
#include "vulkan_error_handling.hpp"
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

VulkanDevice::DriverVersion unpackDriverVersion(
    const VkPhysicalDeviceProperties& props)
{
    if (props.vendorID == VULKAN_VENDOR_NVIDIA)
    {
        // NVidia uses 10|8|8|6 encoding for driver version. We'll ignore the
        // fourth version section.
        return {
            .major = props.driverVersion >> 22,
            .minor = (props.driverVersion >> 14) & 0xff,
            .patch = (props.driverVersion >> 6) & 0xff,
        };
    }
#ifdef _WIN32
    else if (props.vendorID == VULKAN_VENDOR_INTEL)
    {
        return {
            .major = props.driverVersion >> 14,
            .minor = props.driverVersion & 0x3fff,
            .patch = 0,
        };
    }
#endif
    else
    {
        // Everything else seems to use the standard Vulkan encoding.
        return {
            .major = VK_API_VERSION_MAJOR(props.driverVersion),
            .minor = VK_API_VERSION_MINOR(props.driverVersion),
            .patch = VK_API_VERSION_PATCH(props.driverVersion),
        };
    }
}

std::unique_ptr<VulkanDevice> VulkanDevice::Create(VulkanInstance& instance,
                                                   const Options& opts)
{
    bool success = true;

    // Private constructor, can't use make_unique
    auto outDevice = std::unique_ptr<VulkanDevice>{
        new VulkanDevice(instance, opts, &success)};
    CONFIRM_OR_RETURN_VALUE(success, nullptr);
    return outDevice;
}

VulkanDevice::VulkanDevice(VulkanInstance& instance,
                           const Options& opts,
                           bool* successOut)
{
    *successOut = false;

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

    auto findResult = tryFindCompatiblePhysicalDevice(
        instance,
        nameFilter,
        opts.presentationSurfaceForDeviceSelection,
        opts.minimumSupportedAPIVersion);
    CONFIRM_OR_RETURN(findResult.has_value());

    m_physicalDevice = findResult->physicalDevice;
    m_name = findResult->deviceName;
    m_driverVersion = findResult->driverVersion;

    DEFINE_AND_LOAD_INSTANCE_FUNC_OR_RETURN(vkGetPhysicalDeviceFeatures,
                                            instance);

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
        .apiVersion = findResult->deviceAPIVersion,
        .independentBlend = bool(requestedFeatures.independentBlend),
        .fillModeNonSolid = bool(requestedFeatures.fillModeNonSolid),
        .fragmentStoresAndAtomics =
            bool(requestedFeatures.fragmentStoresAndAtomics),
        .shaderClipDistance = bool(requestedFeatures.shaderClipDistance),
    };

    DEFINE_AND_LOAD_INSTANCE_FUNC_OR_RETURN(
        vkEnumerateDeviceExtensionProperties,
        instance);

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
        CONFIRM_OR_RETURN_MSG(
            addExtensionIfSupported(VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                                    supportedExtensions,
                                    addedExtensions),
            "Cannot create device: %s is not supported.",
            VK_KHR_SWAPCHAIN_EXTENSION_NAME);
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
        DEFINE_AND_LOAD_INSTANCE_FUNC_OR_RETURN(
            vkGetPhysicalDeviceQueueFamilyProperties,
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

    CONFIRM_OR_RETURN_MSG(m_graphicsQueueFamilyIndex !=
                              std::numeric_limits<uint32_t>::max(),
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

    DEFINE_AND_LOAD_INSTANCE_FUNC_OR_RETURN(vkCreateDevice, instance);
    VK_CONFIRM_OR_RETURN_MSG(
        vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &m_device),
        "Failed to create Vulkan device.");

    LOAD_MEMBER_INSTANCE_FUNC_OR_RETURN(this, vkGetDeviceProcAddr, instance);
    LOAD_MEMBER_INSTANCE_FUNC_OR_RETURN(this, vkDeviceWaitIdle, instance);

    // Unfortunately, this function cannot be loaded until the device already
    // exists, so if this fails for some reason (it shouldn't, in practice),
    // it's unrecoverable as we would be unable to destroy the device.
    m_vkDestroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(
        m_vkGetDeviceProcAddr(m_device, "vkDestroyDevice"));
    if (m_vkDestroyDevice == nullptr)
    {
        LOG_ERROR_LINE(
            "Could not load Vulkan function 'vkDestroyDevice'. This is an unrecoverable failure, becasue there is no way to properly dispose of the already-created Vulkan device.");
        abort();
    }

    m_vkGetDeviceQueue = reinterpret_cast<PFN_vkGetDeviceQueue>(
        m_vkGetDeviceProcAddr(m_device, "vkGetDeviceQueue"));
    CONFIRM_OR_RETURN_MSG(m_vkGetDeviceQueue != nullptr,
                          "Could not load Vulkan function 'vkGetDeviceQueue'");

    // These functions are only required for presentation-based code - they can
    //  be missing (null) in headless mode
    LOAD_MEMBER_INSTANCE_FUNC(this,
                              vkGetPhysicalDeviceSurfaceFormatsKHR,
                              instance);
    LOAD_MEMBER_INSTANCE_FUNC(this,
                              vkGetPhysicalDeviceSurfacePresentModesKHR,
                              instance);
    LOAD_MEMBER_INSTANCE_FUNC(this,
                              vkGetPhysicalDeviceSurfaceCapabilitiesKHR,
                              instance);
    if (!opts.headless)
    {
        CONFIRM_OR_RETURN_MSG(
            m_vkGetPhysicalDeviceSurfaceFormatsKHR != nullptr,
            "Failed to load Vulkan function 'vkGetPhysicalDeviceSurfaceFormatsKHR'");

        CONFIRM_OR_RETURN_MSG(
            m_vkGetPhysicalDeviceSurfacePresentModesKHR != nullptr,
            "Failed to load Vulkan function 'vkGetPhysicalDeviceSurfacePresentModesKHR'");

        CONFIRM_OR_RETURN_MSG(
            m_vkGetPhysicalDeviceSurfaceCapabilitiesKHR != nullptr,
            "Failed to load Vulkan function 'vkGetPhysicalDeviceSurfaceCapabilitiesKHR'");
    }

    if (opts.printInitializationMessage)
    {
        printf("==== Vulkan %i.%i.%i GPU (%s): %s (driver %i.%i.%i) [ ",
               VK_API_VERSION_MAJOR(m_riveVulkanFeatures.apiVersion),
               VK_API_VERSION_MINOR(m_riveVulkanFeatures.apiVersion),
               VK_API_VERSION_PATCH(m_riveVulkanFeatures.apiVersion),
               physicalDeviceTypeName(findResult->deviceType),
               findResult->deviceName.c_str(),
               findResult->driverVersion.major,
               findResult->driverVersion.minor,
               findResult->driverVersion.patch);
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
            printf("%srasterizationOrderColorAttachmentAccess",
                   *commaSeparator);
        if (m_riveVulkanFeatures.fragmentShaderPixelInterlock)
            printf("%sfragmentShaderPixelInterlock", *commaSeparator);
        if (m_riveVulkanFeatures.VK_KHR_portability_subset)
            printf("%sVK_KHR_portability_subset", *commaSeparator);
        printf(" ] ====\n");
    }

    *successOut = true;
}

VulkanDevice::~VulkanDevice()
{
    if (m_device != VK_NULL_HANDLE)
    {
        m_vkDestroyDevice(m_device, nullptr);
    }
}

VkResult VulkanDevice::getSurfaceCapabilities(
    VkSurfaceKHR surface,
    VkSurfaceCapabilitiesKHR* outCaps) const
{
    if (m_vkGetPhysicalDeviceSurfaceCapabilitiesKHR == nullptr)
    {
        LOG_ERROR_LINE(
            "Could not load Vulkan function 'vkGetPhysicalDeviceSurfaceCapabilitiesKHR', cannot get surface capabilities.");
        return {};
    }

    assert(outCaps != nullptr);
    return m_vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice,
                                                       surface,
                                                       outCaps);
}

bool VulkanDevice::hasSupportedDevice(VulkanInstance& instance,
                                      uint32_t minimumSupportedAPIVersion)
{
    std::vector<VkPhysicalDevice> physicalDevices;
    DEFINE_AND_LOAD_INSTANCE_FUNC_OR_RETURN_VALUE(vkEnumeratePhysicalDevices,
                                                  instance,
                                                  false);

    DEFINE_AND_LOAD_INSTANCE_FUNC_OR_RETURN_VALUE(vkGetPhysicalDeviceProperties,
                                                  instance,
                                                  false);

    {
        uint32_t count;
        VK_CONFIRM_OR_RETURN_VALUE_MSG(
            vkEnumeratePhysicalDevices(instance.vkInstance(), &count, nullptr),
            false,
            "Failed to query Vulkan physical device count.");

        physicalDevices.resize(count);
        VK_CONFIRM_OR_RETURN_VALUE_MSG(
            vkEnumeratePhysicalDevices(instance.vkInstance(),
                                       &count,
                                       physicalDevices.data()),
            false,
            "Failed to enumerate Vulkan physical devices.");
    }

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

std::optional<VulkanDevice::FindDeviceResult> VulkanDevice::
    tryFindCompatiblePhysicalDevice(VulkanInstance& instance,
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
        DEFINE_AND_LOAD_INSTANCE_FUNC_OR_RETURN_VALUE(
            vkEnumeratePhysicalDevices,
            instance,
            std::nullopt);

        uint32_t count;
        VK_CONFIRM_OR_RETURN_VALUE_MSG(
            vkEnumeratePhysicalDevices(instance.vkInstance(), &count, nullptr),
            std::nullopt,
            "Failed to query Vulkan physical device count.");
        physicalDevices.resize(count);
        VK_CONFIRM_OR_RETURN_VALUE_MSG(
            vkEnumeratePhysicalDevices(instance.vkInstance(),
                                       &count,
                                       physicalDevices.data()),
            std::nullopt,
            "Failed to enumerate Vulkan physical devices.");
    }

    DEFINE_AND_LOAD_INSTANCE_FUNC_OR_RETURN_VALUE(vkGetPhysicalDeviceProperties,
                                                  instance,
                                                  std::nullopt);

    DEFINE_AND_LOAD_INSTANCE_FUNC(vkGetPhysicalDeviceSurfaceFormatsKHR,
                                  instance);
    DEFINE_AND_LOAD_INSTANCE_FUNC(vkGetPhysicalDeviceSurfacePresentModesKHR,
                                  instance);

    if (optionalSurfaceForValidation != VK_NULL_HANDLE)
    {
        CONFIRM_OR_RETURN_VALUE_MSG(
            vkGetPhysicalDeviceSurfaceFormatsKHR != nullptr,
            std::nullopt,
            "Failed to load Vulkan function 'vkGetPhysicalDeviceSurfaceFormatsKHR'");

        CONFIRM_OR_RETURN_VALUE_MSG(
            vkGetPhysicalDeviceSurfacePresentModesKHR != nullptr,
            std::nullopt,
            "Failed to load Vulkan function 'vkGetPhysicalDeviceSurfacePresentModesKHR'");
    }

    auto IsSurfaceSupported = [&](VkPhysicalDevice device) {
        uint32_t surfaceFormatCount = 0;

        // This lambda should only be called with an optional surface.
        assert(optionalSurfaceForValidation != VK_NULL_HANDLE);
        assert(vkGetPhysicalDeviceSurfaceFormatsKHR != nullptr);
        assert(vkGetPhysicalDeviceSurfacePresentModesKHR != nullptr);

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
                    .driverVersion = unpackDriverVersion(props),
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
            return std::nullopt;
        }

        if (matchResult.physicalDevice != VK_NULL_HANDLE)
        {
            return matchResult;
        }

        LOG_ERROR_LINE(
            "Cannot create device: No GPU matches for filter '%s'.\nPlease "
            "update the RIVE_GPU environment variable.",
            nameFilter);
        return std::nullopt;
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
                    return FindDeviceResult{
                        .physicalDevice = device,
                        .deviceName = props.deviceName,
                        .deviceType = props.deviceType,
                        .deviceAPIVersion = props.apiVersion,
                        .driverVersion = unpackDriverVersion(props),
                    };
                }
            }
        }

        LOG_ERROR_LINE(
            "Cannot create device: no supported GPU devices detected.");
        return std::nullopt;
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
    if (m_vkGetPhysicalDeviceSurfaceFormatsKHR == nullptr)
    {
        LOG_ERROR_LINE(
            "Could not load Vulkan function 'vkGetPhysicalDeviceSurfaceFormatsKHR', cannot test surface formats.");
        return {};
    }

    std::vector<VkSurfaceFormatKHR> formats;
    uint32_t count;
    VK_CONFIRM_OR_RETURN_VALUE_MSG(
        m_vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice,
                                               surface,
                                               &count,
                                               nullptr),
        std::vector<VkSurfaceFormatKHR>{},
        "Could not query Vulkan physical device surface format count");
    formats.resize(count);
    VK_CONFIRM_OR_RETURN_VALUE_MSG(
        m_vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice,
                                               surface,
                                               &count,
                                               formats.data()),
        std::vector<VkSurfaceFormatKHR>{},
        "Could not get Vulkan physical device surface formats");
    return formats;
}

std::vector<VkPresentModeKHR> VulkanDevice::getSurfacePresentModes(
    VkSurfaceKHR surface)
{
    if (m_vkGetPhysicalDeviceSurfacePresentModesKHR == nullptr)
    {
        LOG_ERROR_LINE(
            "Could not load Vulkan function 'vkGetPhysicalDeviceSurfacePresentModesKHR', cannot get surface present modes.");
        return {};
    }

    std::vector<VkPresentModeKHR> modes;
    uint32_t count;
    VK_CONFIRM_OR_RETURN_VALUE_MSG(
        m_vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice,
                                                    surface,
                                                    &count,
                                                    nullptr),
        std::vector<VkPresentModeKHR>{},
        "Could not query Vulkan physical device present mode count");
    modes.resize(count);
    VK_CONFIRM_OR_RETURN_VALUE_MSG(
        m_vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice,
                                                    surface,
                                                    &count,
                                                    modes.data()),
        std::vector<VkPresentModeKHR>{},
        "Could not get Vulkan physical device presentation modes");
    return modes;
}

void VulkanDevice::waitUntilIdle() const { m_vkDeviceWaitIdle(m_device); }

} // namespace rive_vkb
