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

        uint32_t minimumSupportedAPIVersion = VK_API_VERSION_1_0;

        // If this is set to a valid surface (and not a headless device), device
        // discovery will test for present compatibility to this surface
        VkSurfaceKHR presentationSurfaceForDeviceSelection = VK_NULL_HANDLE;
    };

    VulkanDevice(VulkanInstance&, const Options&);
    ~VulkanDevice();

    VulkanDevice(const VulkanDevice&) = delete;
    VulkanDevice& operator=(const VulkanDevice&) = delete;

    template <typename T> T loadDeviceFunc(const char* name) const
    {
        return reinterpret_cast<T>(m_vkGetDeviceProcAddr(m_device, name));
    }

    VkDevice vkDevice() const { return m_device; }

    VkPhysicalDevice vkPhysicalDevice() const { return m_physicalDevice; }

    std::vector<VkSurfaceFormatKHR> getSurfaceFormats(VkSurfaceKHR surface);
    std::vector<VkPresentModeKHR> getSurfacePresentModes(VkSurfaceKHR surface);

    VkSurfaceCapabilitiesKHR getSurfaceCapabilities(VkSurfaceKHR) const;

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

private:
    struct FindDeviceResult
    {
        VkPhysicalDevice physicalDevice;
        std::string deviceName;
        VkPhysicalDeviceType deviceType;
        uint32_t deviceAPIVersion;
    };

    static FindDeviceResult findCompatiblePhysicalDevice(
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

    VkPhysicalDevice m_physicalDevice;
    VkDevice m_device;
    PFN_vkGetDeviceProcAddr m_vkGetDeviceProcAddr;
    PFN_vkGetDeviceQueue m_vkGetDeviceQueue;
    PFN_vkDestroyDevice m_vkDestroyDevice;
    PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR
        m_vkGetPhysicalDeviceSurfaceCapabilitiesKHR;
    PFN_vkGetPhysicalDeviceSurfaceFormatsKHR
        m_vkGetPhysicalDeviceSurfaceFormatsKHR;
    PFN_vkGetPhysicalDeviceSurfacePresentModesKHR
        m_vkGetPhysicalDeviceSurfacePresentModesKHR;
    PFN_vkDeviceWaitIdle m_vkDeviceWaitIdle;

    rive::gpu::VulkanFeatures m_riveVulkanFeatures;
    uint32_t m_graphicsQueueFamilyIndex;
};
} // namespace rive_vkb
