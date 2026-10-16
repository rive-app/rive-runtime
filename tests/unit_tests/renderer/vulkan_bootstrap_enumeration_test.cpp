/*
 * Copyright 2026 Rive
 */

// Runs VulkanInstance and VulkanDevice creation against a stand-in loader whose
// enumeration commands fail on demand. Android 10's loader fails
// vkEnumerateInstanceExtensionProperties with VK_ERROR_INITIALIZATION_FAILED
// when the Vulkan HAL won't open, and leaves *pPropertyCount untouched, so the
// stand-ins below never write the count when they fail.
//
// Only built with --with_vulkan; see test.sh.

#ifdef RIVE_VULKAN

#include "rive_vk_bootstrap/vulkan_device.hpp"
#include "rive_vk_bootstrap/vulkan_instance.hpp"
#include "../../../renderer/rive_vk_bootstrap/src/vulkan_library.hpp"

#include <catch.hpp>
#include <algorithm>
#include <optional>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <tuple>
#include <vector>

#if defined(_MSC_VER)
#define VKB_NEVER_INLINE __declspec(noinline)
#else
#define VKB_NEVER_INLINE __attribute__((noinline))
#endif

namespace rive_vkb
{
namespace
{
struct EnumerationCall
{
    std::string command;
    bool queriedCount; // pProperties was null.
};

struct
{
    // Index into `calls` of the enumeration that fails, or -1 for none.
    int failingCall = -1;
    VkResult failure = VK_ERROR_INITIALIZATION_FAILED;
    std::vector<EnumerationCall> calls;
    bool createdInstance = false;
    bool createdDevice = false;
    std::vector<std::string> enabledDeviceExtensions;
} g_loader;

template <typename Properties>
VkResult enumerate(const char* command,
                   const std::vector<Properties>& available,
                   uint32_t* pPropertyCount,
                   Properties* pProperties)
{
    const bool fail = int(g_loader.calls.size()) == g_loader.failingCall;
    g_loader.calls.push_back({command, pProperties == nullptr});
    // VK_INCOMPLETE is a partial success: the loader still fills the array.
    if (fail && g_loader.failure != VK_INCOMPLETE)
    {
        return g_loader.failure;
    }
    if (pProperties == nullptr)
    {
        *pPropertyCount = uint32_t(available.size());
        return VK_SUCCESS;
    }
    const uint32_t copied =
        std::min(*pPropertyCount, uint32_t(available.size()));
    std::copy(available.begin(), available.begin() + copied, pProperties);
    *pPropertyCount = copied;
    // A failing VK_INCOMPLETE stands in for the list growing between calls.
    return fail || copied < available.size() ? VK_INCOMPLETE : VK_SUCCESS;
}

std::vector<VkExtensionProperties> extensions(
    std::initializer_list<const char*> names)
{
    std::vector<VkExtensionProperties> props;
    for (auto* name : names)
    {
        VkExtensionProperties prop{};
        strncpy(prop.extensionName, name, sizeof(prop.extensionName));
        props.push_back(prop);
    }
    return props;
}

VKAPI_ATTR VkResult VKAPI_CALL
fake_vkEnumerateInstanceExtensionProperties(const char*,
                                            uint32_t* pPropertyCount,
                                            VkExtensionProperties* pProperties)
{
    return enumerate("vkEnumerateInstanceExtensionProperties",
                     extensions({VK_KHR_SURFACE_EXTENSION_NAME}),
                     pPropertyCount,
                     pProperties);
}

VKAPI_ATTR VkResult VKAPI_CALL
fake_vkEnumerateInstanceLayerProperties(uint32_t* pPropertyCount,
                                        VkLayerProperties* pProperties)
{
    // Non-empty, so the properties call gets a non-null array.
    VkLayerProperties layer{};
    strncpy(layer.layerName, "VK_LAYER_rive_fake", sizeof(layer.layerName));
    return enumerate("vkEnumerateInstanceLayerProperties",
                     std::vector<VkLayerProperties>{layer},
                     pPropertyCount,
                     pProperties);
}

VKAPI_ATTR VkResult VKAPI_CALL
fake_vkEnumerateDeviceExtensionProperties(VkPhysicalDevice,
                                          const char*,
                                          uint32_t* pPropertyCount,
                                          VkExtensionProperties* pProperties)
{
    return enumerate("vkEnumerateDeviceExtensionProperties",
                     extensions({VK_KHR_SWAPCHAIN_EXTENSION_NAME}),
                     pPropertyCount,
                     pProperties);
}

VKAPI_ATTR VkResult VKAPI_CALL fake_vkEnumerateInstanceVersion(uint32_t* v)
{
    *v = VK_API_VERSION_1_1;
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
fake_vkCreateInstance(const VkInstanceCreateInfo*,
                      const VkAllocationCallbacks*,
                      VkInstance* instance)
{
    g_loader.createdInstance = true;
    *instance = reinterpret_cast<VkInstance>(uintptr_t(0x1000));
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL
fake_vkEnumeratePhysicalDevices(VkInstance,
                                uint32_t* count,
                                VkPhysicalDevice* devices)
{
    if (devices != nullptr)
    {
        devices[0] = reinterpret_cast<VkPhysicalDevice>(uintptr_t(0x2000));
    }
    *count = 1;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL
fake_vkGetPhysicalDeviceProperties(VkPhysicalDevice,
                                   VkPhysicalDeviceProperties* props)
{
    memset(props, 0, sizeof(*props));
    props->apiVersion = VK_API_VERSION_1_1;
    props->deviceType = VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
    strncpy(props->deviceName, "Rive fake driver", sizeof(props->deviceName));
}

VKAPI_ATTR void VKAPI_CALL
fake_vkGetPhysicalDeviceFeatures(VkPhysicalDevice,
                                 VkPhysicalDeviceFeatures* features)
{
    memset(features, 0, sizeof(*features));
}

VKAPI_ATTR void VKAPI_CALL
fake_vkGetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice,
                                              uint32_t* count,
                                              VkQueueFamilyProperties* props)
{
    if (props != nullptr)
    {
        props[0] = {.queueFlags = VK_QUEUE_GRAPHICS_BIT, .queueCount = 1};
    }
    *count = 1;
}

VKAPI_ATTR VkResult VKAPI_CALL
fake_vkCreateDevice(VkPhysicalDevice,
                    const VkDeviceCreateInfo* createInfo,
                    const VkAllocationCallbacks*,
                    VkDevice*)
{
    g_loader.enabledDeviceExtensions.assign(
        createInfo->ppEnabledExtensionNames,
        createInfo->ppEnabledExtensionNames +
            createInfo->enabledExtensionCount);
    // Nothing here needs a device to exist, only to know it was asked for.
    g_loader.createdDevice = true;
    return VK_ERROR_INITIALIZATION_FAILED;
}

VKAPI_ATTR void VKAPI_CALL fake_vkDestroyInstance(VkInstance,
                                                  const VkAllocationCallbacks*)
{}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
fake_vkGetInstanceProcAddr(VkInstance, const char* name)
{
#define ENTRY(fn)                                                              \
    if (strcmp(name, #fn) == 0)                                                \
    {                                                                          \
        return reinterpret_cast<PFN_vkVoidFunction>(fake_##fn);                \
    }

    ENTRY(vkEnumerateInstanceExtensionProperties)
    ENTRY(vkEnumerateInstanceLayerProperties)
    ENTRY(vkEnumerateDeviceExtensionProperties)
    ENTRY(vkEnumerateInstanceVersion)
    ENTRY(vkCreateInstance)
    ENTRY(vkEnumeratePhysicalDevices)
    ENTRY(vkGetPhysicalDeviceProperties)
    ENTRY(vkGetPhysicalDeviceFeatures)
    ENTRY(vkGetPhysicalDeviceQueueFamilyProperties)
    ENTRY(vkCreateDevice)
    ENTRY(vkDestroyInstance)
#undef ENTRY
    return nullptr;
}

// Installs the stand-in loader for the duration of a test.
struct FakeLoader
{
    FakeLoader()
    {
        g_loader = {};
        g_testingVkGetInstanceProcAddr = fake_vkGetInstanceProcAddr;
    }
    ~FakeLoader() { g_testingVkGetInstanceProcAddr = nullptr; }
};

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

// A RIVE_GPU filter from the environment wouldn't match the fake device, so
// clear it for the duration of a test and restore it for the tests after.
class ScopedUnsetRiveGpu
{
public:
    ScopedUnsetRiveGpu()
    {
        if (const char* value = getenv("RIVE_GPU"))
        {
            m_saved = value;
        }
        setRiveGpu(nullptr);
    }
    ~ScopedUnsetRiveGpu()
    {
        if (m_saved.has_value())
        {
            setRiveGpu(m_saved->c_str());
        }
    }

private:
    std::optional<std::string> m_saved;
};

const char* g_requiredExtensions[] = {VK_KHR_SURFACE_EXTENSION_NAME};

const VulkanInstance::Options kInstanceOptions = {
    .requiredExtensions = rive::make_span(g_requiredExtensions, 1),
    .desiredValidationType = VulkanValidationType::none,
    .wantDebugCallbacks = false,
};

// Fills the stack below us with 64s, so a count the code never initialized
// reads as 64 rather than whatever the last caller left there. On devices that
// garbage is often a pointer-sized value, and the resize aborts; a small count
// keeps a regression here a failed assertion instead of a terabyte allocation.
VKB_NEVER_INLINE void poisonStack()
{
    volatile uint32_t scratch[4 * 1024];
    for (auto& word : scratch)
    {
        word = 64;
    }
}

std::unique_ptr<VulkanInstance> createInstance()
{
    poisonStack();
    return VulkanInstance::Create(kInstanceOptions);
}
} // namespace

TEST_CASE("VulkanInstance is created when enumeration succeeds",
          "[vulkan_bootstrap]")
{
    FakeLoader loader;

    CHECK(createInstance() != nullptr);
    CHECK(g_loader.createdInstance);
}

TEST_CASE("VulkanInstance creation stops at a failed enumeration",
          "[vulkan_bootstrap]")
{
    FakeLoader loader;
    // 0, 1: extension count and properties. 2, 3: layer count and properties.
    // VK_INCOMPLETE only comes back from a properties call.
    std::tie(g_loader.failingCall, g_loader.failure) =
        GENERATE(table<int, VkResult>({
            {0, VK_ERROR_INITIALIZATION_FAILED},
            {0, VK_ERROR_OUT_OF_HOST_MEMORY},
            {1, VK_ERROR_INITIALIZATION_FAILED},
            {1, VK_ERROR_OUT_OF_HOST_MEMORY},
            {1, VK_INCOMPLETE},
            {2, VK_ERROR_INITIALIZATION_FAILED},
            {2, VK_ERROR_OUT_OF_HOST_MEMORY},
            {3, VK_ERROR_INITIALIZATION_FAILED},
            {3, VK_ERROR_OUT_OF_HOST_MEMORY},
            {3, VK_INCOMPLETE},
        }));

    // Returning null is what lets the Android runtime fall back to OpenGL.
    CHECK(createInstance() == nullptr);
    CHECK_FALSE(g_loader.createdInstance);
    // Nothing may act on the count a failed call left undefined.
    CHECK(g_loader.calls.size() == size_t(g_loader.failingCall) + 1);
}

TEST_CASE("VulkanDevice enables the extensions it needs", "[vulkan_bootstrap]")
{
    FakeLoader loader;
    ScopedUnsetRiveGpu unsetRiveGpu;
    auto instance = createInstance();
    REQUIRE(instance != nullptr);

    VulkanDevice::Create(*instance, {});

    CHECK(g_loader.createdDevice);
    CHECK(g_loader.enabledDeviceExtensions ==
          std::vector<std::string>{VK_KHR_SWAPCHAIN_EXTENSION_NAME});
}

TEST_CASE("VulkanDevice carries on without extensions when enumeration fails",
          "[vulkan_bootstrap]")
{
    FakeLoader loader;
    ScopedUnsetRiveGpu unsetRiveGpu;
    auto instance = createInstance();
    REQUIRE(instance != nullptr);

    // 0: extension count. 1: extension properties.
    const auto [failingOffset, failure] = GENERATE(table<int, VkResult>({
        {0, VK_ERROR_INITIALIZATION_FAILED},
        {0, VK_ERROR_OUT_OF_HOST_MEMORY},
        {1, VK_ERROR_INITIALIZATION_FAILED},
        {1, VK_ERROR_OUT_OF_HOST_MEMORY},
        {1, VK_INCOMPLETE},
    }));
    g_loader.failingCall = int(g_loader.calls.size()) + failingOffset;
    g_loader.failure = failure;

    SECTION("headless, which needs no extensions")
    {
        poisonStack();
        VulkanDevice::Create(*instance, {.headless = true});
        CHECK(g_loader.createdDevice);
        CHECK(g_loader.enabledDeviceExtensions.empty());
    }

    SECTION("presenting, which needs a swapchain")
    {
        poisonStack();
        // Null is what lets the Android runtime fall back to OpenGL.
        CHECK(VulkanDevice::Create(*instance, {}) == nullptr);
        CHECK_FALSE(g_loader.createdDevice);
    }

    // Nothing may act on the count a failed call left undefined.
    REQUIRE(g_loader.calls.size() == size_t(g_loader.failingCall) + 1);
    CHECK(g_loader.calls.back().command ==
          "vkEnumerateDeviceExtensionProperties");
    CHECK(g_loader.calls.back().queriedCount == (failingOffset == 0));
}
} // namespace rive_vkb

#endif // RIVE_VULKAN
