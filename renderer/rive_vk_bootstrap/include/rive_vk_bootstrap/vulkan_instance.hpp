#include <vulkan/vulkan.h>

#include <vector>
#include "rive/span.hpp"

namespace rive_vkb
{
#ifndef NDEBUG
constexpr bool RIVE_DEFAULT_VULKAN_DEBUG_PREFERENCE = true;
#else
constexpr bool RIVE_DEFAULT_VULKAN_DEBUG_PREFERENCE = false;
#endif

class VulkanLibrary;

class VulkanInstance
{
public:
    struct Options
    {
        const char* appName = "fiddle_context app";
        const char* engineName = "Rive Renderer";

        uint32_t idealAPIVersion = VK_API_VERSION_1_3;
        uint32_t minimumSupportedInstanceVersion = VK_API_VERSION_1_0;

        rive::Span<const char*> requiredExtensions;
        rive::Span<const char*> optionalExtensions;

        bool wantValidationLayers = RIVE_DEFAULT_VULKAN_DEBUG_PREFERENCE;
        bool wantDebugCallbacks = RIVE_DEFAULT_VULKAN_DEBUG_PREFERENCE;

        bool logExtendedCreationInfo = false;
    };

    VulkanInstance(const Options&);
    ~VulkanInstance();

    VulkanInstance(const VulkanInstance&) = delete;
    VulkanInstance& operator=(const VulkanInstance&) = delete;

    template <typename T> T loadInstanceFunc(const char* name) const
    {
        return reinterpret_cast<T>(loadInstanceFunc(name));
    }

    VkInstance vkInstance() const { return m_instance; }
    uint32_t instanceVersion() const { return m_instanceVersion; }
    uint32_t apiVersion() const { return m_apiVersion; }

    PFN_vkGetInstanceProcAddr getVkGetInstanceProcAddrPtr() const;

    bool tryGetPhysicalDeviceFeatures2(
        VkPhysicalDevice,
        VkPhysicalDeviceFeatures2* inoutFeatures);

private:
    PFN_vkVoidFunction loadInstanceFunc(const char* name) const;

    std::unique_ptr<VulkanLibrary> m_library;
    std::vector<const char*> m_enabledExtensions;
    std::vector<const char*> m_enabledLayers;
    uint32_t m_instanceVersion;
    uint32_t m_apiVersion;
    VkInstance m_instance;
    PFN_vkDestroyInstance m_vkDestroyInstance;

    // These two are optional, at most one of them will be set
    PFN_vkGetPhysicalDeviceFeatures2 m_vkGetPhysicalDeviceFeatures2 = nullptr;
    PFN_vkGetPhysicalDeviceFeatures2KHR m_vkGetPhysicalDeviceFeatures2KHR =
        nullptr;
    VkDebugUtilsMessengerEXT m_debugUtilsMessenger = VK_NULL_HANDLE;
    VkDebugReportCallbackEXT m_debugReportCallback = VK_NULL_HANDLE;
};

} // namespace rive_vkb