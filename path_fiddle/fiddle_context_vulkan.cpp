#include "fiddle_context.hpp"

#ifndef RIVE_VULKAN

std::unique_ptr<FiddleContext> FiddleContext::MakeVulkanPLS(FiddleContextOptions options)
{
    return nullptr;
}

#else

#include "rive/pls/pls_renderer.hpp"
#include "rive/pls/vulkan/vulkan_fence_pool.hpp"
#include "rive/pls/vulkan/pls_render_context_vulkan_impl.hpp"
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include <vk_mem_alloc.h>

using namespace rive;
using namespace rive::pls;

// +1 because PLS doesn't wait for the previous fence until partway through flush.
// (After we need to acquire a new image from the swapchain.)
static constexpr int kResourcePoolSize = pls::kBufferRingSize + 1;

static const char* physical_device_type_name(VkPhysicalDeviceType type)
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

static uint32_t find_queue_family(VkPhysicalDevice physicalDevice)
{
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice,
                                             &queueFamilyCount,
                                             queueFamilyProperties.data());

    uint32_t i = 0;
    for (const auto& properties : queueFamilyProperties)
    {
        if (properties.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            return i;
        }

        i++;
    }

    abort();
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
               VkDebugUtilsMessageTypeFlagsEXT messageType,
               const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
               void* pUserData)
{
    switch (messageType)
    {
        case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
            if (pCallbackData->messageIdNumber == 0 &&
                strcmp(pCallbackData->pMessageIdName, "Loader Message") == 0 &&
                strcmp(pCallbackData->pMessage, "Copying old device 0 into new device 0") == 0)
            {
                // Swiftshader generates this error during vkEnumeratePhysicalDevices. It seems
                // fine to ignore.
                break;
            }
            fprintf(stderr,
                    "PLS Vulkan error: %i: %s: %s\n",
                    pCallbackData->messageIdNumber,
                    pCallbackData->pMessageIdName,
                    pCallbackData->pMessage);
            abort();
        case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
            fprintf(stderr,
                    "PLS Vulkan Validation error: %i: %s: %s\n",
                    pCallbackData->messageIdNumber,
                    pCallbackData->pMessageIdName,
                    pCallbackData->pMessage);
            abort();
        case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
            fprintf(stderr,
                    "PLS Vulkan Performance warning: %i: %s: %s\n",
                    pCallbackData->messageIdNumber,
                    pCallbackData->pMessageIdName,
                    pCallbackData->pMessage);
            break;
    }
    return VK_FALSE;
}

