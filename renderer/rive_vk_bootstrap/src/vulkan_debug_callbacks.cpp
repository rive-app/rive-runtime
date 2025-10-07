/*
 * Copyright 2024 Rive
 */

#include <array>
#include <stdlib.h>
#include <string.h>
#include "logging.hpp"
#include "vulkan_debug_callbacks.hpp"

namespace rive_vkb
{
static bool shouldErrorMessageAbort(const char* message)
{
    static std::array<const char*, 3> s_ignoredValidationMsgList = {
        // Swiftshader generates this error during
        // vkEnumeratePhysicalDevices. It seems fine to ignore.
        "Copying old device 0 into new device 0",
        // Cirrus Ubuntu runner w/ Nvidia gpu reports this but it seems
        // harmless.
        "terminator_CreateInstance: Received return code -3 from call to "
        "vkCreateInstance in ICD "
        "/usr/lib/x86_64-linux-gnu/libvulkan_virtio.so. Skipping this driver.",
        "Override layer has override paths set to "
        "D:\\VulkanSDK\\1.3.296.0\\Bin",
    };

    for (const char* msg : s_ignoredValidationMsgList)
    {
        if (strcmp(message, msg) == 0)
        {
            return false;
        }
    }

    return true;
}

VKAPI_ATTR VkBool32 VKAPI_CALL defaultDebugUtilCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    bool shouldAbort = true;

    switch (messageType)
    {
        case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
            shouldAbort = shouldErrorMessageAbort(pCallbackData->pMessage);
            LOG_ERROR_LINE("Rive Vulkan error%s: %i: %s: %s\n",
                           (shouldAbort) ? "" : " (error ignored)",
                           pCallbackData->messageIdNumber,
                           pCallbackData->pMessageIdName,
                           pCallbackData->pMessage);

            if (shouldAbort)
            {
                abort();
            }
            break;

        case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
            LOG_ERROR_LINE("Rive Vulkan Validation error: %i: %s: %s",
                           pCallbackData->messageIdNumber,
                           pCallbackData->pMessageIdName,
                           pCallbackData->pMessage);
            abort();
        case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
            LOG_ERROR_LINE("Rive Vulkan Performance warning: %i: %s: %s",
                           pCallbackData->messageIdNumber,
                           pCallbackData->pMessageIdName,
                           pCallbackData->pMessage);
            break;
    }
    return VK_TRUE;
}

VKAPI_ATTR VkBool32 VKAPI_CALL
defaultDebugReportCallback(VkDebugReportFlagsEXT flags,
                           VkDebugReportObjectTypeEXT, // objectType
                           uint64_t,                   // object
                           size_t,                     // location
                           int32_t messageCode,
                           const char* layerPrefix,
                           const char* message,
                           void* pUserData)
{
    if ((flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) != 0)
    {
        bool shouldAbort = shouldErrorMessageAbort(message);

        LOG_ERROR_LINE("Rive Vulkan error%s: %s (%i): %s",
                       (shouldAbort) ? "" : " (error ignored)",
                       layerPrefix,
                       messageCode,
                       message);

        if (shouldAbort)
        {
            abort();
        }
    }
    else if ((flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) != 0)
    {
        LOG_ERROR_LINE("Rive Vulkan warning: %s (%i): %s",
                       layerPrefix,
                       messageCode,
                       message);
        abort();
    }
    else if ((flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT) != 0)
    {
        LOG_ERROR_LINE("Rive Vulkan debug message: %s (%i): %s",
                       layerPrefix,
                       messageCode,
                       message);
        abort();
    }
    else if ((flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) != 0)
    {
        LOG_ERROR_LINE("Rive Vulkan performance warning: %s (%i): %s",
                       layerPrefix,
                       messageCode,
                       message);
    }
    else
    {
        LOG_INFO_LINE("Rive Vulkan info: %s (%i): %s",
                      layerPrefix,
                      messageCode,
                      message);
    }

    return false;
}

} // namespace rive_vkb
