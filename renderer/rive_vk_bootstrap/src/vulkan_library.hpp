/*
 * Copyright 2025 Rive
 */

#ifdef _WIN32 // !!! in the VulkanInstance cpp
#include <Windows.h>
#endif

#include <assert.h>
#include <vulkan/vulkan.h>

// Helper macro to use a given object with a load_instance_func function to load
// a vulkan function with the given name (to avoid redundancy and reduce the
// chance of typos in the string)
#define LOAD_INSTANCE_FUNC(name, obj) (obj).loadInstanceFunc<PFN_##name>(#name)

// Helper macro to define and load a given instance func (eliminating the name
// needing to be typed 3 times and reduce the chance of typos in the string)
#define DEFINE_AND_LOAD_INSTANCE_FUNC(name, obj)                               \
    auto name = LOAD_INSTANCE_FUNC(name, obj)

// Helper macro to load a given instance func into an existing member
// (eliminating the name needing to be typed 3 times and reduce the chance of
// typos in the string)
#define LOAD_MEMBER_INSTANCE_FUNC(name, obj)                                   \
    m_##name = LOAD_INSTANCE_FUNC(name, obj)

// Same as LOAD_MEMBER_INSTANCE_FUNC but asserts non-null
#define LOAD_REQUIRED_MEMBER_INSTANCE_FUNC(name, obj)                          \
    LOAD_MEMBER_INSTANCE_FUNC(name, obj);                                      \
    assert(m_##name != nullptr && "Could not load " #name)
namespace rive_vkb
{
class VulkanLibrary
{
public:
    VulkanLibrary();
    ~VulkanLibrary();

    PFN_vkVoidFunction getInstanceProcAddr(VkInstance instance,
                                           const char* name) const;

    // Load an instance func that doesn't require a specified instance
    template <typename T> T loadInstanceFunc(const char* name) const
    {
        return reinterpret_cast<T>(getInstanceProcAddr(VK_NULL_HANDLE, name));
    }

    bool canEnumerateInstanceVersion() const;

    VkResult enumerateInstanceVersion(uint32_t* outVersion) const;
    VkResult createInstance(const VkInstanceCreateInfo* pCreateInfo,
                            const VkAllocationCallbacks* pAllocator,
                            VkInstance* pInstance) const;
    VkResult enumerateInstanceLayerProperties(
        uint32_t* pPropertyCount,
        VkLayerProperties* pProperties) const;
    VkResult enumerateInstanceExtensionProperties(
        const char* pLayerName,
        uint32_t* pPropertyCount,
        VkExtensionProperties* pProperties) const;

    PFN_vkGetInstanceProcAddr getVkGetInstanceProcAddrPtr() const
    {
        return m_vkGetInstanceProcAddr;
    }

private:
#ifdef _WIN32
    HMODULE m_library;
#else
    void* m_library;
#endif

    PFN_vkGetInstanceProcAddr m_vkGetInstanceProcAddr = nullptr;
    PFN_vkEnumerateInstanceVersion m_vkEnumerateInstanceVersion = nullptr;
    PFN_vkCreateInstance m_vkCreateInstance = nullptr;
    PFN_vkEnumerateInstanceLayerProperties
        m_vkEnumerateInstanceLayerProperties = nullptr;
    PFN_vkEnumerateInstanceExtensionProperties
        m_vkEnumerateInstanceExtensionProperties = nullptr;
};
} // namespace rive_vkb