class FiddleContextVulkanPLS : public FiddleContext
{
public:
    FiddleContextVulkanPLS(FiddleContextOptions options) : m_options(options)
    {
        uint32_t instanceAvailableExtensionCount;
        vkEnumerateInstanceExtensionProperties(nullptr, &instanceAvailableExtensionCount, nullptr);

        std::vector<VkExtensionProperties> instanceAvailableExtensions(
            instanceAvailableExtensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr,
                                               &instanceAvailableExtensionCount,
                                               instanceAvailableExtensions.data());

#if 0
        for (VkExtensionProperties ext : instanceAvailableExtensions)
        {
            printf("  %s\n", ext.extensionName);
        }
#endif

        std::vector<const char*> instanceEnabledExtensions;

        if (!m_options.allowHeadlessRendering)
        {
            uint32_t glfwExtensionCount = 0;
            const char** glfwExtensions;

            glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
            instanceEnabledExtensions.insert(instanceEnabledExtensions.end(),
                                             glfwExtensions,
                                             glfwExtensions + glfwExtensionCount);
        }

        struct
        {
            bool EXT_debug_utils = false;
            bool KHR_portability_enumeration = false;
            bool KHR_get_physical_device_properties2 = false;
        } instanceExtensions;

        for (const VkExtensionProperties& ext : instanceAvailableExtensions)
        {
            if (strcmp(ext.extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0)
            {
                // Did glfw already add this extension?
                if (std::find(instanceEnabledExtensions.begin(),
                              instanceEnabledExtensions.end(),
                              VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == instanceEnabledExtensions.end())
                {
                    instanceEnabledExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
                }
                instanceExtensions.EXT_debug_utils = true;
                continue;
            }
            if (strcmp(ext.extensionName, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0)
            {
                instanceEnabledExtensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
                instanceExtensions.KHR_portability_enumeration = true;
                continue;
            }
            if (strcmp(ext.extensionName, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME) ==
                0)
            {
                instanceEnabledExtensions.push_back(
                    VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
                instanceExtensions.KHR_get_physical_device_properties2 = true;
                continue;
            }
        }

        VkApplicationInfo applicationInfo = {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pApplicationName = "Path Fiddle",
            .applicationVersion = VK_MAKE_VERSION(0, 0, 1),
            .pEngineName = "Rive Renderer",
            .engineVersion = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion = VK_API_VERSION_1_0,
        };

        VkInstanceCreateInfo instanceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pApplicationInfo = &applicationInfo,
        };

        if (options.enableVulkanValidationLayers)
        {
            if (!instanceExtensions.EXT_debug_utils)
            {
                fprintf(stderr,
                        "Validation layers requested but EXT_debug_utils is not supported.");
                exit(-1);
            }

            const char* validationLayerName = "VK_LAYER_KHRONOS_validation";

            uint32_t layerPropertiesCount;
            vkEnumerateInstanceLayerProperties(&layerPropertiesCount, nullptr);

            std::vector<VkLayerProperties> availableLayerProperties(layerPropertiesCount);
            vkEnumerateInstanceLayerProperties(&layerPropertiesCount,
                                               availableLayerProperties.data());

            bool foundValidationLayers = false;
            for (auto& layerProperties : availableLayerProperties)
            {
                if (strcmp(layerProperties.layerName, validationLayerName) == 0)
                {
                    instanceCreateInfo.enabledLayerCount = 1;
                    instanceCreateInfo.ppEnabledLayerNames = &validationLayerName;
                    printf("Vulkan validation layers enabled (%s).\n", layerProperties.layerName);
                    foundValidationLayers = true;
                    break;
                }
            }

            if (!foundValidationLayers)
            {
                fprintf(stderr,
                        "Failed to locate %s layers. Available layers:\n",
                        validationLayerName);
                if (availableLayerProperties.empty())
                {
                    fprintf(stderr, "  <none>\n");
                }
                else
                {
                    for (auto& layerProperties : availableLayerProperties)
                    {
                        fprintf(stderr, "  %s\n", layerProperties.layerName);
                    }
                }
                exit(-1);
            }
        }

        if (instanceExtensions.KHR_portability_enumeration)
        {
            instanceCreateInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
        }

        instanceCreateInfo.enabledExtensionCount = instanceEnabledExtensions.size();
        instanceCreateInfo.ppEnabledExtensionNames = instanceEnabledExtensions.data();

        VK_CHECK(vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance));

        if (options.enableVulkanValidationLayers)
        {
            // Setup a debug callback.
            VkDebugUtilsMessengerCreateInfoEXT debugCallbackCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
                .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                   VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
                .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
                .pfnUserCallback = debug_callback,
                .pUserData = nullptr,
            };

            auto vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)
                vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT");

            vkCreateDebugUtilsMessengerEXT(m_instance,
                                           &debugCallbackCreateInfo,
                                           nullptr,
                                           &m_debugMessenger);
        }

        m_physicalDevice = selectPhysicalDevice(m_options.gpuNameFilter);

        VkPhysicalDeviceProperties deviceProps;
        vkGetPhysicalDeviceProperties(m_physicalDevice, &deviceProps);
        printf("==== Vulkan Device (%s): %s ====\n",
               physical_device_type_name(deviceProps.deviceType),
               deviceProps.deviceName);

        uint32_t queueFamilyIndex = find_queue_family(m_physicalDevice);
        float queuePriority = 1.0;
        VkDeviceQueueCreateInfo deviceQueueCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = queueFamilyIndex,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority,
        };

