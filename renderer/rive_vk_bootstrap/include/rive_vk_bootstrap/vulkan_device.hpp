/*
 * Copyright 2025 Rive
 */

#include <optional>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

#include "rive/renderer/vulkan/vulkan_context.hpp"

namespace rive_vkb
{
class VulkanInstance;

class VulkanDevice
{
public:
    struct Options
    {
        bool coreFeaturesOnly = false;
        const char* gpuNameFilter = nullptr;
        bool headless = false;
        bool printInitializationMessage = true;

        uint32_t minimumSupportedAPIVersion = VK_API_VERSION_1_0;

        // If this is set to a valid surface (and not a headless device), device
        // discovery will test for present compatibility to this surface
        VkSurfaceKHR presentationSurfaceForDeviceSelection = VK_NULL_HANDLE;
    };

    struct DriverVersion
    {
        uint32_t major = 0;
        uint32_t minor = 0;
        uint32_t patch = 0;
    };

    static std::unique_ptr<VulkanDevice> Create(VulkanInstance&,
                                                const Options&);
    ~VulkanDevice();

    VulkanDevice(const VulkanDevice&) = delete;
    VulkanDevice& operator=(const VulkanDevice&) = delete;

    template <typename T> T loadDeviceFunc(const char* name) const
    {
        return reinterpret_cast<T>(m_vkGetDeviceProcAddr(m_device, name));
    }

    VkDevice vkDevice() const { return m_device; }

    VkPhysicalDevice vkPhysicalDevice() const { return m_physicalDevice; }

    // Get the surface formats for a given surface. Returns an empty vector on
    // failure.
    std::vector<VkSurfaceFormatKHR> getSurfaceFormats(VkSurfaceKHR surface);

    // Get the surface presentation modes for a given surface, or an empty
    // vector on failure.
    std::vector<VkPresentModeKHR> getSurfacePresentModes(VkSurfaceKHR surface);

    VkResult getSurfaceCapabilities(VkSurfaceKHR,
                                    VkSurfaceCapabilitiesKHR*) const;

    rive::gpu::VulkanFeatures vulkanFeatures() const
    {
        return m_riveVulkanFeatures;
    }

    void waitUntilIdle() const;

    uint32_t graphicsQueueFamilyIndex() const
    {
        return m_graphicsQueueFamilyIndex;
    }

    static bool hasSupportedDevice(VulkanInstance&,
                                   uint32_t minimumSupportedAPIVersion);

    const std::string& name() const { return m_name; }

    const DriverVersion& driverVersion() const { return m_driverVersion; }

private:
    VulkanDevice(VulkanInstance&, const Options&, bool* successOut);

    struct FindDeviceResult
    {
        VkPhysicalDevice physicalDevice;
        std::string deviceName;
        VkPhysicalDeviceType deviceType;
        uint32_t deviceAPIVersion;
        DriverVersion driverVersion;
    };

    static std::optional<FindDeviceResult> tryFindCompatiblePhysicalDevice(
        VulkanInstance&,
        const char* nameFilter,
        VkSurfaceKHR optionalSurfaceForValidation,
        uint32_t minimumSupportedAPIVersion);

    std::optional<VkPhysicalDeviceRasterizationOrderAttachmentAccessFeaturesEXT>
    tryEnableRasterOrderFeatures(
        VulkanInstance&,
        const std::vector<VkExtensionProperties>& supportedExtensions,
        std::vector<const char*>& extensions);

    std::optional<VkPhysicalDeviceFragmentShaderInterlockFeaturesEXT>
    tryEnableInterlockFeatures(
        VulkanInstance&,
        const std::vector<VkExtensionProperties>& supportedExtensions,
        std::vector<const char*>& extensions);

    bool addExtensionIfSupported(
        const char* name,
        const std::vector<VkExtensionProperties>& supportedExtensions,
        std::vector<const char*>& extensions);

    std::vector<VkQueueFamilyProperties> m_queueFamilyProperties;
    std::string m_name;
    DriverVersion m_driverVersion;

    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    PFN_vkGetDeviceProcAddr m_vkGetDeviceProcAddr = nullptr;
    PFN_vkGetDeviceQueue m_vkGetDeviceQueue = nullptr;
    PFN_vkDestroyDevice m_vkDestroyDevice = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR
        m_vkGetPhysicalDeviceSurfaceCapabilitiesKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR
        m_vkGetPhysicalDeviceSurfaceFormatsKHR = nullptr;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR
        m_vkGetPhysicalDeviceSurfacePresentModesKHR = nullptr;
    PFN_vkDeviceWaitIdle m_vkDeviceWaitIdle = nullptr;

    rive::gpu::VulkanFeatures m_riveVulkanFeatures;
    uint32_t m_graphicsQueueFamilyIndex = 0;
};
} // namespace rive_vkb
