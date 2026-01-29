/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/vulkan/vkutil.hpp"
#include "rive_vk_bootstrap/vulkan_instance.hpp"
#include "logging.hpp"
#include "vulkan_debug_callbacks.hpp"
#include "vulkan_error_handling.hpp"
#include "vulkan_library.hpp"

namespace rive_vkb
{
std::unique_ptr<VulkanInstance> VulkanInstance::Create(const Options& opts)
{
    bool success = true;
    auto outInstance =
        std::unique_ptr<VulkanInstance>{new VulkanInstance(opts, &success)};
    CONFIRM_OR_RETURN_VALUE(success, nullptr);
    return outInstance;
}

VulkanInstance::VulkanInstance(const Options& opts, bool* successOut) :
    m_library(VulkanLibrary::Create())
{
    *successOut = false;

    CONFIRM_OR_RETURN(m_library != nullptr);

    // Figure out which version to use
    m_instanceVersion = opts.idealAPIVersion;
    m_apiVersion = opts.idealAPIVersion;
    if (opts.idealAPIVersion > opts.minimumSupportedInstanceVersion)
    {
        if (m_library->canEnumerateInstanceVersion())
        {
            m_library->enumerateInstanceVersion(&m_instanceVersion);
        }
        else
        {
            // If vkEnumerateInstanceVersion doesn't exist then this we need
            // to assume this is Vulkan 1.0 (the function was introduced
            // in 1.1)
            m_instanceVersion = VK_API_VERSION_1_0;
        }
    }

    if (m_instanceVersion < VK_API_VERSION_1_1)
    {
        // The API veresion is intended to be the maximum Vulkan version
        // supported by the given application. However, Vulkan 1.0
        // implementations will fail with VK_ERROR_INCOMPATIBLE_DRIVER for
        // api versions > 1.0, so if we detect that it's not a 1.1 or
        // greater device, we need to force the api version to 1.0
        m_apiVersion = VK_API_VERSION_1_0;
    }

    CONFIRM_OR_RETURN_MSG(
        m_instanceVersion >= opts.minimumSupportedInstanceVersion,
        "Instance version %d.%d.%d is less than the minimum supported "
        "version of %d.%d.%d",
        VK_VERSION_MAJOR(m_instanceVersion),
        VK_VERSION_MINOR(m_instanceVersion),
        VK_VERSION_PATCH(m_instanceVersion),
        VK_VERSION_MAJOR(opts.minimumSupportedInstanceVersion),
        VK_VERSION_MINOR(opts.minimumSupportedInstanceVersion),
        VK_VERSION_PATCH(opts.minimumSupportedInstanceVersion));

    VkApplicationInfo appInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = opts.appName,
        .pEngineName = opts.engineName,
        .apiVersion = m_apiVersion,
    };

    std::vector<VkExtensionProperties> supportedExtensions;
    std::vector<VkLayerProperties> supportedLayers;
    {
        uint32_t count;
        m_library->enumerateInstanceExtensionProperties(nullptr,
                                                        &count,
                                                        nullptr);

        supportedExtensions.resize(count);
        m_library->enumerateInstanceExtensionProperties(
            nullptr,
            &count,
            supportedExtensions.data());

        m_library->enumerateInstanceLayerProperties(&count, nullptr);
        supportedLayers.resize(count);
        m_library->enumerateInstanceLayerProperties(&count,
                                                    supportedLayers.data());

        if (opts.logExtendedCreationInfo)
        {
            LOG_INFO_LINE("Reported Vulkan extensions:");
            for (const auto& ext : supportedExtensions)
            {
                LOG_INFO_LINE("    %s (version %d)",
                              ext.extensionName,
                              ext.specVersion);
            }

            LOG_INFO_LINE("Reported Vulkan layers:");
            for (const auto& layer : supportedLayers)
            {
                LOG_INFO_LINE("    %s (spec: %d, impl: %d)%s%s",
                              layer.layerName,
                              layer.specVersion,
                              layer.implementationVersion,
                              (layer.description[0] != '\0') ? "\n        "
                                                             : "",
                              layer.description);
            }
        }
    }

    bool enableDebugCallbacks = opts.wantDebugCallbacks;
    auto validationType = opts.desiredValidationType;

    std::vector<const char*> extensions;
    std::vector<const char*> layers;

    auto add_extension_if_supported = [&](const char* extName) {
        for (const auto& ext : supportedExtensions)
        {
            if (strcmp(ext.extensionName, extName) == 0)
            {
                extensions.push_back(extName);
                return true;
            }
        }

        return false;
    };

    auto add_layer_if_supported = [&](const char* layerName) {
        for (const auto& layer : supportedLayers)
        {
            if (strcmp(layer.layerName, layerName) == 0)
            {
                layers.push_back(layerName);
                return true;
            }
        }

        return false;
    };