        VkPhysicalDeviceFeatures physicalDeviceFeatures = {
            .independentBlend = VK_TRUE,
            .fragmentStoresAndAtomics = VK_TRUE,
        };

        uint32_t deviceAvailableExtensionCount;
        vkEnumerateDeviceExtensionProperties(m_physicalDevice,
                                             nullptr,
                                             &deviceAvailableExtensionCount,
                                             nullptr);

        std::vector<VkExtensionProperties> deviceAvailableExtensions(deviceAvailableExtensionCount);
        vkEnumerateDeviceExtensionProperties(m_physicalDevice,
                                             nullptr,
                                             &deviceAvailableExtensionCount,
                                             deviceAvailableExtensions.data());

#if 0
        printf("Device extensions:\n");
        for (const VkExtensionProperties& ext : deviceAvailableExtensions)
        {
            printf("  %s\n", ext.extensionName);
        }
#endif

        std::vector<const char*> deviceEnabledExtensions;

        struct
        {
            bool KHR_swapchain = false;
            bool KHR_portability_subset = false;
        } extensions;

        for (const VkExtensionProperties& ext : deviceAvailableExtensions)
        {
            if (!options.allowHeadlessRendering &&
                strcmp(ext.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0)
            {
                deviceEnabledExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
                extensions.KHR_swapchain = true;
                continue;
            }
            if (instanceExtensions.KHR_portability_enumeration &&
                strcmp(ext.extensionName, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME) == 0)
            {
                deviceEnabledExtensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
                extensions.KHR_portability_subset = true;
                continue;
            }
        }

        if (!options.allowHeadlessRendering && !extensions.KHR_swapchain)
        {
            fprintf(stderr,
                    "extension %s, required for GLFW windowed rendering, is not supported.\n",
                    VK_KHR_SWAPCHAIN_EXTENSION_NAME);
            exit(-1);
        }

        VkDeviceCreateInfo deviceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .queueCreateInfoCount = 1,
            .pQueueCreateInfos = &deviceQueueCreateInfo,
            .enabledExtensionCount = static_cast<uint32_t>(deviceEnabledExtensions.size()),
            .ppEnabledExtensionNames = deviceEnabledExtensions.data(),
            .pEnabledFeatures = &physicalDeviceFeatures,
        };

