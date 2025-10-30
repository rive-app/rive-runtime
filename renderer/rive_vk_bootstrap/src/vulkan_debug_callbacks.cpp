/*
 * Copyright 2024 Rive
 */

#include <array>
#include <assert.h>
#include <atomic>
#include <stdlib.h>
#include <string.h>
#include "logging.hpp"
#include "vulkan_debug_callbacks.hpp"

namespace rive_vkb
{

// Some messages should be fully ignored (because they are erroneous and would
// otherwise fill the logs)
static bool should_error_message_be_fully_ignored(const char* message)
{
#ifdef RIVE_ANDROID
    // Some Mali drivers (specifically the Mali-G71) will report spurious
    // resource limit errors/warnings where it incorrectly thinks the maximum
    // size is 0.
    // For example: "pCreateInfo->mipLevels (1) exceed image format
    // maxMipLevels (0) for format VK_FORMAT_R8G8B8A8_UNORM."
    static constexpr const char* s_ignoreList[] = {
        "exceeds allowable maximum image extent width 0 ",
        "exceeds allowable maximum image extent height 0 ",
        "exceeds allowable maximum image extent depth 0 ",
        "exceed image format maxMipLevels (0) ",
        " (format maxArrayLayers: 0)",
        "maximum resource size = 0 for format ",
        "(format sampleCounts: VkSampleCountFlags(0))",
    };

    for (const char* ignore : s_ignoreList)
    {
        if (strstr(message, ignore) != nullptr)
        {
            return true;
        }
    }
#endif

    return false;
}

static bool should_error_message_abort(const char* message)
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
    if (should_error_message_be_fully_ignored(pCallbackData->pMessage))
    {
        return VK_TRUE;
    }

    switch (messageType)
    {
        case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
            shouldAbort = should_error_message_abort(pCallbackData->pMessage);
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
    if (should_error_message_be_fully_ignored(message))
    {
        return false;
    }

    if ((flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) != 0)
    {
        bool shouldAbort = should_error_message_abort(message);

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
