/*
 * Copyright 2025 Rive
 */

#include "logging.hpp"
#include "vulkan_error_handling.hpp"
#include "vulkan_library.hpp"

#include <stdio.h>

#ifndef _WIN32
#include <dlfcn.h>
#endif

namespace rive_vkb
{
std::unique_ptr<VulkanLibrary> VulkanLibrary::Create()
{
    // Private constructor, can't use make_unique
    bool success = true;
    auto outLibrary =
        std::unique_ptr<VulkanLibrary>{new VulkanLibrary(&success)};
    CONFIRM_OR_RETURN_VALUE(success, nullptr);
    return outLibrary;
}

VulkanLibrary::VulkanLibrary(bool* successOut)
{
    *successOut = false;

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

    CONFIRM_OR_RETURN_MSG(m_library != nullptr,
                          "Failed to find Vulkan library");

#ifdef _WIN32
    m_vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        GetProcAddress(m_library, "vkGetInstanceProcAddr"));
#else
    m_vkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
        dlsym(m_library, "vkGetInstanceProcAddr"));
#endif
    CONFIRM_OR_RETURN_MSG(
        m_vkGetInstanceProcAddr != nullptr,
        "Failed to load Vulkan function 'vkGetInstanceProcAddr'");

    // This function is allowed to be null
    LOAD_MEMBER_INSTANCE_FUNC(this, vkEnumerateInstanceVersion, *this);

    LOAD_MEMBER_INSTANCE_FUNC_OR_RETURN(this, vkCreateInstance, *this);
    LOAD_MEMBER_INSTANCE_FUNC_OR_RETURN(this,
                                        vkEnumerateInstanceLayerProperties,
                                        *this);
    LOAD_MEMBER_INSTANCE_FUNC_OR_RETURN(this,
                                        vkEnumerateInstanceExtensionProperties,
                                        *this);

    *successOut = true;
}

VulkanLibrary::~VulkanLibrary()
{
#ifdef _WIN32
    if (m_library != nullptr)
    {
        FreeLibrary(m_library);
    }
#else
    if (m_library != nullptr)
    {
        dlclose(m_library);
    }
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