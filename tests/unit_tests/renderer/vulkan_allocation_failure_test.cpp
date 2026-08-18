/*
 * Copyright 2026 Rive
 */

// Exercises the paths that only run when a Vulkan driver fails to allocate.
// Real drivers don't fail on demand, so these run a real VulkanContext against
// a stand-in driver whose commands fail whenever we ask them to.
//
// Only built with --with_vulkan; see test.sh.

#ifdef RIVE_VULKAN

#include "rive/renderer/vulkan/vulkan_context.hpp"

#include <catch.hpp>
#include <string.h>

namespace rive::gpu
{
namespace
{
// Which commands the stand-in driver should fail, and with what.
struct
{
    VkResult createRenderPass = VK_SUCCESS;
    VkResult createSampler = VK_SUCCESS;
    VkResult createBuffer = VK_SUCCESS;
    VkResult mapMemory = VK_SUCCESS;
} g_failures;

// The Vulkan spec leaves an output parameter undefined when a command fails, so
// the stand-ins below write this to theirs before returning an error. Anything
// that publishes a handle without checking the VkResult hands back this value,
// and a destructor that then frees it is the crash we're guarding against.
template <typename Handle> Handle poisonHandle()
{
#if defined(VK_USE_64_BIT_PTR_DEFINES) && VK_USE_64_BIT_PTR_DEFINES == 1
    return reinterpret_cast<Handle>(static_cast<uintptr_t>(0xDEADBEEF));
#else
    return static_cast<Handle>(0xDEADBEEFull);
#endif
}

// The handle a successful command hands back.
template <typename Handle> Handle liveHandle()
{
#if defined(VK_USE_64_BIT_PTR_DEFINES) && VK_USE_64_BIT_PTR_DEFINES == 1
    return reinterpret_cast<Handle>(static_cast<uintptr_t>(0x1000));
#else
    return static_cast<Handle>(0x1000ull);
#endif
}

#define DEFINE_FAKE_CREATE(CMD, CreateInfo, Handle, failureField)              \
    VKAPI_ATTR VkResult VKAPI_CALL fake_vk##CMD(VkDevice,                      \
                                                const CreateInfo*,             \
                                                const VkAllocationCallbacks*,  \
                                                Handle* out)                   \
    {                                                                          \
        /* Always scribble, so a published failure is detectable. */           \
        *out = g_failures.failureField == VK_SUCCESS ? liveHandle<Handle>()    \
                                                     : poisonHandle<Handle>(); \
        return g_failures.failureField;                                        \
    }

DEFINE_FAKE_CREATE(CreateRenderPass,
                   VkRenderPassCreateInfo,
                   VkRenderPass,
                   createRenderPass)
DEFINE_FAKE_CREATE(CreateSampler, VkSamplerCreateInfo, VkSampler, createSampler)
DEFINE_FAKE_CREATE(CreateBuffer, VkBufferCreateInfo, VkBuffer, createBuffer)
#undef DEFINE_FAKE_CREATE

VKAPI_ATTR void VKAPI_CALL
fake_vkGetPhysicalDeviceProperties(VkPhysicalDevice,
                                   VkPhysicalDeviceProperties* props)
{
    memset(props, 0, sizeof(*props));
    props->apiVersion = VK_API_VERSION_1_1;
    props->vendorID = 0x1234; // Not a vendor we apply workarounds for.
    props->limits.bufferImageGranularity = 1;
    props->limits.maxMemoryAllocationCount = 4096;
    props->limits.nonCoherentAtomSize = 1;
    strncpy(props->deviceName, "Rive fake driver", sizeof(props->deviceName));
}

VKAPI_ATTR void VKAPI_CALL fake_vkGetPhysicalDeviceMemoryProperties(
    VkPhysicalDevice,
    VkPhysicalDeviceMemoryProperties* props)
{
    memset(props, 0, sizeof(*props));
    props->memoryHeapCount = 1;
    props->memoryHeaps[0].size = 256 * 1024 * 1024;
    props->memoryHeaps[0].flags = VK_MEMORY_HEAP_DEVICE_LOCAL_BIT;
    // One device-local type and one mappable type, so VMA can satisfy both
    // Mappability::none and Mappability::writeOnly.
    props->memoryTypeCount = 2;
    props->memoryTypes[0].heapIndex = 0;
    props->memoryTypes[0].propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    props->memoryTypes[1].heapIndex = 0;
    props->memoryTypes[1].propertyFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
}

VKAPI_ATTR void VKAPI_CALL fake_vkGetPhysicalDeviceMemoryProperties2(
    VkPhysicalDevice physicalDevice,
    VkPhysicalDeviceMemoryProperties2* props)
{
    fake_vkGetPhysicalDeviceMemoryProperties(physicalDevice,
                                             &props->memoryProperties);
}

VKAPI_ATTR void VKAPI_CALL
fake_vkGetPhysicalDeviceFormatProperties(VkPhysicalDevice,
                                         VkFormat,
                                         VkFormatProperties* props)
{
    memset(props, 0, sizeof(*props));
    // VulkanContext asserts that some depth/stencil format is supported.
    props->optimalTilingFeatures = ~0u;
}

VKAPI_ATTR void VKAPI_CALL
fake_vkGetPhysicalDeviceFeatures(VkPhysicalDevice,
                                 VkPhysicalDeviceFeatures* features)
{
    memset(features, 0, sizeof(*features));
}

// VMA touches these but nothing in these tests gets far enough to care what
// they do, since every allocation we drive either fails at vkCreateBuffer or
// never allocates memory.
VKAPI_ATTR VkResult VKAPI_CALL
fake_vkAllocateMemory(VkDevice,
                      const VkMemoryAllocateInfo*,
                      const VkAllocationCallbacks*,
                      VkDeviceMemory* memory)
{
    *memory = liveHandle<VkDeviceMemory>();
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL
fake_vkGetBufferMemoryRequirements(VkDevice,
                                   VkBuffer,
                                   VkMemoryRequirements* requirements)
{
    requirements->size = 256;
    requirements->alignment = 16;
    requirements->memoryTypeBits = ~0u;
}

VKAPI_ATTR void VKAPI_CALL
fake_vkGetImageMemoryRequirements(VkDevice,
                                  VkImage,
                                  VkMemoryRequirements* requirements)
{
    requirements->size = 256;
    requirements->alignment = 16;
    requirements->memoryTypeBits = ~0u;
}

VKAPI_ATTR void VKAPI_CALL
fake_vkGetBufferMemoryRequirements2(VkDevice device,
                                    const VkBufferMemoryRequirementsInfo2* info,
                                    VkMemoryRequirements2* requirements)
{
    fake_vkGetBufferMemoryRequirements(device,
                                       info->buffer,
                                       &requirements->memoryRequirements);
}

VKAPI_ATTR void VKAPI_CALL
fake_vkGetImageMemoryRequirements2(VkDevice device,
                                   const VkImageMemoryRequirementsInfo2* info,
                                   VkMemoryRequirements2* requirements)
{
    fake_vkGetImageMemoryRequirements(device,
                                      info->image,
                                      &requirements->memoryRequirements);
}

VKAPI_ATTR VkResult VKAPI_CALL fake_vkCreateImage(VkDevice,
                                                  const VkImageCreateInfo*,
                                                  const VkAllocationCallbacks*,
                                                  VkImage* image)
{
    *image = liveHandle<VkImage>();
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL fake_vkMapMemory(VkDevice,
                                                VkDeviceMemory,
                                                VkDeviceSize,
                                                VkDeviceSize,
                                                VkMemoryMapFlags,
                                                void** data)
{
    if (g_failures.mapMemory != VK_SUCCESS)
    {
        // Scribbled for the same reason the create commands scribble.
        *data = reinterpret_cast<void*>(static_cast<uintptr_t>(0xDEADBEEF));
        return g_failures.mapMemory;
    }
    static char scratch[4096];
    *data = scratch;
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL fakeSucceed() { return VK_SUCCESS; }
VKAPI_ATTR void VKAPI_CALL fakeIgnore() {}

// Every command VulkanContext or VMA resolves by name. Anything absent comes
// back null, which is fine as long as no test drives a path that calls it.
PFN_vkVoidFunction resolve(const char* name)
{
#define ENTRY(vkName, fn)                                                      \
    if (strcmp(name, vkName) == 0)                                             \
    {                                                                          \
        return reinterpret_cast<PFN_vkVoidFunction>(fn);                       \
    }

    ENTRY("vkCreateRenderPass", fake_vkCreateRenderPass)
    ENTRY("vkCreateSampler", fake_vkCreateSampler)
    ENTRY("vkCreateBuffer", fake_vkCreateBuffer)
    ENTRY("vkCreateImage", fake_vkCreateImage)
    ENTRY("vkGetPhysicalDeviceProperties", fake_vkGetPhysicalDeviceProperties)
    ENTRY("vkGetPhysicalDeviceFeatures", fake_vkGetPhysicalDeviceFeatures)
    ENTRY("vkGetPhysicalDeviceFormatProperties",
          fake_vkGetPhysicalDeviceFormatProperties)
    ENTRY("vkGetPhysicalDeviceMemoryProperties",
          fake_vkGetPhysicalDeviceMemoryProperties)
    ENTRY("vkGetPhysicalDeviceMemoryProperties2",
          fake_vkGetPhysicalDeviceMemoryProperties2)
    ENTRY("vkGetPhysicalDeviceMemoryProperties2KHR",
          fake_vkGetPhysicalDeviceMemoryProperties2)
    ENTRY("vkAllocateMemory", fake_vkAllocateMemory)
    ENTRY("vkMapMemory", fake_vkMapMemory)
    ENTRY("vkGetBufferMemoryRequirements", fake_vkGetBufferMemoryRequirements)
    ENTRY("vkGetImageMemoryRequirements", fake_vkGetImageMemoryRequirements)
    ENTRY("vkGetBufferMemoryRequirements2", fake_vkGetBufferMemoryRequirements2)
    ENTRY("vkGetBufferMemoryRequirements2KHR",
          fake_vkGetBufferMemoryRequirements2)
    ENTRY("vkGetImageMemoryRequirements2", fake_vkGetImageMemoryRequirements2)
    ENTRY("vkGetImageMemoryRequirements2KHR",
          fake_vkGetImageMemoryRequirements2)

    // Commands whose return value or output nothing here depends on.
    ENTRY("vkFreeMemory", fakeIgnore)
    ENTRY("vkUnmapMemory", fakeIgnore)
    ENTRY("vkDestroyBuffer", fakeIgnore)
    ENTRY("vkDestroyImage", fakeIgnore)
    ENTRY("vkCmdCopyBuffer", fakeIgnore)
    ENTRY("vkFlushMappedMemoryRanges", fakeSucceed)
    ENTRY("vkInvalidateMappedMemoryRanges", fakeSucceed)
    ENTRY("vkBindBufferMemory", fakeSucceed)
    ENTRY("vkBindImageMemory", fakeSucceed)
    ENTRY("vkBindBufferMemory2", fakeSucceed)
    ENTRY("vkBindBufferMemory2KHR", fakeSucceed)
    ENTRY("vkBindImageMemory2", fakeSucceed)
    ENTRY("vkBindImageMemory2KHR", fakeSucceed)
#undef ENTRY
    return nullptr;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
fake_vkGetDeviceProcAddr(VkDevice, const char* name)
{
    return resolve(name);
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL
fake_vkGetInstanceProcAddr(VkInstance, const char* name)
{
    if (strcmp(name, "vkGetDeviceProcAddr") == 0)
    {
        return reinterpret_cast<PFN_vkVoidFunction>(fake_vkGetDeviceProcAddr);
    }
    return resolve(name);
}

// A VulkanContext backed by the stand-in driver above. Nothing here talks to
// real hardware.
//
// VulkanContext is a GPUResourceManager, which requires a shutdown cycle to
// drain its resource purgatory before it is destroyed. RenderContext normally
// drives that, so the tests get it from this instead.
rcp<VulkanContext> makeRawContext()
{
    rcp<VulkanContext> vk = VulkanContext::make(liveHandle<VkInstance>(),
                                                liveHandle<VkPhysicalDevice>(),
                                                liveHandle<VkDevice>(),
                                                VulkanFeatures{},
                                                fake_vkGetInstanceProcAddr);
    // Everything below assumes a live allocator; fail here rather than deep
    // inside VMA if the stand-in driver ever stops satisfying it.
    REQUIRE(vk != nullptr);
    return vk;
}

class FakeContext
{
public:
    FakeContext() :
        m_vk(makeRawContext()),
        // Without this, a failed allocation aborts the test binary.
        m_allocationFailures(m_vk.get())
    {}
    FakeContext(const FakeContext&) = delete;
    FakeContext& operator=(const FakeContext&) = delete;
    ~FakeContext() { m_vk->shutdown(); }

    VulkanContext* operator->() const { return m_vk.get(); }
    VulkanContext* get() const { return m_vk.get(); }
    operator VulkanContext*() const { return m_vk.get(); }

    bool anyAllocationFailed() const
    {
        return m_allocationFailures.anyFailed();
    }

private:
    const rcp<VulkanContext> m_vk;
    VulkanContext::AllocationFailureScope m_allocationFailures;
};

// Resets the stand-in driver, then builds a context on it.
struct FakeDriverReset
{
    FakeDriverReset() { g_failures = {}; }
};

// Stands in for an init path that creates one object and bails if the driver
// couldn't deliver it.
bool initRenderPass(VulkanContext* vk, VkRenderPass* renderPass)
{
    const uint32_t baseline = vk->allocationFailureCount();
    const VkRenderPassCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
    };
    *renderPass = VK_CREATE_HANDLE(vk, CreateRenderPass, &createInfo);
    if (vk->allocationFailureCount() != baseline)
    {
        return false;
    }
    return true;
}
} // namespace

TEST_CASE("VK_CREATE_HANDLE publishes the driver's handle on success",
          "[vulkan]")
{
    FakeDriverReset resetDriver;
    FakeContext vk;

    const VkRenderPassCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
    };
    VkRenderPass renderPass =
        VK_CREATE_HANDLE(vk, CreateRenderPass, &createInfo);

    CHECK(renderPass == liveHandle<VkRenderPass>());
    CHECK(vk->allocationFailureCount() == 0);
}

TEST_CASE("VK_CREATE_HANDLE drops the out parameter a failed driver scribbled",
          "[vulkan]")
{
    FakeDriverReset resetDriver;
    FakeContext vk;

    // Every failure code, since the spec only guarantees the out parameter is
    // defined on VK_SUCCESS.
    g_failures.createRenderPass = GENERATE(VK_ERROR_OUT_OF_HOST_MEMORY,
                                           VK_ERROR_OUT_OF_DEVICE_MEMORY,
                                           VK_ERROR_INITIALIZATION_FAILED,
                                           VK_ERROR_DEVICE_LOST);

    const VkRenderPassCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
    };
    VkRenderPass renderPass =
        VK_CREATE_HANDLE(vk, CreateRenderPass, &createInfo);

    // The poison value is what the owner would hand to vkDestroyRenderPass;
    // only VK_NULL_HANDLE is safe to destroy.
    CHECK(renderPass != poisonHandle<VkRenderPass>());
    CHECK(renderPass == VK_NULL_HANDLE);
    CHECK(vk->allocationFailureCount() == 1);
}

TEST_CASE("VK_CREATE_HANDLE deduces the handle type per command", "[vulkan]")
{
    FakeDriverReset resetDriver;
    FakeContext vk;

    const VkSamplerCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
    };
    CHECK(VK_CREATE_HANDLE(vk, CreateSampler, &createInfo) ==
          liveHandle<VkSampler>());

    g_failures.createSampler = VK_ERROR_OUT_OF_DEVICE_MEMORY;
    VkSampler sampler = VK_CREATE_HANDLE(vk, CreateSampler, &createInfo);
    CHECK(sampler == VK_NULL_HANDLE);
    CHECK(vk->allocationFailureCount() == 1);
}

TEST_CASE("an allocation-count baseline bails out an init path", "[vulkan]")
{
    FakeDriverReset resetDriver;
    FakeContext vk;

    SECTION("driver succeeds")
    {
        VkRenderPass renderPass = VK_NULL_HANDLE;
        CHECK(initRenderPass(vk, &renderPass));
        CHECK(renderPass == liveHandle<VkRenderPass>());
    }

    SECTION("driver fails")
    {
        g_failures.createRenderPass = VK_ERROR_OUT_OF_DEVICE_MEMORY;
        VkRenderPass renderPass = VK_NULL_HANDLE;
        CHECK_FALSE(initRenderPass(vk, &renderPass));
        // Safe for the caller's destructor to hand to vkDestroyRenderPass.
        CHECK(renderPass == VK_NULL_HANDLE);
    }

    SECTION("a failure before the baseline doesn't trip a later init")
    {
        const VkSamplerCreateInfo samplerInfo = {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        };
        g_failures.createSampler = VK_ERROR_OUT_OF_DEVICE_MEMORY;
        VK_CREATE_HANDLE(vk, CreateSampler, &samplerInfo);
        REQUIRE(vk->allocationFailureCount() == 1);

        VkRenderPass renderPass = VK_NULL_HANDLE;
        CHECK(initRenderPass(vk, &renderPass));
    }
}

TEST_CASE("a failed vkutil::Buffer allocation counts and stays null",
          "[vulkan]")
{
    FakeDriverReset resetDriver;
    FakeContext vk;
    g_failures.createBuffer = VK_ERROR_OUT_OF_DEVICE_MEMORY;

    rcp<vkutil::Buffer> buffer = vk->makeBuffer(
        {
            .size = 1024,
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        },
        vkutil::Mappability::writeOnly);

    REQUIRE(buffer != nullptr);
    CHECK(vk->allocationFailureCount() == 1);
    // Null rather than the poison VMA would have left behind, so ~Buffer() has
    // nothing to free and callers can detect the failure.
    CHECK(static_cast<VkBuffer>(*buffer) == VK_NULL_HANDLE);
    // Releasing the half-built buffer must not touch a garbage allocation.
    buffer = nullptr;
}

TEST_CASE("a vkutil::Buffer that allocates but can't map stays null",
          "[vulkan]")
{
    FakeDriverReset resetDriver;
    FakeContext vk;
    // The buffer itself allocates; only the mapping fails, which is the case
    // that used to leave a live handle behind with null contents.
    g_failures.mapMemory = VK_ERROR_MEMORY_MAP_FAILED;

    rcp<vkutil::Buffer> buffer = vk->makeBuffer(
        {
            .size = 1024,
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        },
        vkutil::Mappability::writeOnly);

    REQUIRE(buffer != nullptr);
    CHECK(vk->allocationFailureCount() == 1);
    CHECK_FALSE(buffer->hasContents());
    // The handle has to go too: callers test it to decide whether anything was
    // allocated, and ~Buffer() would otherwise unmap a mapping we never made.
    CHECK(static_cast<VkBuffer>(*buffer) == VK_NULL_HANDLE);
    buffer = nullptr;
}

TEST_CASE("a successful vkutil::Buffer allocation doesn't count", "[vulkan]")
{
    FakeDriverReset resetDriver;
    FakeContext vk;

    rcp<vkutil::Buffer> buffer = vk->makeBuffer(
        {
            .size = 1024,
            .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        },
        vkutil::Mappability::writeOnly);

    REQUIRE(buffer != nullptr);
    CHECK(vk->allocationFailureCount() == 0);
    CHECK(static_cast<VkBuffer>(*buffer) != VK_NULL_HANDLE);
    CHECK(buffer->hasContents());
    buffer = nullptr;
}

TEST_CASE("each AllocationFailureScope starts from where it began", "[vulkan]")
{
    // A VulkanContext can outlive the renderer that built it, and can serve
    // several of them, so a later init must not inherit an earlier one's
    // failures.
    g_failures = {};
    rcp<VulkanContext> vk = makeRawContext();

    const VkRenderPassCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
    };

    {
        VulkanContext::AllocationFailureScope firstInit(vk.get());
        CHECK_FALSE(firstInit.anyFailed());

        g_failures.createRenderPass = VK_ERROR_OUT_OF_DEVICE_MEMORY;
        CHECK(VK_CREATE_HANDLE(vk, CreateRenderPass, &createInfo) ==
              VK_NULL_HANDLE);
        CHECK(firstInit.anyFailed());
    }

    g_failures.createRenderPass = VK_SUCCESS;

    {
        VulkanContext::AllocationFailureScope secondInit(vk.get());
        // The first init's failure is still in the count, but is not this
        // init's problem.
        REQUIRE(vk->allocationFailureCount() == 1);
        CHECK_FALSE(secondInit.anyFailed());

        CHECK(VK_CREATE_HANDLE(vk, CreateRenderPass, &createInfo) ==
              liveHandle<VkRenderPass>());
        CHECK_FALSE(secondInit.anyFailed());
    }

    vk->shutdown();
}
} // namespace rive::gpu

#endif // RIVE_VULKAN
