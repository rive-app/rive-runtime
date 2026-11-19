/*
 * Copyright 2026 Rive
 */

// Creates a VulkanSwapchain against a stand-in loader that counts surface
// format queries and records the format the swapchain was created with.
//
// Only built with --with_vulkan; see test.sh.

#ifdef RIVE_VULKAN

#include "rive_vk_bootstrap/vulkan_device.hpp"
#include "rive_vk_bootstrap/vulkan_instance.hpp"
#include "rive_vk_bootstrap/vulkan_swapchain.hpp"
#include "../../../renderer/rive_vk_bootstrap/src/vulkan_library.hpp"

#include <catch.hpp>
#include <algorithm>
#include <optional>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

namespace rive_vkb
{
namespace
{
constexpr VkSurfaceFormatKHR kRGBA = {
    .format = VK_FORMAT_R8G8B8A8_UNORM,
    .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
};
constexpr VkSurfaceFormatKHR kBGRA = {
    .format = VK_FORMAT_B8G8R8A8_UNORM,
    .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
};

struct
{
  std::vector<VkSurfaceFormatKHR> surfaceFormats;
  int surfaceFormatQueries = 0;
  std::optional<VkFormat> swapchainFormat;
} g_loader;

uint64_t g_nextHandle = 0x1000;

template <typename Handle> Handle newHandle()
{
  return (Handle)(uintptr_t)(g_nextHandle += 0x10);
}

template <typename T>
VkResult fill(const std::vector<T>& available, uint32_t* count, T* out)
{
  if (out == nullptr)
  {
    *count = uint32_t(available.size());
    return VK_SUCCESS;
  }
  *count = std::min(*count, uint32_t(available.size()));
  std::copy(available.begin(), available.begin() + *count, out);
  return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL fake_vkEnumerateInstanceExtensionProperties(
    const char*,
    uint32_t* count,
    VkExtensionProperties* props)
{
  VkExtensionProperties surface{};
  strncpy(
      surface.extensionName,
      VK_KHR_SURFACE_EXTENSION_NAME,
      sizeof(surface.extensionName));
  return fill(std::vector<VkExtensionProperties>{surface}, count, props);
}

VKAPI_ATTR VkResult VKAPI_CALL
fake_vkEnumerateInstanceLayerProperties(uint32_t* count, VkLayerProperties*)
{
  *count = 0;
  return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL fake_vkEnumerateInstanceVersion(uint32_t* v)
{
  *v = VK_API_VERSION_1_1;
  return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL fake_vkCreateInstance(
    const VkInstanceCreateInfo*,
    const VkAllocationCallbacks*,
    VkInstance* instance)
{
  *instance = newHandle<VkInstance>();
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL
fake_vkDestroyInstance(VkInstance, const VkAllocationCallbacks*)
{}

VKAPI_ATTR VkResult VKAPI_CALL fake_vkEnumeratePhysicalDevices(
    VkInstance,
    uint32_t* count,
    VkPhysicalDevice* devices)
{
  if (devices != nullptr)
  {
    devices[0] = (VkPhysicalDevice)(uintptr_t)0x2000;
  }
  *count = 1;
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL fake_vkGetPhysicalDeviceProperties(
    VkPhysicalDevice,
    VkPhysicalDeviceProperties* p)
{
  memset(p, 0, sizeof(*p));
  p->apiVersion = VK_API_VERSION_1_1;
  p->deviceType = VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
  strncpy(p->deviceName, "Rive fake driver", sizeof(p->deviceName));
}

VKAPI_ATTR void VKAPI_CALL
fake_vkGetPhysicalDeviceFeatures(VkPhysicalDevice, VkPhysicalDeviceFeatures* f)
{
  memset(f, 0, sizeof(*f));
}

VKAPI_ATTR void VKAPI_CALL fake_vkGetPhysicalDeviceQueueFamilyProperties(
    VkPhysicalDevice,
    uint32_t* count,
    VkQueueFamilyProperties* props)
{
  if (props != nullptr)
  {
    props[0] = {.queueFlags = VK_QUEUE_GRAPHICS_BIT, .queueCount = 1};
  }
  *count = 1;
}

VKAPI_ATTR VkResult VKAPI_CALL fake_vkEnumerateDeviceExtensionProperties(
    VkPhysicalDevice,
    const char*,
    uint32_t* count,
    VkExtensionProperties* props)
{
  VkExtensionProperties swapchain{};
  strncpy(
      swapchain.extensionName,
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
      sizeof(swapchain.extensionName));
  return fill(std::vector<VkExtensionProperties>{swapchain}, count, props);
}

VKAPI_ATTR VkResult VKAPI_CALL fake_vkCreateDevice(
    VkPhysicalDevice,
    const VkDeviceCreateInfo*,
    const VkAllocationCallbacks*,
    VkDevice* device)
{
  *device = newHandle<VkDevice>();
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL
fake_vkDestroyDevice(VkDevice, const VkAllocationCallbacks*)
{}

VKAPI_ATTR VkResult VKAPI_CALL fake_vkDeviceWaitIdle(VkDevice)
{
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL
fake_vkGetDeviceQueue(VkDevice, uint32_t, uint32_t, VkQueue* queue)
{
  *queue = newHandle<VkQueue>();
}

VKAPI_ATTR VkResult VKAPI_CALL fake_vkGetPhysicalDeviceSurfaceFormatsKHR(
    VkPhysicalDevice,
    VkSurfaceKHR,
    uint32_t* count,
    VkSurfaceFormatKHR* formats)
{
  ++g_loader.surfaceFormatQueries;
  return fill(g_loader.surfaceFormats, count, formats);
}

VKAPI_ATTR VkResult VKAPI_CALL fake_vkGetPhysicalDeviceSurfacePresentModesKHR(
    VkPhysicalDevice,
    VkSurfaceKHR,
    uint32_t* count,
    VkPresentModeKHR* modes)
{
  return fill(
      std::vector<VkPresentModeKHR>{VK_PRESENT_MODE_FIFO_KHR},
      count,
      modes);
}

VKAPI_ATTR VkResult VKAPI_CALL fake_vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
    VkPhysicalDevice,
    VkSurfaceKHR,
    VkSurfaceCapabilitiesKHR* caps)
{
  memset(caps, 0, sizeof(*caps));
  caps->minImageCount = 2;
  caps->maxImageCount = 3;
  caps->currentExtent = {64, 64};
  caps->supportedTransforms = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  caps->currentTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  caps->supportedUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL fake_vkCreateSwapchainKHR(
    VkDevice,
    const VkSwapchainCreateInfoKHR* createInfo,
    const VkAllocationCallbacks*,
    VkSwapchainKHR* swapchain)
{
  g_loader.swapchainFormat = createInfo->imageFormat;
  *swapchain = newHandle<VkSwapchainKHR>();
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL fake_vkDestroySwapchainKHR(
    VkDevice,
    VkSwapchainKHR,
    const VkAllocationCallbacks*)
{}

VKAPI_ATTR VkResult VKAPI_CALL fake_vkGetSwapchainImagesKHR(
    VkDevice,
    VkSwapchainKHR,
    uint32_t* count,
    VkImage* images)
{
  if (images != nullptr)
  {
    for (uint32_t i = 0; i < *count; ++i)
    {
      images[i] = newHandle<VkImage>();
    }
  }
  *count = 2;
  return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL fake_vkCreateImageView(
    VkDevice,
    const VkImageViewCreateInfo*,
    const VkAllocationCallbacks*,
    VkImageView* view)
{
  *view = newHandle<VkImageView>();
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL
fake_vkDestroyImageView(VkDevice, VkImageView, const VkAllocationCallbacks*)
{}

VKAPI_ATTR VkResult VKAPI_CALL fake_vkCreateCommandPool(
    VkDevice,
    const VkCommandPoolCreateInfo*,
    const VkAllocationCallbacks*,
    VkCommandPool* pool)
{
  *pool = newHandle<VkCommandPool>();
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL
fake_vkDestroyCommandPool(VkDevice, VkCommandPool, const VkAllocationCallbacks*)
{}

VKAPI_ATTR VkResult VKAPI_CALL fake_vkCreateFence(
    VkDevice,
    const VkFenceCreateInfo*,
    const VkAllocationCallbacks*,
    VkFence* fence)
{
  *fence = newHandle<VkFence>();
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL
fake_vkDestroyFence(VkDevice, VkFence, const VkAllocationCallbacks*)
{}

VKAPI_ATTR VkResult VKAPI_CALL fake_vkCreateSemaphore(
    VkDevice,
    const VkSemaphoreCreateInfo*,
    const VkAllocationCallbacks*,
    VkSemaphore* semaphore)
{
  *semaphore = newHandle<VkSemaphore>();
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL
fake_vkDestroySemaphore(VkDevice, VkSemaphore, const VkAllocationCallbacks*)
{}

VKAPI_ATTR VkResult VKAPI_CALL fake_vkAllocateCommandBuffers(
    VkDevice,
    const VkCommandBufferAllocateInfo*,
    VkCommandBuffer* buffers)
{
  buffers[0] = newHandle<VkCommandBuffer>();
  return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL fake_vkFreeCommandBuffers(
    VkDevice,
    VkCommandPool,
    uint32_t,
    const VkCommandBuffer*)
{}

// Loaded by the swapchain but not called while it is created or destroyed.
VKAPI_ATTR void VKAPI_CALL fake_unused()
{
  fprintf(stderr, "Unexpected Vulkan call in the stand-in loader\n");
  abort();
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
fake_vkGetDeviceProcAddr(VkDevice, const char* name)
{
  if (strcmp(name, "vkDestroyDevice") == 0)
  {
    return reinterpret_cast<PFN_vkVoidFunction>(fake_vkDestroyDevice);
  }
  if (strcmp(name, "vkGetDeviceQueue") == 0)
  {
    return reinterpret_cast<PFN_vkVoidFunction>(fake_vkGetDeviceQueue);
  }
  return nullptr;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
fake_vkGetInstanceProcAddr(VkInstance, const char* name)
{
#define ENTRY(fn)                                                              \
  if (strcmp(name, #fn) == 0)                                                  \
  {                                                                            \
    return reinterpret_cast<PFN_vkVoidFunction>(fake_##fn);                    \
  }
#define UNUSED(fn)                                                             \
  if (strcmp(name, #fn) == 0)                                                  \
  {                                                                            \
    return reinterpret_cast<PFN_vkVoidFunction>(fake_unused);                  \
  }

  ENTRY(vkEnumerateInstanceExtensionProperties)
  ENTRY(vkEnumerateInstanceLayerProperties)
  ENTRY(vkEnumerateInstanceVersion)
  ENTRY(vkCreateInstance)
  ENTRY(vkDestroyInstance)
  ENTRY(vkEnumeratePhysicalDevices)
  ENTRY(vkGetPhysicalDeviceProperties)
  ENTRY(vkGetPhysicalDeviceFeatures)
  ENTRY(vkGetPhysicalDeviceQueueFamilyProperties)
  ENTRY(vkEnumerateDeviceExtensionProperties)
  ENTRY(vkCreateDevice)
  ENTRY(vkGetDeviceProcAddr)
  ENTRY(vkDeviceWaitIdle)
  ENTRY(vkGetDeviceQueue)
  ENTRY(vkGetPhysicalDeviceSurfaceFormatsKHR)
  ENTRY(vkGetPhysicalDeviceSurfacePresentModesKHR)
  ENTRY(vkGetPhysicalDeviceSurfaceCapabilitiesKHR)
  ENTRY(vkCreateSwapchainKHR)
  ENTRY(vkDestroySwapchainKHR)
  ENTRY(vkGetSwapchainImagesKHR)
  ENTRY(vkCreateImageView)
  ENTRY(vkDestroyImageView)
  ENTRY(vkCreateCommandPool)
  ENTRY(vkDestroyCommandPool)
  ENTRY(vkCreateFence)
  ENTRY(vkDestroyFence)
  ENTRY(vkCreateSemaphore)
  ENTRY(vkDestroySemaphore)
  ENTRY(vkAllocateCommandBuffers)
  ENTRY(vkFreeCommandBuffers)
  UNUSED(vkWaitForFences)
  UNUSED(vkGetFenceStatus)
  UNUSED(vkCmdPipelineBarrier)
  UNUSED(vkQueueSubmit)
  UNUSED(vkResetFences)
  UNUSED(vkResetCommandBuffer)
  UNUSED(vkBeginCommandBuffer)
  UNUSED(vkEndCommandBuffer)
  UNUSED(vkCmdCopyImageToBuffer)
  UNUSED(vkAcquireNextImageKHR)
  UNUSED(vkQueuePresentKHR)
#undef UNUSED
#undef ENTRY
  return nullptr;
}

void setRiveGpu(const char* value)
{
#ifdef _WIN32
  // An empty value removes the variable.
  _putenv_s("RIVE_GPU", value != nullptr ? value : "");
#else
  if (value != nullptr)
  {
    setenv("RIVE_GPU", value, 1);
  }
  else
  {
    unsetenv("RIVE_GPU");
  }
#endif
}

const char* g_requiredExtensions[] = {VK_KHR_SURFACE_EXTENSION_NAME};

// Owns what a swapchain needs, destroying it in reverse order.
struct SwapchainFixture
{
  std::unique_ptr<VulkanInstance> instance;
  std::unique_ptr<VulkanDevice> device;
  std::unique_ptr<VulkanSwapchain> swapchain;
};

// Installs the stand-in loader, then creates a swapchain for a surface that
// lists only `surfaceFormats`.
SwapchainFixture createSwapchain(
    std::vector<VkSurfaceFormatKHR> surfaceFormats,
    VulkanSwapchain::Options opts)
{
  g_loader = {};
  g_loader.surfaceFormats = std::move(surfaceFormats);

  // A RIVE_GPU filter from the environment wouldn't match the fake device.
  std::optional<std::string> riveGpu;
  if (const char* value = getenv("RIVE_GPU"))
  {
    riveGpu = value;
    setRiveGpu(nullptr);
  }

  SwapchainFixture fixture;
  g_testingVkGetInstanceProcAddr = fake_vkGetInstanceProcAddr;
  fixture.instance = VulkanInstance::Create({
      .requiredExtensions = rive::make_span(g_requiredExtensions, 1),
      .desiredValidationType = VulkanValidationType::none,
      .wantDebugCallbacks = false,
  });
  // Only creating the instance reads the hook, so later tests get the real
  // loader even if a REQUIRE below fails.
  g_testingVkGetInstanceProcAddr = nullptr;
  if (fixture.instance != nullptr)
  {
    fixture.device = VulkanDevice::Create(
        *fixture.instance,
        {.printInitializationMessage = false});
  }
  if (riveGpu.has_value())
  {
    setRiveGpu(riveGpu->c_str());
  }
  REQUIRE(fixture.device != nullptr);

  g_loader.surfaceFormatQueries = 0;
  opts.presentModePreferences = {VK_PRESENT_MODE_FIFO_KHR};
  fixture.swapchain = VulkanSwapchain::Create(
      *fixture.instance,
      *fixture.device,
      nullptr,
      (VkSurfaceKHR)(uintptr_t)0x3000,
      opts);
  return fixture;
}
} // namespace

TEST_CASE(
    "VulkanSwapchain uses the first preferred format without a query when told to",
    "[vulkan_bootstrap]")
{
  // The surface lists only BGRA, so RGBA here means the query never ran.
  auto fixture = createSwapchain(
      {kBGRA},
      {.formatPreferences = {kRGBA, kBGRA},
       .assumeFirstSurfaceFormatIsValid = true});

  REQUIRE(fixture.swapchain != nullptr);
  CHECK(g_loader.surfaceFormatQueries == 0);
  CHECK(g_loader.swapchainFormat == kRGBA.format);
}

TEST_CASE(
    "VulkanSwapchain picks the first preferred format the surface lists",
    "[vulkan_bootstrap]")
{
  auto fixture = createSwapchain(
      {kBGRA},
      {.formatPreferences = {kRGBA, kBGRA}});

  REQUIRE(fixture.swapchain != nullptr);
  CHECK(g_loader.surfaceFormatQueries > 0);
  CHECK(g_loader.swapchainFormat == kBGRA.format);
}
} // namespace rive_vkb

#endif // RIVE_VULKAN
