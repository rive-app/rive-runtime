/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/ore/ore_context.hpp"
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_texture.hpp"
#include "rive/renderer/ore/ore_sampler.hpp"
#include "rive/renderer/ore/ore_shader_module.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/renderer/ore/ore_render_pass.hpp"
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/vulkan/render_target_vulkan.hpp"

#include <vk_mem_alloc.h>
// VMA_IMPLEMENTATION is defined in src/vulkan/vulkan_memory_allocator.cpp,
// which is compiled when --with_vulkan is passed (required for this backend).

#include <cassert>
#include <cstring>
#include <string>

namespace rive::ore
{

// ============================================================================
// Enum → VkFormat helper (shared with ore_texture_vulkan.cpp via header,
// but declared again here as a file-local helper for the render pass builder).
// ============================================================================

static VkFormat oreFormatToVkLocal(TextureFormat fmt)
{
    // Mirror of oreFormatToVk in ore_texture_vulkan.cpp.
    switch (fmt)
    {
        case TextureFormat::r8unorm:
            return VK_FORMAT_R8_UNORM;
        case TextureFormat::rg8unorm:
            return VK_FORMAT_R8G8_UNORM;
        case TextureFormat::rgba8unorm:
            return VK_FORMAT_R8G8B8A8_UNORM;
        case TextureFormat::rgba8snorm:
            return VK_FORMAT_R8G8B8A8_SNORM;
        case TextureFormat::bgra8unorm:
            return VK_FORMAT_B8G8R8A8_UNORM;
        case TextureFormat::rgba16float:
            return VK_FORMAT_R16G16B16A16_SFLOAT;
        case TextureFormat::rg16float:
            return VK_FORMAT_R16G16_SFLOAT;
        case TextureFormat::r16float:
            return VK_FORMAT_R16_SFLOAT;
        case TextureFormat::rgba32float:
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        case TextureFormat::rg32float:
            return VK_FORMAT_R32G32_SFLOAT;
        case TextureFormat::r32float:
            return VK_FORMAT_R32_SFLOAT;
        case TextureFormat::rgb10a2unorm:
            return VK_FORMAT_A2B10G10R10_UNORM_PACK32;
        case TextureFormat::r11g11b10float:
            return VK_FORMAT_B10G11R11_UFLOAT_PACK32;
        case TextureFormat::depth16unorm:
            return VK_FORMAT_D16_UNORM;
        case TextureFormat::depth24plusStencil8:
            return VK_FORMAT_D24_UNORM_S8_UINT;
        case TextureFormat::depth32float:
            return VK_FORMAT_D32_SFLOAT;
        case TextureFormat::depth32floatStencil8:
            return VK_FORMAT_D32_SFLOAT_S8_UINT;
        case TextureFormat::bc1unorm:
            return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
        case TextureFormat::bc3unorm:
            return VK_FORMAT_BC3_UNORM_BLOCK;
        case TextureFormat::bc7unorm:
            return VK_FORMAT_BC7_UNORM_BLOCK;
        case TextureFormat::etc2rgb8:
            return VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
        case TextureFormat::etc2rgba8:
            return VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
        case TextureFormat::astc4x4:
            return VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
        case TextureFormat::astc6x6:
            return VK_FORMAT_ASTC_6x6_UNORM_BLOCK;
        case TextureFormat::astc8x8:
            return VK_FORMAT_ASTC_8x8_UNORM_BLOCK;
    }
    return VK_FORMAT_UNDEFINED;
}

static bool isDepthStencilFormatLocal(TextureFormat fmt)
{
    switch (fmt)
    {
        case TextureFormat::depth16unorm:
        case TextureFormat::depth24plusStencil8:
        case TextureFormat::depth32float:
        case TextureFormat::depth32floatStencil8:
            return true;
        default:
            return false;
    }
}

static bool hasStencilLocal(TextureFormat fmt)
{
    return fmt == TextureFormat::depth24plusStencil8 ||
           fmt == TextureFormat::depth32floatStencil8;
}

static VkAttachmentLoadOp oreLoadOpToVk(LoadOp op)
{
    switch (op)
    {
        case LoadOp::clear:
            return VK_ATTACHMENT_LOAD_OP_CLEAR;
        case LoadOp::load:
            return VK_ATTACHMENT_LOAD_OP_LOAD;
        case LoadOp::dontCare:
            return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }
    return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
}

static VkAttachmentStoreOp oreStoreOpToVk(StoreOp op)
{
    switch (op)
    {
        case StoreOp::store:
            return VK_ATTACHMENT_STORE_OP_STORE;
        case StoreOp::discard:
            return VK_ATTACHMENT_STORE_OP_DONT_CARE;
    }
    return VK_ATTACHMENT_STORE_OP_DONT_CARE;
}

// ============================================================================
// Context lifecycle
// ============================================================================

#if !defined(ORE_BACKEND_GL)

Context::Context() {}

Context::~Context()
{
#if defined(ORE_BACKEND_VK)
    if (m_vkDevice == VK_NULL_HANDLE)
        return;

    // Wait for any in-flight GPU work before tearing down resources. Use
    // QueueWaitIdle to also cover the external-CB path where the host owns
    // the frame fence and may have submitted work Ore doesn't track.
    if (m_vkQueue != VK_NULL_HANDLE)
        m_vk.QueueWaitIdle(m_vkQueue);
    if (m_vkFrameFence != VK_NULL_HANDLE)
        m_vk.DestroyFence(m_vkDevice, m_vkFrameFence, nullptr);

    // Drop any BindGroups that were deferred past the final endFrame(). Their
    // descriptor sets live in m_vkPersistentDescriptorPool and must go away
    // before that pool is destroyed below, otherwise validation flags leaked
    // VkDescriptorSets at vkDestroyDevice time.
    m_deferredBindGroups.clear();

    vkDrainDeferred();

    // Drain the render pass cache.
    for (auto& [key, rp] : m_vkRenderPassCache)
        m_vk.DestroyRenderPass(m_vkDevice, rp, nullptr);
    m_vkRenderPassCache.clear();

    if (m_vkDescriptorPool != VK_NULL_HANDLE)
        m_vk.DestroyDescriptorPool(m_vkDevice, m_vkDescriptorPool, nullptr);

    // The persistent pool backs BindGroups (allocated lazily in makeBindGroup).
    // Destroying it implicitly frees any remaining descriptor sets it owns.
    if (m_vkPersistentDescriptorPool != VK_NULL_HANDLE)
        m_vk.DestroyDescriptorPool(m_vkDevice,
                                   m_vkPersistentDescriptorPool,
                                   nullptr);

    if (m_vkEmptyDSL != VK_NULL_HANDLE)
        m_vk.DestroyDescriptorSetLayout(m_vkDevice, m_vkEmptyDSL, nullptr);

    if (m_vkCommandPool != VK_NULL_HANDLE)
        m_vk.DestroyCommandPool(m_vkDevice, m_vkCommandPool, nullptr);
#endif
}

Context::Context(Context&& other) noexcept :
    m_features(other.m_features)
#if defined(ORE_BACKEND_VK)
    ,
    m_vk(other.m_vk),
    m_vkPhysicalDevice(other.m_vkPhysicalDevice),
    m_vkDevice(other.m_vkDevice),
    m_vkQueue(other.m_vkQueue),
    m_vkQueueFamily(other.m_vkQueueFamily),
    m_vmaAllocator(other.m_vmaAllocator),
    m_vkCommandPool(other.m_vkCommandPool),
    m_vkCommandBuffer(other.m_vkCommandBuffer),
    m_vkDescriptorPool(other.m_vkDescriptorPool),
    m_vkFrameFence(other.m_vkFrameFence),
    m_vkPersistentDescriptorPool(other.m_vkPersistentDescriptorPool),
    m_vkDeferredFramebuffers(std::move(other.m_vkDeferredFramebuffers)),
    m_vkDeferredStagingBuffers(std::move(other.m_vkDeferredStagingBuffers)),
    m_vkRenderPassCache(std::move(other.m_vkRenderPassCache)),
    m_deferredBindGroups(std::move(other.m_deferredBindGroups))
#endif
{
#if defined(ORE_BACKEND_VK)
    other.m_vkDevice = VK_NULL_HANDLE;
    other.m_vkCommandPool = VK_NULL_HANDLE;
    other.m_vkDescriptorPool = VK_NULL_HANDLE;
    other.m_vkPersistentDescriptorPool = VK_NULL_HANDLE;
    other.m_vkFrameFence = VK_NULL_HANDLE;
#endif
}

Context& Context::operator=(Context&& other) noexcept
{
    if (this != &other)
    {
        this->~Context();
        new (this) Context(std::move(other));
    }
    return *this;
}

// ============================================================================
// createVulkan
// ============================================================================

Context Context::createVulkan(VkInstance instance,
                              VkPhysicalDevice physicalDevice,
                              VkDevice device,
                              VkQueue queue,
                              uint32_t queueFamilyIndex,
                              VmaAllocator allocator,
                              PFN_vkGetInstanceProcAddr pfnGetInstanceProcAddr)
{
    Context ctx;
    ctx.m_vkPhysicalDevice = physicalDevice;
    ctx.m_vkDevice = device;
    ctx.m_vkQueue = queue;
    ctx.m_vkQueueFamily = queueFamilyIndex;
    ctx.m_vmaAllocator = allocator;

    // Load instance-level function pointers.
#define LOAD_INSTANCE_CMD(CMD)                                                 \
    ctx.m_vk.CMD = reinterpret_cast<PFN_vk##CMD>(                              \
        pfnGetInstanceProcAddr(instance, "vk" #CMD));
    ORE_VK_INSTANCE_COMMANDS(LOAD_INSTANCE_CMD)
#undef LOAD_INSTANCE_CMD

    // Load device-level function pointers via GetDeviceProcAddr.
#define LOAD_DEVICE_CMD(CMD)                                                   \
    ctx.m_vk.CMD = reinterpret_cast<PFN_vk##CMD>(                              \
        ctx.m_vk.GetDeviceProcAddr(device, "vk" #CMD));
    ORE_VK_DEVICE_COMMANDS(LOAD_DEVICE_CMD)
#undef LOAD_DEVICE_CMD

    // Command pool: reset-per-command-buffer for single-threaded use.
    VkCommandPoolCreateInfo poolCI{};
    poolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolCI.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolCI.queueFamilyIndex = queueFamilyIndex;
    ctx.m_vk.CreateCommandPool(device, &poolCI, nullptr, &ctx.m_vkCommandPool);

    // Single reusable command buffer.
    VkCommandBufferAllocateInfo cbAI{};
    cbAI.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbAI.commandPool = ctx.m_vkCommandPool;
    cbAI.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbAI.commandBufferCount = 1;
    ctx.m_vk.AllocateCommandBuffers(device, &cbAI, &ctx.m_vkCommandBuffer);

    // Descriptor pool: sized generously for per-frame allocation + reset.
    // 3 sets per draw × 256 draws per frame × 3 types = 2304 descriptors.
    VkDescriptorPoolSize poolSizes[3];
    poolSizes[0] = {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2048};
    poolSizes[1] = {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 2048};
    poolSizes[2] = {VK_DESCRIPTOR_TYPE_SAMPLER, 2048};

    VkDescriptorPoolCreateInfo dpCI{};
    dpCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dpCI.flags = 0; // No FREE_DESCRIPTOR_SET — we reset the whole pool.
    dpCI.maxSets = 768;
    dpCI.poolSizeCount = 3;
    dpCI.pPoolSizes = poolSizes;
    ctx.m_vk.CreateDescriptorPool(device,
                                  &dpCI,
                                  nullptr,
                                  &ctx.m_vkDescriptorPool);

    // Frame fence — signaled after each endFrame() QueueSubmit, waited on
    // in the next beginFrame() (or the destructor) so we never destroy
    // resources the GPU is still using.  Created in the signaled state so
    // the first beginFrame() can wait without blocking.
    VkFenceCreateInfo fenceCI{};
    fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCI.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    ctx.m_vk.CreateFence(device, &fenceCI, nullptr, &ctx.m_vkFrameFence);

    // Populate features from physical device properties.
    VkPhysicalDeviceProperties props{};
    ctx.m_vk.GetPhysicalDeviceProperties(physicalDevice, &props);
    VkPhysicalDeviceFeatures feat{};
    ctx.m_vk.GetPhysicalDeviceFeatures(physicalDevice, &feat);

    Features& f = ctx.m_features;
    f.colorBufferFloat = true; // All Vulkan 1.1+ support rgba16f attachments.
    f.perTargetBlend = feat.independentBlend == VK_TRUE;
    f.perTargetWriteMask = feat.independentBlend == VK_TRUE;
    f.textureViewSampling = true;
    f.drawBaseInstance = true;
    f.depthBiasClamp = feat.depthBiasClamp == VK_TRUE;
    f.anisotropicFiltering = feat.samplerAnisotropy == VK_TRUE;
    f.texture3D = true;
    f.textureArrays = true;
    f.computeShaders = true;
    f.storageBuffers = true;
    // Compressed format support via format properties.
    f.bc = feat.textureCompressionBC == VK_TRUE;
    f.etc2 = feat.textureCompressionETC2 == VK_TRUE;
    f.astc = feat.textureCompressionASTC_LDR == VK_TRUE;
    f.maxColorAttachments = props.limits.maxColorAttachments;
    f.maxTextureSize2D = props.limits.maxImageDimension2D;
    f.maxTextureSizeCube = props.limits.maxImageDimensionCube;
    f.maxTextureSize3D = props.limits.maxImageDimension3D;
    f.maxUniformBufferSize =
        static_cast<uint32_t>(props.limits.maxUniformBufferRange);
    f.maxVertexAttributes = props.limits.maxVertexInputAttributes;
    f.maxSamplers = props.limits.maxPerStageDescriptorSamplers;

    return ctx;
}

// ============================================================================
// Frame lifecycle
// ============================================================================

void Context::vkDrainDeferred()
{
    for (VkFramebuffer fb : m_vkDeferredFramebuffers)
        m_vk.DestroyFramebuffer(m_vkDevice, fb, nullptr);
    m_vkDeferredFramebuffers.clear();

    for (auto& buf : m_vkDeferredStagingBuffers)
        vmaDestroyBuffer(m_vmaAllocator, buf.buffer, buf.allocation);
    m_vkDeferredStagingBuffers.clear();

    for (auto& destroy : m_vkDeferredDestroys)
        destroy();
    m_vkDeferredDestroys.clear();
}

void Context::vkDeferDestroy(std::function<void()> destroy)
{
    if (m_vkExternalCmdBuf)
        m_vkDeferredDestroys.push_back(std::move(destroy));
    else
        destroy();
}

// Lazy-create a shared empty `VkDescriptorSetLayout`. Used to fill null
// slots in a pipeline's `pSetLayouts[]` when the user supplies a sparse
// `bindGroupLayouts[]` (e.g. `[NULL, layout1, layout2]` for a shader
// that only binds to groups 1 and 2). Vulkan VUID 06753 forbids
// VK_NULL_HANDLE in pSetLayouts without graphicsPipelineLibrary.
VkDescriptorSetLayout Context::vkGetOrCreateEmptyDSL()
{
    if (m_vkEmptyDSL != VK_NULL_HANDLE)
        return m_vkEmptyDSL;
    VkDescriptorSetLayoutCreateInfo ci{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    ci.bindingCount = 0;
    ci.pBindings = nullptr;
    m_vk.CreateDescriptorSetLayout(m_vkDevice, &ci, nullptr, &m_vkEmptyDSL);
    return m_vkEmptyDSL;
}

void Context::vkFlushPendingInitialTransitions()
{
    if (m_vkPendingInitialTransitions.empty())
        return;

    // Only transition textures that are still in UNDEFINED — another code
    // path (upload(), a prior render pass attachment) may have moved the
    // layout forward since we queued this entry.  Emitting UNDEFINED→
    // SHADER_READ_ONLY on an already-initialized image would let the driver
    // discard its contents.
    std::vector<VkImageMemoryBarrier> barriers;
    barriers.reserve(m_vkPendingInitialTransitions.size());
    for (const auto& pt : m_vkPendingInitialTransitions)
    {
        if (pt.texture->m_vkLayout != VK_IMAGE_LAYOUT_UNDEFINED)
            continue;

        VkImageMemoryBarrier b{};
        b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        b.srcAccessMask = 0;
        b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        b.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        b.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image = pt.texture->m_vkImage;
        b.subresourceRange.aspectMask = pt.aspectMask;
        b.subresourceRange.baseMipLevel = 0;
        b.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        b.subresourceRange.baseArrayLayer = 0;
        b.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
        barriers.push_back(b);
        pt.texture->m_vkLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    if (!barriers.empty())
    {
        m_vk.CmdPipelineBarrier(m_vkCommandBuffer,
                                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                                0,
                                0,
                                nullptr,
                                0,
                                nullptr,
                                static_cast<uint32_t>(barriers.size()),
                                barriers.data());
    }
    m_vkPendingInitialTransitions.clear();
}

void Context::beginFrame()
{
    // Release deferred BindGroups from last frame. endFrame() already
    // waited for GPU completion, so these are safe to destroy.
    m_deferredBindGroups.clear();

    // If the previous frame was external, its endFrame() skipped draining.
    if (!m_vkDeferredFramebuffers.empty() ||
        !m_vkDeferredStagingBuffers.empty())
    {
        vkDrainDeferred();
    }

    m_vkExternalCmdBuf = false;

    // Reset the descriptor pool — all sets from last frame are invalid.
    m_vk.ResetDescriptorPool(m_vkDevice, m_vkDescriptorPool, 0);

    // Begin the frame command buffer.
    m_vk.ResetCommandBuffer(m_vkCommandBuffer, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    m_vk.BeginCommandBuffer(m_vkCommandBuffer, &beginInfo);

    // Flush any lazy-init barriers queued between frames (e.g. BindGroups
    // created before beginFrame that referenced a still-UNDEFINED texture).
    vkFlushPendingInitialTransitions();
}

void Context::beginFrame(VkCommandBuffer externalCb)
{
    assert(externalCb != VK_NULL_HANDLE);

    m_deferredBindGroups.clear();
    vkDrainDeferred();

    m_vk.ResetDescriptorPool(m_vkDevice, m_vkDescriptorPool, 0);

    m_vkCommandBuffer = externalCb;
    m_vkExternalCmdBuf = true;

    // Flush any lazy-init barriers queued before this frame.  Host has
    // already called BeginCommandBuffer on externalCb, so recording is safe.
    vkFlushPendingInitialTransitions();
}

void Context::waitForGPU()
{
#if defined(ORE_BACKEND_VK)
    if (m_vkFrameFence != VK_NULL_HANDLE)
        m_vk.WaitForFences(m_vkDevice, 1, &m_vkFrameFence, VK_TRUE, UINT64_MAX);
#endif
}

void Context::endFrame()
{
    if (m_vkExternalCmdBuf)
    {
        // External-CB mode: host owns End/Submit/fence. Deferred destroys
        // wait until the next beginFrame() when the host has waited on the
        // prior submission.
        return;
    }

    m_vk.EndCommandBuffer(m_vkCommandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_vkCommandBuffer;
    m_vk.ResetFences(m_vkDevice, 1, &m_vkFrameFence);
    m_vk.QueueSubmit(m_vkQueue, 1, &submitInfo, m_vkFrameFence);
    // Wait for GPU completion before returning — callers destroy Ore
    // resources (textures, views, pipelines) after endFrame(), and those
    // must not be in use by the GPU.  Matches D3D12's endFrame() behavior.
    m_vk.WaitForFences(m_vkDevice, 1, &m_vkFrameFence, VK_TRUE, UINT64_MAX);

    vkDrainDeferred();
}

// ============================================================================
// getOrCreateRenderPass
// ============================================================================

VkRenderPass Context::getOrCreateRenderPass(const VKRenderPassKey& key)
{
    // Linear scan — cache is typically <10 entries.
    for (auto& [k, rp] : m_vkRenderPassCache)
    {
        if (k == key)
            return rp;
    }

    // Build attachment descriptions. Layout: [color × N][resolve × N_resolve]
    // [depth?]. Resolve attachments are interleaved after the colors so the
    // subpass's `pResolveAttachments` array indexes into a contiguous range.
    constexpr uint32_t kMaxAttachments = 9; // 4 color + 4 resolve + 1 depth.
    VkAttachmentDescription attachments[kMaxAttachments]{};
    VkAttachmentReference colorRefs[4]{};
    VkAttachmentReference resolveRefs[4]{};
    VkAttachmentReference depthRef{};
    uint32_t attachIdx = 0;
    bool anyResolve = false;

    for (uint32_t i = 0; i < key.colorCount; ++i)
    {
        VkAttachmentDescription& a = attachments[attachIdx];
        a.format = oreFormatToVkLocal(key.colorFormats[i]);
        a.samples = static_cast<VkSampleCountFlagBits>(key.sampleCount);
        a.loadOp = oreLoadOpToVk(key.colorLoadOps[i]);
        a.storeOp = oreStoreOpToVk(key.colorStoreOps[i]);
        a.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        a.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        // After finish(), color images are always in SHADER_READ_ONLY_OPTIMAL.
        // A subsequent LoadOp::load pass must start from that layout.
        a.initialLayout = (key.colorLoadOps[i] == LoadOp::load)
                              ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                              : VK_IMAGE_LAYOUT_UNDEFINED;
        a.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        colorRefs[i].attachment = attachIdx;
        colorRefs[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        ++attachIdx;
        anyResolve = anyResolve || key.colorHasResolve[i];
    }

    // Resolve attachments (single-sample). Must be in the same order as
    // colorRefs; an `attachment = VK_ATTACHMENT_UNUSED` entry signals
    // "this color attachment has no resolve target." Vulkan requires the
    // resolve and color attachment to share format and aspect.
    if (anyResolve)
    {
        for (uint32_t i = 0; i < key.colorCount; ++i)
        {
            if (!key.colorHasResolve[i])
            {
                resolveRefs[i].attachment = VK_ATTACHMENT_UNUSED;
                resolveRefs[i].layout = VK_IMAGE_LAYOUT_UNDEFINED;
                continue;
            }
            VkAttachmentDescription& a = attachments[attachIdx];
            a.format = oreFormatToVkLocal(key.colorFormats[i]);
            a.samples = VK_SAMPLE_COUNT_1_BIT;
            a.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            a.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            a.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            a.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            a.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            a.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            resolveRefs[i].attachment = attachIdx;
            resolveRefs[i].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            ++attachIdx;
        }
    }

    bool hasDepthAttach = false;
    if (key.hasDepth)
    {
        VkAttachmentDescription& a = attachments[attachIdx];
        a.format = oreFormatToVkLocal(key.depthFormat);
        a.samples = static_cast<VkSampleCountFlagBits>(key.sampleCount);
        a.loadOp = oreLoadOpToVk(key.depthLoadOp);
        a.storeOp = oreStoreOpToVk(key.depthStoreOp);
        a.stencilLoadOp = oreLoadOpToVk(key.depthLoadOp);
        a.stencilStoreOp = oreStoreOpToVk(key.depthStoreOp);
        a.initialLayout = (key.depthLoadOp == LoadOp::load)
                              ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
                              : VK_IMAGE_LAYOUT_UNDEFINED;
        a.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        depthRef.attachment = attachIdx;
        depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        ++attachIdx;
        hasDepthAttach = true;
    }

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = key.colorCount;
    subpass.pColorAttachments = colorRefs;
    subpass.pResolveAttachments = anyResolve ? resolveRefs : nullptr;
    subpass.pDepthStencilAttachment = hasDepthAttach ? &depthRef : nullptr;

    // Subpass dependency: ensure the render pass waits for prior renders before
    // writing to attachments, and makes writes visible before subsequent reads.
    VkSubpassDependency deps[2]{};
    deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    deps[0].dstSubpass = 0;
    deps[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                           VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                           VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    deps[0].srcAccessMask = 0;
    deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    deps[1].srcSubpass = 0;
    deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                           VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    // Include VERTEX_SHADER so a subsequent pass that samples this
    // attachment from a vertex shader (legal in WGSL) sees the writes
    // with proper visibility. Fragment is the common case; vertex is
    // near-free insurance. Not covered: compute, transfer — those
    // consumers must emit their own barrier.
    deps[1].dstStageMask = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                           VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    VkRenderPassCreateInfo rpCI{};
    rpCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpCI.attachmentCount = attachIdx;
    rpCI.pAttachments = attachments;
    rpCI.subpassCount = 1;
    rpCI.pSubpasses = &subpass;
    rpCI.dependencyCount = 2;
    rpCI.pDependencies = deps;

    VkRenderPass rp = VK_NULL_HANDLE;
    m_vk.CreateRenderPass(m_vkDevice, &rpCI, nullptr, &rp);

    m_vkRenderPassCache.emplace_back(key, rp);
    return rp;
}

// ============================================================================
// makeBuffer
// ============================================================================

rcp<Buffer> Context::makeBuffer(const BufferDesc& desc)
{
    auto buffer = rcp<Buffer>(new Buffer(desc.size, desc.usage));
    buffer->m_vkDevice = m_vkDevice;
    buffer->m_vmaAllocator = m_vmaAllocator;
    buffer->m_vkOreContext = this;

    VkBufferUsageFlags usage = 0;
    switch (desc.usage)
    {
        case BufferUsage::vertex:
            usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
            break;
        case BufferUsage::index:
            usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
            break;
        case BufferUsage::uniform:
            usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
            break;
    }

    VkBufferCreateInfo bufCI{};
    bufCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufCI.size = desc.size;
    bufCI.usage = usage;

    // Host-visible, persistently mapped — direct CPU writes, no staging.
    VmaAllocationCreateInfo allocCI{};
    allocCI.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    allocCI.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocationInfo allocInfo{};
    vmaCreateBuffer(m_vmaAllocator,
                    &bufCI,
                    &allocCI,
                    &buffer->m_vkBuffer,
                    &buffer->m_vmaAllocation,
                    &allocInfo);
    buffer->m_vkMappedPtr = allocInfo.pMappedData;

    if (desc.data != nullptr)
    {
        memcpy(buffer->m_vkMappedPtr, desc.data, desc.size);
    }

    return buffer;
}

// ============================================================================
// makeTexture
// ============================================================================

rcp<Texture> Context::makeTexture(const TextureDesc& desc)
{
    auto texture = rcp<Texture>(new Texture(desc));
    texture->m_vkDevice = m_vkDevice;
    texture->m_vmaAllocator = m_vmaAllocator;
    texture->m_vkOreContext = this;

    VkImageUsageFlags usage =
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (desc.renderTarget)
    {
        bool isDS = isDepthStencilFormatLocal(desc.format);
        usage |= isDS ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                      : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    }

    VkImageType imageType = VK_IMAGE_TYPE_2D;
    VkImageCreateFlags createFlags = 0;
    uint32_t arrayLayers = desc.depthOrArrayLayers;

    switch (desc.type)
    {
        case TextureType::texture2D:
            imageType = VK_IMAGE_TYPE_2D;
            arrayLayers = 1;
            break;
        case TextureType::cube:
            imageType = VK_IMAGE_TYPE_2D;
            createFlags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
            arrayLayers = 6;
            break;
        case TextureType::texture3D:
            imageType = VK_IMAGE_TYPE_3D;
            arrayLayers = 1;
            break;
        case TextureType::array2D:
            imageType = VK_IMAGE_TYPE_2D;
            break;
    }

    VkImageCreateInfo imgCI{};
    imgCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imgCI.flags = createFlags;
    imgCI.imageType = imageType;
    imgCI.format = oreFormatToVkLocal(desc.format);
    imgCI.extent = {desc.width, desc.height, 1};
    imgCI.mipLevels = desc.numMipmaps > 0 ? desc.numMipmaps : 1;
    imgCI.arrayLayers = arrayLayers;
    imgCI.samples = static_cast<VkSampleCountFlagBits>(desc.sampleCount);
    imgCI.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgCI.usage = usage;
    imgCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo allocCI{};
    allocCI.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    vmaCreateImage(m_vmaAllocator,
                   &imgCI,
                   &allocCI,
                   &texture->m_vkImage,
                   &texture->m_vmaAllocation,
                   nullptr);
    texture->m_vkLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    return texture;
}

// ============================================================================
// makeTextureView
// ============================================================================

rcp<TextureView> Context::makeTextureView(const TextureViewDesc& desc)
{
    Texture* tex = desc.texture;
    if (!tex)
        return nullptr;

    auto view = rcp<TextureView>(new TextureView(ref_rcp(tex), desc));
    view->m_vkDevice = m_vkDevice;
    view->m_vkOreContext = this;

    VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    switch (desc.aspect)
    {
        case TextureAspect::all:
            aspectMask = isDepthStencilFormatLocal(tex->format())
                             ? (VK_IMAGE_ASPECT_DEPTH_BIT |
                                (hasStencilLocal(tex->format())
                                     ? VK_IMAGE_ASPECT_STENCIL_BIT
                                     : 0u))
                             : VK_IMAGE_ASPECT_COLOR_BIT;
            break;
        case TextureAspect::depthOnly:
            aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            break;
        case TextureAspect::stencilOnly:
            aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
            break;
    }

    VkImageViewType viewType = VK_IMAGE_VIEW_TYPE_2D;
    switch (desc.dimension)
    {
        case TextureViewDimension::texture2D:
            viewType = VK_IMAGE_VIEW_TYPE_2D;
            break;
        case TextureViewDimension::cube:
            viewType = VK_IMAGE_VIEW_TYPE_CUBE;
            break;
        case TextureViewDimension::texture3D:
            viewType = VK_IMAGE_VIEW_TYPE_3D;
            break;
        case TextureViewDimension::array2D:
            viewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
            break;
        case TextureViewDimension::cubeArray:
            viewType = VK_IMAGE_VIEW_TYPE_CUBE_ARRAY;
            break;
    }

    VkImageViewCreateInfo viewCI{};
    viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewCI.image = tex->m_vkImage;
    viewCI.viewType = viewType;
    viewCI.format = oreFormatToVkLocal(tex->format());
    viewCI.subresourceRange.aspectMask = aspectMask;
    viewCI.subresourceRange.baseMipLevel = desc.baseMipLevel;
    viewCI.subresourceRange.levelCount =
        desc.mipCount > 0 ? desc.mipCount : VK_REMAINING_MIP_LEVELS;
    viewCI.subresourceRange.baseArrayLayer = desc.baseLayer;
    viewCI.subresourceRange.layerCount =
        desc.layerCount > 0 ? desc.layerCount : VK_REMAINING_ARRAY_LAYERS;

    m_vk.CreateImageView(m_vkDevice, &viewCI, nullptr, &view->m_vkImageView);
    view->m_vkDestroyImageView = m_vk.DestroyImageView;

    return view;
}

// ============================================================================
// makeSampler
// ============================================================================

rcp<Sampler> Context::makeSampler(const SamplerDesc& desc)
{
    auto sampler = rcp<Sampler>(new Sampler());
    sampler->m_vkDevice = m_vkDevice;
    sampler->m_vkOreContext = this;

    auto filterToVk = [](Filter f) {
        return f == Filter::linear ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
    };
    auto mipmapToVk = [](Filter f) {
        return f == Filter::linear ? VK_SAMPLER_MIPMAP_MODE_LINEAR
                                   : VK_SAMPLER_MIPMAP_MODE_NEAREST;
    };
    auto wrapToVk = [](WrapMode w) -> VkSamplerAddressMode {
        switch (w)
        {
            case WrapMode::repeat:
                return VK_SAMPLER_ADDRESS_MODE_REPEAT;
            case WrapMode::mirrorRepeat:
                return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
            case WrapMode::clampToEdge:
                return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        }
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    };
    auto compareToVk = [](CompareFunction c) -> VkCompareOp {
        switch (c)
        {
            case CompareFunction::none:
            case CompareFunction::always:
                return VK_COMPARE_OP_ALWAYS;
            case CompareFunction::never:
                return VK_COMPARE_OP_NEVER;
            case CompareFunction::less:
                return VK_COMPARE_OP_LESS;
            case CompareFunction::equal:
                return VK_COMPARE_OP_EQUAL;
            case CompareFunction::lessEqual:
                return VK_COMPARE_OP_LESS_OR_EQUAL;
            case CompareFunction::greater:
                return VK_COMPARE_OP_GREATER;
            case CompareFunction::notEqual:
                return VK_COMPARE_OP_NOT_EQUAL;
            case CompareFunction::greaterEqual:
                return VK_COMPARE_OP_GREATER_OR_EQUAL;
        }
        return VK_COMPARE_OP_ALWAYS;
    };

    VkSamplerCreateInfo sCI{};
    sCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    sCI.magFilter = filterToVk(desc.magFilter);
    sCI.minFilter = filterToVk(desc.minFilter);
    sCI.mipmapMode = mipmapToVk(desc.mipmapFilter);
    sCI.addressModeU = wrapToVk(desc.wrapU);
    sCI.addressModeV = wrapToVk(desc.wrapV);
    sCI.addressModeW = wrapToVk(desc.wrapW);
    sCI.mipLodBias = 0.0f;
    sCI.anisotropyEnable =
        (desc.maxAnisotropy > 1 && m_features.anisotropicFiltering) ? VK_TRUE
                                                                    : VK_FALSE;
    sCI.maxAnisotropy = static_cast<float>(desc.maxAnisotropy);
    sCI.compareEnable =
        (desc.compare != CompareFunction::none) ? VK_TRUE : VK_FALSE;
    sCI.compareOp = compareToVk(desc.compare);
    sCI.minLod = desc.minLod;
    sCI.maxLod = desc.maxLod;
    sCI.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;

    m_vk.CreateSampler(m_vkDevice, &sCI, nullptr, &sampler->m_vkSampler);
    sampler->m_vkDestroySampler = m_vk.DestroySampler;

    return sampler;
}

// ============================================================================
// makeShaderModule
// ============================================================================

rcp<ShaderModule> Context::makeShaderModule(const ShaderModuleDesc& desc)
{
    auto module = rcp<ShaderModule>(new ShaderModule());
    module->m_vkDevice = m_vkDevice;

    VkShaderModuleCreateInfo smCI{};
    smCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    smCI.codeSize = desc.codeSize;
    smCI.pCode = static_cast<const uint32_t*>(desc.code);

    VkResult result = m_vk.CreateShaderModule(m_vkDevice,
                                              &smCI,
                                              nullptr,
                                              &module->m_vkShaderModule);
    if (result != VK_SUCCESS || module->m_vkShaderModule == VK_NULL_HANDLE)
    {
        setLastError("Ore Vulkan: vkCreateShaderModule failed (VkResult=%d)",
                     static_cast<int>(result));
        return nullptr;
    }
    module->m_vkDestroyShaderModule = m_vk.DestroyShaderModule;

    module->applyBindingMapFromDesc(desc);
    return module;
}

// ============================================================================
// makeBindGroupLayout
// ============================================================================

rcp<BindGroupLayout> Context::makeBindGroupLayout(
    const BindGroupLayoutDesc& desc)
{
    if (desc.groupIndex >= kMaxBindGroups)
    {
        setLastError("makeBindGroupLayout: groupIndex %u out of range [0, %u)",
                     desc.groupIndex,
                     kMaxBindGroups);
        return nullptr;
    }
    auto layout = rcp<BindGroupLayout>(new BindGroupLayout());
    layout->m_context = this;
    layout->m_groupIndex = desc.groupIndex;
    layout->m_entries.reserve(desc.entryCount);
    for (uint32_t i = 0; i < desc.entryCount; ++i)
        layout->m_entries.push_back(desc.entries[i]);

    layout->m_vkDevice = m_vkDevice;
    layout->m_vkDestroyDescriptorSetLayout = m_vk.DestroyDescriptorSetLayout;
    layout->m_vkDSL = createDSLFromLayoutDesc(m_vk.CreateDescriptorSetLayout,
                                              m_vkDevice,
                                              desc);
    if (layout->m_vkDSL == VK_NULL_HANDLE)
    {
        setLastError("makeBindGroupLayout: vkCreateDescriptorSetLayout failed "
                     "(group=%u)",
                     desc.groupIndex);
        return nullptr;
    }
    return layout;
}

// ============================================================================
// makeBindGroup
// ============================================================================

rcp<BindGroup> Context::makeBindGroup(const BindGroupDesc& desc)
{
    if (desc.layout == nullptr)
    {
        setLastError("makeBindGroup: BindGroupDesc::layout is null");
        return nullptr;
    }
    BindGroupLayout* layout = desc.layout;
    const uint32_t groupIndex = layout->groupIndex();
    if (groupIndex >= kMaxBindGroups)
    {
        setLastError(
            "makeBindGroup: layout->groupIndex %u out of range [0, %u)",
            groupIndex,
            kMaxBindGroups);
        return nullptr;
    }

    auto bg = rcp<BindGroup>(new BindGroup());
    bg->m_context = this;
    bg->m_layoutRef = ref_rcp(layout);

    // Count dynamic-offset UBOs declared by the layout — authoritative.
    uint32_t dynamicCount = 0;
    for (const auto& e : layout->entries())
    {
        if (e.kind == BindingKind::uniformBuffer && e.hasDynamicOffset)
            ++dynamicCount;
    }
    bg->m_dynamicOffsetCount = dynamicCount;

    // Lazily create the persistent descriptor pool (supports
    // VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT so individual sets can
    // be freed when the BindGroup is destroyed).
    if (m_vkPersistentDescriptorPool == VK_NULL_HANDLE)
    {
        VkDescriptorPoolSize poolSizes[] = {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 256},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 256},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 256},
            {VK_DESCRIPTOR_TYPE_SAMPLER, 256},
        };
        VkDescriptorPoolCreateInfo ci{
            VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        ci.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        ci.maxSets = 256;
        ci.poolSizeCount = 4;
        ci.pPoolSizes = poolSizes;
        m_vk.CreateDescriptorPool(m_vkDevice,
                                  &ci,
                                  nullptr,
                                  &m_vkPersistentDescriptorPool);
    }

    // Allocate a descriptor set from the persistent pool using the layout's
    // pre-built VkDescriptorSetLayout.
    VkDescriptorSetLayout dsl = layout->m_vkDSL;
    VkDescriptorSetAllocateInfo allocInfo{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    allocInfo.descriptorPool = m_vkPersistentDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &dsl;

    VkResult result = m_vk.AllocateDescriptorSets(m_vkDevice,
                                                  &allocInfo,
                                                  &bg->m_vkDescriptorSet);
    if (result != VK_SUCCESS)
    {
        // Allocation failed — pool may be exhausted.
        return nullptr;
    }

    // Build VkWriteDescriptorSet entries for each binding. Vulkan binding
    // numbers equal WGSL `@binding` directly (Vulkan is per-set namespaced).
    // Validate against the layout's entries.
    constexpr uint32_t kMaxWrites = 32;
    VkWriteDescriptorSet writes[kMaxWrites] = {};
    VkDescriptorBufferInfo bufferInfos[kMaxWrites] = {};
    VkDescriptorImageInfo imageInfos[kMaxWrites] = {};
    uint32_t writeIdx = 0;
    uint32_t bufInfoIdx = 0;
    uint32_t imgInfoIdx = 0;

    // Resolves the layout's native descriptor binding for a WGSL @binding.
    // The SPIR-V emitter compacts sparse @binding's into per-group
    // sequential indices; the VkDescriptorSet was built using those
    // compacted indices (via createDSLFromLayoutDesc), so dstBinding has
    // to match. Falls back to the raw @binding for entries without a
    // pre-resolved native slot (forward-compat / non-shader-resolved
    // layouts) — matches Dawn's `BindGroupVk.cpp` `dstBinding =
    // uint32_t(bindingIndex)` pattern.
    auto resolveLayout = [&](uint32_t binding,
                             BindingKind expected,
                             uint32_t* outNativeBinding) -> bool {
        const BindGroupLayoutEntry* le = layout->findEntry(binding);
        if (le == nullptr)
        {
            setLastError("makeBindGroup: (group=%u, binding=%u) not declared "
                         "in BindGroupLayout",
                         groupIndex,
                         binding);
            return false;
        }
        if (le->kind != expected &&
            !((le->kind == BindingKind::sampler ||
               le->kind == BindingKind::comparisonSampler) &&
              (expected == BindingKind::sampler ||
               expected == BindingKind::comparisonSampler)))
        {
            setLastError("makeBindGroup: (group=%u, binding=%u) layout kind "
                         "mismatch",
                         groupIndex,
                         binding);
            return false;
        }
        const uint32_t kAbsent = BindGroupLayoutEntry::kNativeSlotAbsent;
        *outNativeBinding = (le->nativeSlotVS != kAbsent)   ? le->nativeSlotVS
                            : (le->nativeSlotFS != kAbsent) ? le->nativeSlotFS
                                                            : binding;
        return true;
    };

    // UBO entries.
    for (uint32_t i = 0; i < desc.uboCount; ++i)
    {
        const auto& ubo = desc.ubos[i];
        uint32_t dstBinding = 0;
        if (!resolveLayout(ubo.slot, BindingKind::uniformBuffer, &dstBinding))
            continue;
        assert(writeIdx < kMaxWrites && bufInfoIdx < kMaxWrites);

        VkDescriptorBufferInfo& bi = bufferInfos[bufInfoIdx++];
        bi.buffer = ubo.buffer->m_vkBuffer;
        bi.offset = ubo.offset;
        bi.range = (ubo.size > 0) ? ubo.size : ubo.buffer->size();

        VkWriteDescriptorSet& w = writes[writeIdx++];
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = bg->m_vkDescriptorSet;
        w.dstBinding = dstBinding;
        w.dstArrayElement = 0;
        w.descriptorCount = 1;
        w.descriptorType = layout->hasDynamicOffset(ubo.slot)
                               ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
                               : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        w.pBufferInfo = &bi;

        bg->m_retainedBuffers.push_back(ref_rcp(ubo.buffer));
    }

    // Texture entries (combined image sampler — imageView only, sampler set
    // separately via the sampler bind group).
    for (uint32_t i = 0; i < desc.textureCount; ++i)
    {
        const auto& tex = desc.textures[i];
        uint32_t dstBinding = 0;
        if (!resolveLayout(tex.slot, BindingKind::sampledTexture, &dstBinding))
            continue;
        assert(writeIdx < kMaxWrites && imgInfoIdx < kMaxWrites);

        // Lazy-init: a renderTarget-capable texture that has never been
        // uploaded or used as an attachment is still in
        // VK_IMAGE_LAYOUT_UNDEFINED, but the descriptor below claims
        // SHADER_READ_ONLY_OPTIMAL.  Queue an UNDEFINED → READ_ONLY
        // barrier — flushed at beginFrame()/beginRenderPass() before any
        // draw can reference this descriptor.  Mirrors WebGPU's
        // lazy-initialize-on-first-use semantic.  The layout is updated
        // at flush time, not here, so an intervening upload() still sees
        // UNDEFINED and emits the correct upload barrier sequence.
        Texture* baseTex = tex.view->texture();
        if (baseTex->m_vkLayout == VK_IMAGE_LAYOUT_UNDEFINED)
        {
            VkImageAspectFlags aspect =
                isDepthStencilFormatLocal(baseTex->format())
                    ? (VK_IMAGE_ASPECT_DEPTH_BIT |
                       (hasStencilLocal(baseTex->format())
                            ? VK_IMAGE_ASPECT_STENCIL_BIT
                            : 0))
                    : VK_IMAGE_ASPECT_COLOR_BIT;
            m_vkPendingInitialTransitions.push_back({ref_rcp(baseTex), aspect});
        }

        VkDescriptorImageInfo& ii = imageInfos[imgInfoIdx++];
        ii.imageView = tex.view->m_vkImageView;
        ii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        ii.sampler = VK_NULL_HANDLE; // Sampler bound separately.

        VkWriteDescriptorSet& w = writes[writeIdx++];
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = bg->m_vkDescriptorSet;
        w.dstBinding = dstBinding;
        w.dstArrayElement = 0;
        w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        w.pImageInfo = &ii;

        bg->m_retainedViews.push_back(ref_rcp(tex.view));
    }

    // Sampler entries.
    for (uint32_t i = 0; i < desc.samplerCount; ++i)
    {
        const auto& samp = desc.samplers[i];
        uint32_t dstBinding = 0;
        if (!resolveLayout(samp.slot, BindingKind::sampler, &dstBinding))
            continue;
        assert(writeIdx < kMaxWrites && imgInfoIdx < kMaxWrites);

        VkDescriptorImageInfo& ii = imageInfos[imgInfoIdx++];
        ii.sampler = samp.sampler->m_vkSampler;
        ii.imageView = VK_NULL_HANDLE;
        ii.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VkWriteDescriptorSet& w = writes[writeIdx++];
        w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w.dstSet = bg->m_vkDescriptorSet;
        w.dstBinding = dstBinding;
        w.dstArrayElement = 0;
        w.descriptorCount = 1;
        w.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        w.pImageInfo = &ii;

        bg->m_retainedSamplers.push_back(ref_rcp(samp.sampler));
    }

    if (writeIdx > 0)
    {
        m_vk.UpdateDescriptorSets(m_vkDevice, writeIdx, writes, 0, nullptr);
    }

    return bg;
}

// ============================================================================
// beginRenderPass
// ============================================================================

RenderPass Context::beginRenderPass(const RenderPassDesc& desc,
                                    std::string* outError)
{
    finishActiveRenderPass();

    // Flush lazy-init barriers before we enter the render pass — barriers
    // inside a render pass are restricted to declared self-dependencies,
    // so any UNDEFINED → SHADER_READ_ONLY_OPTIMAL transitions recorded by
    // makeBindGroup must be emitted here.
    vkFlushPendingInitialTransitions();

    RenderPass pass;
    pass.m_context = this;
    pass.m_vkContext = this;
    pass.m_vkCmdBuf = m_vkCommandBuffer;
    pass.populateAttachmentMetadata(desc);

    // Build render pass key from the actual load/store ops + formats.
    // Attachment layout in `attachViews`: [color × N][resolve × N_resolve]
    // [depth?]. Order must match `getOrCreateRenderPass`'s attachment
    // layout exactly — the framebuffer's pAttachments and the render
    // pass's attachment indices share the same numbering space.
    VKRenderPassKey key{};
    key.colorCount = desc.colorCount;
    key.sampleCount = 1;
    uint32_t passWidth = 0, passHeight = 0;

    VkImageView attachViews[9]{}; // 4 color + 4 resolve + 1 depth
    uint32_t attachCount = 0;
    VkImageView resolveViews[4]{};
    bool anyResolve = false;

    pass.m_vkColorCount = desc.colorCount;
    for (uint32_t i = 0; i < desc.colorCount; ++i)
    {
        const ColorAttachment& ca = desc.colorAttachments[i];
        assert(ca.view != nullptr);
        Texture* tex = ca.view->texture();
        key.colorFormats[i] = tex->format();
        key.colorLoadOps[i] = ca.loadOp;
        key.colorStoreOps[i] = ca.storeOp;
        key.colorHasResolve[i] = (ca.resolveTarget != nullptr);
        if (key.colorHasResolve[i])
        {
            anyResolve = true;
            resolveViews[i] = ca.resolveTarget->m_vkImageView;
        }
        if (key.sampleCount == 1)
            key.sampleCount = tex->sampleCount();
        passWidth = tex->width();
        passHeight = tex->height();
        attachViews[attachCount++] = ca.view->m_vkImageView;
        // Store image handle, layer range, and render target back-ref for
        // finish().
        pass.m_vkColorImages[i] = tex->m_vkImage;
        pass.m_vkColorBaseLayer[i] = ca.view->baseLayer();
        pass.m_vkColorLayerCount[i] = ca.view->layerCount();
        pass.m_vkColorRenderTargets[i] = ca.view->m_vkRenderTarget;
    }

    // Resolve image views must be appended in the same order as the
    // resolveRefs[] array in `getOrCreateRenderPass`. A color attachment
    // without a resolve target gets no entry here (the render pass uses
    // VK_ATTACHMENT_UNUSED in its place).
    if (anyResolve)
    {
        for (uint32_t i = 0; i < desc.colorCount; ++i)
        {
            if (key.colorHasResolve[i])
                attachViews[attachCount++] = resolveViews[i];
        }
    }

    if (desc.depthStencil.view != nullptr)
    {
        Texture* dsTex = desc.depthStencil.view->texture();
        key.hasDepth = true;
        key.depthFormat = dsTex->format();
        key.depthLoadOp = desc.depthStencil.depthLoadOp;
        key.depthStoreOp = desc.depthStencil.depthStoreOp;
        attachViews[attachCount++] = desc.depthStencil.view->m_vkImageView;
        // Store depth image handle and layer range for finish() barrier.
        pass.m_vkDepthImage = dsTex->m_vkImage;
        pass.m_vkDepthBaseLayer = desc.depthStencil.view->baseLayer();
        pass.m_vkDepthLayerCount = desc.depthStencil.view->layerCount();
        // Depth-only pass: no color attachments contributed dimensions.
        if (passWidth == 0)
        {
            passWidth = dsTex->width();
            passHeight = dsTex->height();
        }
    }

    VkRenderPass renderPass = getOrCreateRenderPass(key);

    // Framebuffer.
    VkFramebufferCreateInfo fbCI{};
    fbCI.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbCI.renderPass = renderPass;
    fbCI.attachmentCount = attachCount;
    fbCI.pAttachments = attachViews;
    fbCI.width = passWidth;
    fbCI.height = passHeight;
    fbCI.layers = 1;
    m_vk.CreateFramebuffer(m_vkDevice, &fbCI, nullptr, &pass.m_vkFramebuffer);

    // Clear values — must match the attachment ordering that
    // `getOrCreateRenderPass` builds: [color × N][resolve × R][depth?].
    // Resolve attachments use loadOp=DONT_CARE so their slots can hold
    // any value, but Vulkan still requires `clearValueCount ==
    // attachmentCount`.
    VkClearValue clearValues[9]{};
    for (uint32_t i = 0; i < desc.colorCount; ++i)
    {
        const ClearColor& c = desc.colorAttachments[i].clearColor;
        clearValues[i].color = {{c.r, c.g, c.b, c.a}};
    }
    if (key.hasDepth)
    {
        // Depth comes after the colors AND the resolve attachments.
        uint32_t resolveCount = 0;
        if (anyResolve)
        {
            for (uint32_t i = 0; i < desc.colorCount; ++i)
                if (key.colorHasResolve[i])
                    ++resolveCount;
        }
        clearValues[desc.colorCount + resolveCount].depthStencil = {
            desc.depthStencil.depthClearValue,
            desc.depthStencil.stencilClearValue};
    }

    VkRenderPassBeginInfo rpBI{};
    rpBI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBI.renderPass = renderPass;
    rpBI.framebuffer = pass.m_vkFramebuffer;
    rpBI.renderArea.offset = {0, 0};
    rpBI.renderArea.extent = {passWidth, passHeight};
    rpBI.clearValueCount = attachCount;
    rpBI.pClearValues = clearValues;

    m_vk.CmdBeginRenderPass(m_vkCommandBuffer,
                            &rpBI,
                            VK_SUBPASS_CONTENTS_INLINE);

    return pass;
}

// ============================================================================
// wrapCanvasTexture
// ============================================================================

rcp<TextureView> Context::wrapCanvasTexture(gpu::RenderCanvas* canvas)
{
    assert(canvas != nullptr);

    auto* vkTarget =
        static_cast<gpu::RenderTargetVulkan*>(canvas->renderTarget());

    VkImage image = vkTarget->targetImage();
    VkImageView imageView = vkTarget->targetImageView();
    if (image == VK_NULL_HANDLE || imageView == VK_NULL_HANDLE)
        return nullptr;

    // Derive the ore format from the actual Vulkan surface format so any MSAA
    // texture created from this descriptor matches the resolve target exactly
    // (Vulkan requires identical formats for MSAA resolve).
    auto vkFmt = vkTarget->framebufferFormat();
    TextureFormat oreFormat;
    switch (vkFmt)
    {
        case VK_FORMAT_B8G8R8A8_UNORM:
            oreFormat = TextureFormat::bgra8unorm;
            break;
        case VK_FORMAT_R16G16B16A16_SFLOAT:
            oreFormat = TextureFormat::rgba16float;
            break;
        case VK_FORMAT_A2B10G10R10_UNORM_PACK32:
            oreFormat = TextureFormat::rgb10a2unorm;
            break;
        default:
            oreFormat = TextureFormat::rgba8unorm;
            break;
    }

    TextureDesc texDesc{};
    texDesc.width = canvas->width();
    texDesc.height = canvas->height();
    texDesc.format = oreFormat;
    texDesc.type = TextureType::texture2D;
    texDesc.renderTarget = true;
    texDesc.numMipmaps = 1;
    texDesc.sampleCount = 1;

    // Borrow the VkImage — the RenderCanvas owns it.
    auto texture = rcp<Texture>(new Texture(texDesc));
    texture->m_vkImage = image;
    // Mark as not VMA-owned so onRefCntReachedZero skips the VMA free.
    texture->m_vmaAllocation = VK_NULL_HANDLE;
    texture->m_vkLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    // No destroy pointers needed — caller (canvas) owns the image lifetime.

    TextureViewDesc viewDesc{};
    viewDesc.texture = texture.get();
    viewDesc.dimension = TextureViewDimension::texture2D;
    viewDesc.baseMipLevel = 0;
    viewDesc.mipCount = 1;
    viewDesc.baseLayer = 0;
    viewDesc.layerCount = 1;

    auto view = rcp<TextureView>(new TextureView(std::move(texture), viewDesc));
    view->m_vkImageView = imageView;
    view->m_vkOreContext = this;
    // No destroy pointer — caller owns the image view lifetime.
    // Store the back-ref so RenderPass::finish() can update Rive's layout
    // tracking after the Ore render pass transitions the image.
    view->m_vkRenderTarget = vkTarget;

    return view;
}

rcp<TextureView> Context::wrapRiveTexture(gpu::Texture* gpuTex,
                                          uint32_t w,
                                          uint32_t h)
{
    if (!gpuTex)
        return nullptr;

    // On Vulkan, gpu::Texture is vkutil::Texture2D.
    auto* vkTex = static_cast<gpu::vkutil::Texture2D*>(gpuTex);
    VkImage image = vkTex->vkImage();
    VkImageView imageView = vkTex->vkImageView();
    if (image == VK_NULL_HANDLE || imageView == VK_NULL_HANDLE)
        return nullptr;

    TextureDesc texDesc{};
    texDesc.width = w;
    texDesc.height = h;
    texDesc.format = TextureFormat::rgba8unorm; // Images are decoded to RGBA8.
    texDesc.type = TextureType::texture2D;
    texDesc.renderTarget = false;
    texDesc.numMipmaps = 1;
    texDesc.sampleCount = 1;

    // Transition the Rive-owned image to SHADER_READ_ONLY_OPTIMAL in the
    // current command buffer so Ore can sample it. Requires an open frame
    // CB — in external-CB mode this lands in the host's CB (same submission
    // as Rive's writes), which is the only way this works correctly for the
    // Rive-writes → Ore-reads direction. Rive's lastAccess tracker is
    // updated by barrier() so Rive's subsequent uses stay consistent.
    assert(m_vkCommandBuffer != VK_NULL_HANDLE &&
           "wrapRiveTexture requires an open frame: call beginFrame() first");
    vkTex->prepareForFragmentShaderRead(m_vkCommandBuffer);

    auto texture = rcp<Texture>(new Texture(texDesc));
    texture->m_vkImage = image;
    texture->m_vmaAllocation = VK_NULL_HANDLE; // Borrowed, not VMA-owned.
    texture->m_vkLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    TextureViewDesc viewDesc{};
    viewDesc.texture = texture.get();
    viewDesc.dimension = TextureViewDimension::texture2D;
    viewDesc.baseMipLevel = 0;
    viewDesc.mipCount = 1;
    viewDesc.baseLayer = 0;
    viewDesc.layerCount = 1;

    auto view = rcp<TextureView>(new TextureView(std::move(texture), viewDesc));
    view->m_vkImageView = imageView;
    view->m_vkOreContext = this;
    return view;
}

#endif // !ORE_BACKEND_GL

} // namespace rive::ore