        VK_CHECK(vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &m_device));

        vkGetDeviceQueue(m_device, queueFamilyIndex, 0, &m_queue);

        VkCommandPoolCreateInfo commandPoolCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VkCommandPoolCreateFlagBits::VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = queueFamilyIndex,
        };

        VK_CHECK(vkCreateCommandPool(m_device, &commandPoolCreateInfo, nullptr, &m_commandPool));

        VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = m_commandPool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1,
        };

        for (VkCommandBuffer& commandBuffer : m_commandBuffers)
        {
            VK_CHECK(
                vkAllocateCommandBuffers(m_device, &commandBufferAllocateInfo, &commandBuffer));
        }

        m_allocator =
            make_rcp<vkutil::Allocator>(m_instance, m_physicalDevice, m_device, VK_API_VERSION_1_0);
        m_plsContext = PLSRenderContextVulkanImpl::MakeContext(m_allocator);

        VkSemaphoreCreateInfo semaphoreCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        };
        for (VkSemaphore& semaphore : m_semaphores)
        {
            VK_CHECK(vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &semaphore));
        }

        m_fencePool = make_rcp<VulkanFencePool>(m_device);
    }

    ~FiddleContextVulkanPLS()
    {
        // Destroy these before destroying the VkDevice.
        m_plsContext.reset();
        m_renderTarget.reset();
        m_pixelReadBuffer.reset();
        m_headlessRenderTexture.reset();
        m_swapchainImageViews.clear();
        m_fencePool.reset();
        m_frameFence.reset();
        m_allocator.reset();

        VK_CHECK(vkQueueWaitIdle(m_queue));

        vkFreeCommandBuffers(m_device, m_commandPool, kResourcePoolSize, m_commandBuffers);
        vkDestroyCommandPool(m_device, m_commandPool, nullptr);

        for (VkSemaphore semaphore : m_semaphores)
        {
            vkDestroySemaphore(m_device, semaphore, nullptr);
        }

        if (m_swapchain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
        }

        if (m_windowSurface != VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(m_instance, m_windowSurface, nullptr);
        }

        if (m_debugMessenger != nullptr)
        {
            auto vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)
                vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT");
            vkDestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
        }

        vkDestroyDevice(m_device, nullptr);
        vkDestroyInstance(m_instance, nullptr);
    }

    VkPhysicalDevice selectPhysicalDevice(const char* filter)
    {
        if (filter == nullptr)
        {
            filter = getenv("RIVE_GPU");
        }

        uint32_t physicalDeviceCount = 0;
        vkEnumeratePhysicalDevices(m_instance, &physicalDeviceCount, nullptr);

        std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
        vkEnumeratePhysicalDevices(m_instance, &physicalDeviceCount, physicalDevices.data());

        if (physicalDevices.size() == 1 && filter == nullptr)
        {
            return physicalDevices[0];
        }

        std::vector<VkPhysicalDevice> matches;
        for (size_t i = 0; i < physicalDevices.size(); ++i)
        {
            VkPhysicalDeviceProperties deviceProps;
            vkGetPhysicalDeviceProperties(physicalDevices[i], &deviceProps);
            if (filter != nullptr)
            {
                if (strstr(deviceProps.deviceName, filter) != nullptr)
                {
                    matches.push_back(physicalDevices[i]);
                }
            }
            else if (deviceProps.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            {
                // Prefer a discrete GPU if no filter.
                matches.push_back(physicalDevices[i]);
            }
        }
        if (matches.size() != 1)
        {
            const char* filterName = filter != nullptr ? filter : "<discrete_gpu>";
            const std::vector<VkPhysicalDevice>* devicePrintList;
            if (matches.size() > 1)
            {
                fprintf(stderr,
                        "Cannot select GPU\nToo many matches for filter '%s'.\nMatches:\n",
                        filterName);
                devicePrintList = &matches;
            }
            else
            {
                fprintf(stderr,
                        "Cannot select GPU.\nNo matches for filter '%s'.\nAvailable GPUs:\n",
                        filterName);
                devicePrintList = &physicalDevices;
            }
            for (VkPhysicalDevice physicalDevice : *devicePrintList)
            {
                VkPhysicalDeviceProperties deviceProps;
                vkGetPhysicalDeviceProperties(physicalDevice, &deviceProps);
                fprintf(stderr,
                        "  %s [%s]\n",
                        deviceProps.deviceName,
                        physical_device_type_name(deviceProps.deviceType));
            }
            fprintf(stderr, "\nPlease update the $RIVE_GPU environment variable\n");
            exit(-1);
        }
        return matches[0];
    }

    float dpiScale(GLFWwindow* window) const override { return 1.0; }

    Factory* factory() override { return m_plsContext.get(); }

    rive::pls::PLSRenderContext* plsContextOrNull() override { return m_plsContext.get(); }

    rive::pls::PLSRenderTarget* plsRenderTargetOrNull() override { return m_renderTarget.get(); }

    void onSizeChanged(GLFWwindow* window, int width, int height, uint32_t sampleCount) override
    {
        if (m_options.allowHeadlessRendering)
        {
            m_swapchainFormat = VK_FORMAT_R8G8B8A8_UNORM;
            m_swapchainImages.resize(1);
            m_headlessRenderTexture = m_allocator->makeTexture({
                .format = m_swapchainFormat,
                .extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1},
                .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT |
                         VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            });
            assert(m_options.enableReadPixels);
            m_swapchainImages = {*m_headlessRenderTexture};
            assert(m_swapchain == VK_NULL_HANDLE);
        }
        else
        {
            VK_CHECK(vkQueueWaitIdle(m_queue));

            if (m_swapchain != VK_NULL_HANDLE)
            {
                vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
            }

            if (m_windowSurface != VK_NULL_HANDLE)
            {
                vkDestroySurfaceKHR(m_instance, m_windowSurface, nullptr);
            }

            VK_CHECK(glfwCreateWindowSurface(m_instance, window, nullptr, &m_windowSurface));

            VkSurfaceCapabilitiesKHR surfaceCapabilities;
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice,
                                                      m_windowSurface,
                                                      &surfaceCapabilities);

            uint32_t formatCount;
            vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice,
                                                 m_windowSurface,
                                                 &formatCount,
                                                 nullptr);

            std::vector<VkSurfaceFormatKHR> formats(formatCount);

            vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice,
                                                 m_windowSurface,
                                                 &formatCount,
                                                 formats.data());

            int formatIndex = -1;
            for (int i = 0; i < formats.size(); i++)
            {
                if (formats[i].format != VK_FORMAT_B8G8R8A8_UNORM &&
                    formats[i].format != VK_FORMAT_R8G8B8A8_UNORM)
                {
                    continue;
                }
                if (formats[i].colorSpace != VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
                {
                    continue;
                }
                formatIndex = i;
                break;
            }
            if (formatIndex == -1)
            {
                fprintf(stderr, "No supported physical device surface format.");
                exit(-1);
            }

            uint32_t presentationModeCount;
            vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice,
                                                      m_windowSurface,
                                                      &presentationModeCount,
                                                      nullptr);

            std::vector<VkPresentModeKHR> presentationModes(presentationModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice,
                                                      m_windowSurface,
                                                      &presentationModeCount,
                                                      presentationModes.data());

            VkPresentModeKHR presentationMode = VK_PRESENT_MODE_FIFO_KHR; // FIFO is required.
            for (VkPresentModeKHR priorityMode : {VK_PRESENT_MODE_IMMEDIATE_KHR,
                                                  VK_PRESENT_MODE_MAILBOX_KHR,
                                                  VK_PRESENT_MODE_FIFO_RELAXED_KHR})
            {
                if (std::find(presentationModes.begin(), presentationModes.end(), priorityMode) !=
                    presentationModes.end())
                {
                    presentationMode = priorityMode;
                    break;
                }
            }

            VkSwapchainCreateInfoKHR swapchainCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                .surface = m_windowSurface,
                .minImageCount = surfaceCapabilities.minImageCount + 1,
                .imageFormat = formats[formatIndex].format,
                .imageColorSpace = formats[formatIndex].colorSpace,
                .imageExtent = surfaceCapabilities.currentExtent,
                .imageArrayLayers = 1,
                .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
                .preTransform = surfaceCapabilities.currentTransform,
                .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                .presentMode = presentationMode,
                .clipped = VK_TRUE,
            };
            if (m_options.enableReadPixels)
            {
                // Needed for readbacks and for framebuffer preservation with blend
                // modes enabled.
                if (!(surfaceCapabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT))
                {
                    fprintf(stderr, "Readbacks not supported on GLFW window.");
                }
                swapchainCreateInfo.imageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            }

            VK_CHECK(vkCreateSwapchainKHR(m_device, &swapchainCreateInfo, nullptr, &m_swapchain));

            m_swapchainFormat = formats[formatIndex].format;

            uint32_t imageCount;
            vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, nullptr);

            m_swapchainImages.resize(imageCount);
            vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, m_swapchainImages.data());
        }

        m_swapchainImageViews.clear();
        m_swapchainImageViews.reserve(m_swapchainImages.size());
        for (VkImage image : m_swapchainImages)
        {
            m_swapchainImageViews.push_back(
                m_allocator->makeTextureView(nullptr,
                                             {
                                                 .image = image,
                                                 .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                                 .format = m_swapchainFormat,
                                                 .subresourceRange =
                                                     {
                                                         .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                                         .levelCount = 1,
                                                         .layerCount = 1,
                                                     },
                                             }));
        }

        m_swapchainImageLayouts = std::vector(m_swapchainImages.size(), VK_IMAGE_LAYOUT_UNDEFINED);

        m_swapchainImageIndex = 0;

        m_renderTarget = plsImplVulkan()->makeRenderTarget(width, height, m_swapchainFormat);

        m_pixelReadBuffer = nullptr;
    }

    void toggleZoomWindow() override {}

    std::unique_ptr<Renderer> makeRenderer(int width, int height) override
    {
        return std::make_unique<PLSRenderer>(m_plsContext.get());
    }

    void begin(const PLSRenderContext::FrameDescriptor& frameDescriptor) override
    {
        if (!m_options.allowHeadlessRendering)
        {
            vkAcquireNextImageKHR(m_device,
                                  m_swapchain,
                                  UINT64_MAX,
                                  m_semaphores[m_resourcePoolIdx],
                                  VK_NULL_HANDLE,
                                  &m_swapchainImageIndex);
        }

        m_renderTarget->setTargetTextureView(m_swapchainImageViews[m_swapchainImageIndex]);
        m_plsContext->beginFrame(std::move(frameDescriptor));

        VkCommandBuffer commandBuffer = m_commandBuffers[m_resourcePoolIdx];
        vkResetCommandBuffer(commandBuffer, {});

        VkCommandBufferBeginInfo commandBufferBeginInfo = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        };

        vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo);

        // TODO: consider using GENERAL only in atomic case.
        vkutil::insert_image_memory_barrier(commandBuffer,
                                            m_swapchainImages[m_swapchainImageIndex],
                                            m_swapchainImageLayouts[m_swapchainImageIndex],
                                            VK_IMAGE_LAYOUT_GENERAL);

        m_swapchainImageLayouts[m_swapchainImageIndex] = VK_IMAGE_LAYOUT_GENERAL;

        m_frameFence = m_fencePool->makeFence();
    }

    void flushPLSContext() final
    {
        m_plsContext->flush({
            .renderTarget = m_renderTarget.get(),
            .externalCommandBuffer = m_commandBuffers[m_resourcePoolIdx],
            .frameCompletionFence = m_frameFence.get(),
        });
    }

    void end(GLFWwindow* window, std::vector<uint8_t>* pixelData) final
    {
        flushPLSContext();

        VkCommandBuffer commandBuffer = m_commandBuffers[m_resourcePoolIdx];
        uint32_t w = m_renderTarget->width();
        uint32_t h = m_renderTarget->height();

        if (pixelData != nullptr)
        {
            // Copy the framebuffer out to a buffer.
            if (m_pixelReadBuffer == nullptr)
            {
                m_pixelReadBuffer = m_allocator->makeBuffer(
                    {
                        .size = h * w * 4,
                        .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                    },
                    vkutil::Mappability::readWrite);
            }
            assert(m_pixelReadBuffer->info().size == h * w * 4);

            VkBufferImageCopy imageCopyDesc = {
                .imageSubresource =
                    {
                        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                        .mipLevel = 0,
                        .baseArrayLayer = 0,
                        .layerCount = 1,
                    },
                .imageExtent = {w, h, 1},
            };

            vkutil::insert_image_memory_barrier(commandBuffer,
                                                m_swapchainImages[m_swapchainImageIndex],
                                                m_swapchainImageLayouts[m_swapchainImageIndex],
                                                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

            m_swapchainImageLayouts[m_swapchainImageIndex] = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

            vkCmdCopyImageToBuffer(commandBuffer,
                                   m_swapchainImages[m_swapchainImageIndex],
                                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                   *m_pixelReadBuffer,
                                   1,
                                   &imageCopyDesc);

            vkutil::insert_buffer_memory_barrier(commandBuffer,
                                                 VK_ACCESS_TRANSFER_WRITE_BIT,
                                                 VK_ACCESS_HOST_READ_BIT,
                                                 *m_pixelReadBuffer);
        }

        if (!m_options.allowHeadlessRendering)
        {
            vkutil::insert_image_memory_barrier(commandBuffer,
                                                m_swapchainImages[m_swapchainImageIndex],
                                                m_swapchainImageLayouts[m_swapchainImageIndex],
                                                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

            m_swapchainImageLayouts[m_swapchainImageIndex] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        }

        VK_CHECK(vkEndCommandBuffer(commandBuffer));

        VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

        VkSubmitInfo submitInfo = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .pWaitDstStageMask = &waitDstStageMask,
            .commandBufferCount = 1,
            .pCommandBuffers = &commandBuffer,
        };
        if (!m_options.allowHeadlessRendering)
        {
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores = &m_semaphores[m_resourcePoolIdx];
        }

        VK_CHECK(vkQueueSubmit(m_queue, 1, &submitInfo, *m_frameFence));

        if (pixelData != nullptr)
        {
            // Copy the buffer containing the framebuffer contents to pixelData.
            pixelData->resize(h * w * 4);

            // Wait for all rendering to complete before transferring the
            // framebuffer data to pixelData.
            m_frameFence->wait();

            assert(m_pixelReadBuffer->info().size == h * w * 4);
            for (uint32_t y = 0; y < h; ++y)
            {
                auto row = static_cast<const char*>(m_pixelReadBuffer->contents()) + w * 4 * y;
                memcpy(pixelData->data() + (h - y - 1) * w * 4, row, w * 4);
            }
        }

        if (!m_options.allowHeadlessRendering)
        {
            VkPresentInfoKHR presentInfo = {
                .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                .swapchainCount = 1,
                .pSwapchains = &m_swapchain,
                .pImageIndices = &m_swapchainImageIndex,
            };

            vkQueuePresentKHR(m_queue, &presentInfo);
        }

        m_resourcePoolIdx = (m_resourcePoolIdx + 1) % kResourcePoolSize;
        m_frameFence = nullptr;
    }

private:
    PLSRenderContextVulkanImpl* plsImplVulkan() const
    {
        return m_plsContext->static_impl_cast<PLSRenderContextVulkanImpl>();
    }

    const FiddleContextOptions m_options;
    VkInstance m_instance;
    VkDebugUtilsMessengerEXT m_debugMessenger = nullptr;
    VkPhysicalDevice m_physicalDevice;
    VkDevice m_device;
    VkQueue m_queue;

    VkFormat m_swapchainFormat = VK_FORMAT_UNDEFINED;
    VkSurfaceKHR m_windowSurface = VK_NULL_HANDLE;
    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> m_swapchainImages;
    std::vector<rcp<vkutil::TextureView>> m_swapchainImageViews;
    std::vector<VkImageLayout> m_swapchainImageLayouts;
    uint32_t m_swapchainImageIndex;

    // Headless rendering.
    rcp<vkutil::Texture> m_headlessRenderTexture;

    VkCommandPool m_commandPool;
    VkCommandBuffer m_commandBuffers[kResourcePoolSize];
    VkSemaphore m_semaphores[kResourcePoolSize];

    rcp<VulkanFencePool> m_fencePool;
    rcp<VulkanFence> m_frameFence;

    rcp<vkutil::Allocator> m_allocator;
    std::unique_ptr<PLSRenderContext> m_plsContext;
    rcp<PLSRenderTargetVulkan> m_renderTarget;
    rcp<vkutil::Buffer> m_pixelReadBuffer;

    int m_resourcePoolIdx = 0;
};

std::unique_ptr<FiddleContext> FiddleContext::MakeVulkanPLS(FiddleContextOptions options)
{
    return std::make_unique<FiddleContextVulkanPLS>(options);
}

#endif
