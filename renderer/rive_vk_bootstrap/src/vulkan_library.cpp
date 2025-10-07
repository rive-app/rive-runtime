/*
 * Copyright 2025 Rive
 */

#include "vulkan_library.hpp"

#ifndef _WIN32
#include <dlfcn.h>
#endif

namespace rive_vkb
{
VulkanLibrary::VulkanLibrary()
{
#if !defined(_WIN32) && !defined(__APPLE__) && !defined(__linux__)
    static_assert(false, "Unsupported platform");
#endif

    constexpr const char* libFilenameCandidates[] = {
#if defined(_WIN32)
        "vulkan-1.dll",
#elif defined(__APPLE__)
        "libvulkan.dylib",
        "libvulkan.1.dylib",
        "libMoltenVK.dylib",
        // The Vulkan SDK on Mac gets installed to /usr/local/lib, which is no
        // longer on the library search path after Sonoma.
        "/usr/local/lib/libvulkan.1.dylib",
#else
        "libvulkan.so.1",
        "libvulkan.so",
#endif
    };

    for (auto* filenameCandidate : libFilenameCandidates)
    {
#ifdef _WIN32
        m_library = LoadLibraryA(filenameCandidate);
#else
        m_library = dlopen(filenameCandidate, RTLD_NOW | RTLD_LOCAL);
#endif

        if (m_library != nullptr)
        {
            break;
        }
    }

    assert(m_library != nullptr && "Failed to find Vulkan library");

#ifdef _WIN32
    m_vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        GetProcAddress(m_library, "vkGetInstanceProcAddr"));
#else
    m_vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        dlsym(m_library, "vkGetInstanceProcAddr"));
#endif
    assert(m_vkGetInstanceProcAddr != nullptr);

    LOAD_MEMBER_INSTANCE_FUNC(vkEnumerateInstanceVersion, *this);
    LOAD_MEMBER_INSTANCE_FUNC(vkCreateInstance, *this);
    assert(m_vkCreateInstance != nullptr);
    LOAD_MEMBER_INSTANCE_FUNC(vkEnumerateInstanceLayerProperties, *this);
    assert(m_vkEnumerateInstanceLayerProperties != nullptr);
    LOAD_MEMBER_INSTANCE_FUNC(vkEnumerateInstanceExtensionProperties, *this);
    assert(m_vkEnumerateInstanceExtensionProperties != nullptr);
}

VulkanLibrary::~VulkanLibrary()
{
#ifdef _WIN32
    FreeLibrary(m_library);
#else
    dlclose(m_library);
#endif
}

PFN_vkVoidFunction VulkanLibrary::getInstanceProcAddr(VkInstance instance,
                                                      const char* name) const
{
    return m_vkGetInstanceProcAddr(instance, name);
}

bool VulkanLibrary::canEnumerateInstanceVersion() const
{
    return m_vkEnumerateInstanceVersion != nullptr;
}

VkResult VulkanLibrary::enumerateInstanceVersion(uint32_t* outVersion) const
{
    return m_vkEnumerateInstanceVersion(outVersion);
}

VkResult VulkanLibrary::createInstance(const VkInstanceCreateInfo* pCreateInfo,
                                       const VkAllocationCallbacks* pAllocator,
                                       VkInstance* pInstance) const
{
    return m_vkCreateInstance(pCreateInfo, pAllocator, pInstance);
}

VkResult VulkanLibrary::enumerateInstanceLayerProperties(
    uint32_t* pPropertyCount,
    VkLayerProperties* pProperties) const
{
    return m_vkEnumerateInstanceLayerProperties(pPropertyCount, pProperties);
}

VkResult VulkanLibrary::enumerateInstanceExtensionProperties(
    const char* pLayerName,
    uint32_t* pPropertyCount,
    VkExtensionProperties* pProperties) const
{
    return m_vkEnumerateInstanceExtensionProperties(pLayerName,
                                                    pPropertyCount,
                                                    pProperties);
}
} // namespace rive_vkb