    auto add_extensions_if_supported = [&](const auto& extNames) {
        for (auto* extName : extNames)
        {
            add_extension_if_supported(extName);
        }
    };

    for (auto* extName : opts.requiredExtensions)
    {
        CONFIRM_OR_RETURN_MSG(add_extension_if_supported(extName),
                              "Required extension '%s' was not supported",
                              extName);
    }

    add_extensions_if_supported(opts.optionalExtensions);

    VkInstanceCreateFlags instanceCreateFlags = 0;

#ifdef VK_KHR_portability_enumeration
    if (add_extension_if_supported(
            VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME))
    {
        instanceCreateFlags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }
#endif

    bool useFallbackDebugCallbacks = false;
    if (validationType != VulkanValidationType::none)
    {
        if (!add_layer_if_supported("VK_LAYER_KHRONOS_validation"))
        {
            LOG_ERROR_LINE("WARNING: Validation layers are not supported. "
                           "Creating context without validation layers.");
            validationType = VulkanValidationType::none;
        }
        else if (validationType != VulkanValidationType::core)
        {
            // If we're doing any other type of validation, add the settings
            // extension. (RenderDoc does not work when this extension is,
            // enabled, which is why we don't always enable it)
            extensions.push_back(VK_EXT_LAYER_SETTINGS_EXTENSION_NAME);
        }
    }

    if (enableDebugCallbacks)
    {
        if (validationType != VulkanValidationType::none)
        {
            // If we are enabling validation layers, VK_EXT_debug_utils willl
            // come along with that, even though it currently isn't in the
            // reported list of extensions, so just assume that it will work.
            extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }
        else if (!add_extension_if_supported(VK_EXT_DEBUG_UTILS_EXTENSION_NAME))
        {
            if (add_extension_if_supported(VK_EXT_DEBUG_REPORT_EXTENSION_NAME))
            {
                // Some devices only have the older debug report extension, so
                // use that where we need to.
                useFallbackDebugCallbacks = true;
            }
            else
            {
                LOG_ERROR_LINE("WARNING: Debug callbacks are not supported. "
                               "Creating context without debug callbacks.");
                enableDebugCallbacks = false;
            }
        }
    }

    bool enabledKHRDeviceProperties2 = false;
    if (m_instanceVersion < VK_API_VERSION_1_1)
    {
        enabledKHRDeviceProperties2 = add_extension_if_supported(
            VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    }

    VkInstanceCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .flags = instanceCreateFlags,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = uint32_t(layers.size()),
        .ppEnabledLayerNames = layers.data(),
        .enabledExtensionCount = uint32_t(extensions.size()),
        .ppEnabledExtensionNames = extensions.data(),
    };

    // Options to switch to synchronization validation vs. core validation
    constexpr VkBool32 vkFalseBool = VK_FALSE;
    constexpr VkBool32 vkTrueBool = VK_TRUE;
    const VkLayerSettingEXT settingsForSyncValidate[] = {
        {
            .pLayerName = "VK_LAYER_KHRONOS_validation",
            .pSettingName = "validate_core",
            .type = VK_LAYER_SETTING_TYPE_BOOL32_EXT,
            .valueCount = 1,
            .pValues = &vkFalseBool,
        },
        {
            .pLayerName = "VK_LAYER_KHRONOS_validation",
            .pSettingName = "validate_sync",
            .type = VK_LAYER_SETTING_TYPE_BOOL32_EXT,
            .valueCount = 1,
            .pValues = &vkTrueBool,
        },
        {
            .pLayerName = "VK_LAYER_KHRONOS_validation",
            .pSettingName = "thread_safety",
            .type = VK_LAYER_SETTING_TYPE_BOOL32_EXT,
            .valueCount = 1,
            .pValues = &vkTrueBool,
        },
    };

    VkLayerSettingsCreateInfoEXT validationLayerSettingsForSyncValidation = {
        .sType = VK_STRUCTURE_TYPE_LAYER_SETTINGS_CREATE_INFO_EXT,
        .settingCount = std::size(settingsForSyncValidate),
        .pSettings = settingsForSyncValidate,
    };

    if (validationType == VulkanValidationType::synchronization)
    {
        validationLayerSettingsForSyncValidation.pNext = createInfo.pNext;
        createInfo.pNext = &validationLayerSettingsForSyncValidation;
    }

    VK_CONFIRM_OR_RETURN_MSG(
        m_library->createInstance(&createInfo, VK_NULL_HANDLE, &m_instance),
        "Failed to create Vulkan instance");

