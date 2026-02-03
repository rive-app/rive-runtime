#include <vulkan/vulkan.h>

#include <vector>
#include "rive/span.hpp"

namespace rive_vkb
{
enum class VulkanValidationType
{
    none,
    core,
    synchronization,
};

#ifndef NDEBUG
constexpr bool RIVE_DEFAULT_VULKAN_DEBUG_PREFERENCE = true;
constexpr VulkanValidationType RIVE_DEFAULT_VULKAN_VALIDATION_TYPE =
    VulkanValidationType::core;
#else
constexpr bool RIVE_DEFAULT_VULKAN_DEBUG_PREFERENCE = false;
constexpr VulkanValidationType RIVE_DEFAULT_VULKAN_VALIDATION_TYPE =
    VulkanValidationType::none;
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
        uint32_t minimumSupportedInstanceVersion = VK_API_VERSION_1_1;

        rive::Span<const char*> requiredExtensions;
        rive::Span<const char*> optionalExtensions;

        VulkanValidationType desiredValidationType =
            RIVE_DEFAULT_VULKAN_VALIDATION_TYPE;
        bool wantDebugCallbacks = RIVE_DEFAULT_VULKAN_DEBUG_PREFERENCE;

        bool logExtendedCreationInfo = false;
    };

    // Create a VulkanInstance object, or return nullptr and print to stderr on
    // failure.
    static std::unique_ptr<VulkanInstance> Create(const Options&);
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
    VulkanInstance(const Options&, bool* successOut);

    PFN_vkVoidFunction loadInstanceFunc(const char* name) const;

    const std::unique_ptr<VulkanLibrary> m_library;
    std::vector<const char*> m_enabledExtensions;
    std::vector<const char*> m_enabledLayers;
    uint32_t m_instanceVersion = 0;
    uint32_t m_apiVersion = 0;
    PFN_vkDestroyInstance m_vkDestroyInstance = nullptr;
    VkInstance m_instance = VK_NULL_HANDLE;

    // These two are optional, at most one of them will be set
    PFN_vkGetPhysicalDeviceFeatures2 m_vkGetPhysicalDeviceFeatures2 = nullptr;
    PFN_vkGetPhysicalDeviceFeatures2KHR m_vkGetPhysicalDeviceFeatures2KHR =
        nullptr;
    VkDebugUtilsMessengerEXT m_debugUtilsMessenger = VK_NULL_HANDLE;
    VkDebugReportCallbackEXT m_debugReportCallback = VK_NULL_HANDLE;
};

} // namespace rive_vkb