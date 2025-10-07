/*
 * Copyright 2024 Rive
 */

#include <vulkan/vulkan.h>

namespace rive_vkb
{
VKAPI_ATTR VkBool32 VKAPI_CALL
defaultDebugReportCallback(VkDebugReportFlagsEXT flags,
                           VkDebugReportObjectTypeEXT objectType,
                           uint64_t object,
                           size_t location,
                           int32_t messageCode,
                           const char* pLayerPrefix,
                           const char* pMessage,
                           void* pUserData);

VKAPI_ATTR VkBool32 VKAPI_CALL
defaultDebugUtilCallback(VkDebugUtilsMessageSeverityFlagBitsEXT,
                         VkDebugUtilsMessageTypeFlagsEXT,
                         const VkDebugUtilsMessengerCallbackDataEXT*,
                         void* pUserData);
} // namespace rive_vkb