    // Unfortunately this function cannot be loaded before there is an instance,
    // so a failure here is unrecoverable (as there would be no way to clean up
    // the created VkInstance)
    LOAD_MEMBER_INSTANCE_FUNC(this, vkDestroyInstance, *this);
    if (m_vkDestroyInstance == nullptr)
    {
        LOG_ERROR_LINE(
            "Could not load Vulkan function 'vkDestroyInstance'. This is an unrecoverable failure, becasue there is no way to properly dispose of the already-created Vulkan instance.");
        abort();
    }

    m_enabledExtensions = std::move(extensions);
    m_enabledLayers = std::move(layers);

    // This should always exist on Vulkan 1.1 but there are some devices where
    // the reported instance version is > 1.0 that actually only
    // have 1.0-enabled devices.
    LOAD_MEMBER_INSTANCE_FUNC(this, vkGetPhysicalDeviceFeatures2, *this);
    if (m_vkGetPhysicalDeviceFeatures2 == nullptr &&
        enabledKHRDeviceProperties2)
    {
        LOAD_MEMBER_INSTANCE_FUNC(this, vkGetPhysicalDeviceFeatures2KHR, *this);
    }

    if (enableDebugCallbacks)
    {
        if (useFallbackDebugCallbacks)
        {
            LOG_INFO_LINE("Note: " VK_EXT_DEBUG_UTILS_EXTENSION_NAME
                          " was not supported, falling back "
                          "to " VK_EXT_DEBUG_REPORT_EXTENSION_NAME ".");
            DEFINE_AND_LOAD_INSTANCE_FUNC(vkCreateDebugReportCallbackEXT,
                                          *this);

            VkDebugReportCallbackCreateInfoEXT debugCreateInfo = {};
            debugCreateInfo.sType =
                VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
            debugCreateInfo.flags =
                VK_DEBUG_REPORT_WARNING_BIT_EXT |
                VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT |
                VK_DEBUG_REPORT_ERROR_BIT_EXT;
            debugCreateInfo.pfnCallback = &defaultDebugReportCallback;
            vkCreateDebugReportCallbackEXT(m_instance,
                                           &debugCreateInfo,
                                           nullptr,
                                           &m_debugReportCallback);
        }
        else
        {
            DEFINE_AND_LOAD_INSTANCE_FUNC_OR_RETURN(
                vkCreateDebugUtilsMessengerEXT,
                *this);

            VkDebugUtilsMessengerCreateInfoEXT messengerCreateInfo = {
                .sType =
                    VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
                .messageSeverity =
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
                .pfnUserCallback = &defaultDebugUtilCallback,
            };

            if (auto result =
                    vkCreateDebugUtilsMessengerEXT(m_instance,
                                                   &messengerCreateInfo,
                                                   nullptr,
                                                   &m_debugUtilsMessenger);
                result != VK_SUCCESS)
            {
                LOG_ERROR_LINE(
                    "Failed to create debug messenger. Error code: %s",
                    rive::gpu::vkutil::string_from_vk_result(result));
            }
        }
    }

    *successOut = true;
}

VulkanInstance::~VulkanInstance()
{
    if (m_debugUtilsMessenger != VK_NULL_HANDLE)
    {
        DEFINE_AND_LOAD_INSTANCE_FUNC(vkDestroyDebugUtilsMessengerEXT, *this);
        assert(vkDestroyDebugUtilsMessengerEXT != nullptr);
        vkDestroyDebugUtilsMessengerEXT(m_instance,
                                        m_debugUtilsMessenger,
                                        nullptr);
    }

    if (m_debugReportCallback != VK_NULL_HANDLE)
    {
        DEFINE_AND_LOAD_INSTANCE_FUNC(vkDestroyDebugReportCallbackEXT, *this);
        assert(vkDestroyDebugReportCallbackEXT != nullptr);
        vkDestroyDebugReportCallbackEXT(m_instance,
                                        m_debugReportCallback,
                                        nullptr);
    }

    if (m_instance != VK_NULL_HANDLE)
    {
        m_vkDestroyInstance(m_instance, nullptr);
    }
}

PFN_vkVoidFunction VulkanInstance::loadInstanceFunc(const char* name) const
{
    return m_library->getInstanceProcAddr(m_instance, name);
}

bool VulkanInstance::tryGetPhysicalDeviceFeatures2(
    VkPhysicalDevice device,
    VkPhysicalDeviceFeatures2* inoutFeatures)
{
    if (m_vkGetPhysicalDeviceFeatures2 != nullptr)
    {
        m_vkGetPhysicalDeviceFeatures2(device, inoutFeatures);
        return true;
    }
    else if (m_vkGetPhysicalDeviceFeatures2KHR != nullptr)
    {
        m_vkGetPhysicalDeviceFeatures2KHR(device, inoutFeatures);
        return true;
    }

    return false;
}

PFN_vkGetInstanceProcAddr VulkanInstance::getVkGetInstanceProcAddrPtr() const
{
    return m_library->getVkGetInstanceProcAddrPtr();
}

} // namespace rive_vkb