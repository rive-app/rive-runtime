/*
 * Copyright 2025 Rive
 */

// Combined Vulkan + GL Context implementation for Android.
// Compiled when both ORE_BACKEND_VK and ORE_BACKEND_GL are active.
// Provides all Context method definitions with runtime dispatch via
// Context::BackendType so Vulkan and GL contexts can coexist in the same
// binary.

#if defined(ORE_BACKEND_VK) && defined(ORE_BACKEND_GL)

// Pull in Vulkan static helpers (oreFormatToVkLocal, oreLoadOpToVk, etc.).
// The !defined(ORE_BACKEND_GL) guard in ore_context_vulkan.cpp excludes
// Context method bodies, so we get only the helper functions.
#include "ore_context_vulkan.cpp"

// Pull in GL static helpers (oreFormatToGLInternal, oreFilterToGL, etc.).
// The !defined(ORE_BACKEND_METAL) && !defined(ORE_BACKEND_VK) guard in
// ore_context_gl.cpp excludes Context method bodies, so we get only the
// helper functions.
#include "../gl/ore_context_gl.cpp"

// Pull in VK pipeline static helpers (oreVertexFormatToVk, createDSL, etc.).
// The !defined(ORE_BACKEND_GL) guard in ore_pipeline_vulkan.cpp excludes
// Context::makePipeline and Pipeline::onRefCntReachedZero method bodies,
// so we get only the helper functions.
#include "ore_pipeline_vulkan.cpp"

// GL render-target header for wrapCanvasTexture (GL path).
#include "rive/renderer/gl/render_target_gl.hpp"

// VK render-target header for wrapCanvasTexture (VK path).
#include "rive/renderer/vulkan/render_target_vulkan.hpp"

#include <string>

namespace rive::ore
{

// ============================================================================
// Context lifecycle (Vulkan + GL dispatch)
// ============================================================================

Context::Context() {}

Context::~Context()
{
    if (m_backendType == BackendType::GL)
        return; // GL Context holds no owned GPU resources.

    if (m_vkDevice == VK_NULL_HANDLE)
        return;

    // Wait for any in-flight GPU work before tearing down resources. Use
    // QueueWaitIdle so we also cover the external-CB path where the host
    // owns the fence and may have submitted work Ore doesn't track.
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

    // Destroy transient pool first, then persistent — matches
    // ore_context_vulkan.cpp's destructor ordering. Keeping the two variants
    // consistent avoids surprises if a future change adds explicit
    // FreeDescriptorSets calls in BindGroup teardown.
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
}

Context::Context(Context&& other) noexcept :
    m_features(other.m_features),
    m_backendType(other.m_backendType),
    m_savedState(other.m_savedState),
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
    m_vkRenderPassCache(std::move(other.m_vkRenderPassCache))
{
    other.m_vkDevice = VK_NULL_HANDLE;
    other.m_vkCommandPool = VK_NULL_HANDLE;
    other.m_vkDescriptorPool = VK_NULL_HANDLE;
    other.m_vkPersistentDescriptorPool = VK_NULL_HANDLE;
    other.m_vkFrameFence = VK_NULL_HANDLE;
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
// createVulkan / createGL
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
    ctx.m_backendType = BackendType::Vulkan;
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
    // 3 sets per draw x 256 draws per frame x 3 types = 2304 descriptors.
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

Context Context::createGL()
{
    Context ctx;
    ctx.m_backendType = BackendType::GL;

    Features& f = ctx.m_features;
    f.colorBufferFloat = false;
    f.perTargetBlend = false;
    f.perTargetWriteMask = false;
    f.textureViewSampling = false;
    f.drawBaseInstance = false;
    f.depthBiasClamp = false;
    f.anisotropicFiltering = false;
    f.texture3D = true;
    f.textureArrays = true;
    f.computeShaders = false;
    f.storageBuffers = false;
    f.bc = false;
    f.etc2 = true;
    f.astc = false;

    GLint maxTexSize = 0, maxCubeSize = 0, max3DSize = 0, maxUBOSize = 0;
    GLint maxDrawBuffers = 0, maxVertexAttribs = 0, maxTexUnits = 0;
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);
    glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &maxCubeSize);
    glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &max3DSize);
    glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &maxUBOSize);
    glGetIntegerv(GL_MAX_DRAW_BUFFERS, &maxDrawBuffers);
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxVertexAttribs);
    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maxTexUnits);

    f.maxTextureSize2D = maxTexSize;
    f.maxTextureSizeCube = maxCubeSize;
    f.maxTextureSize3D = max3DSize;
    f.maxUniformBufferSize = maxUBOSize;
    f.maxColorAttachments = std::min(static_cast<uint32_t>(maxDrawBuffers), 4u);
    f.maxVertexAttributes = maxVertexAttribs;
    f.maxSamplers = maxTexUnits;

    GLint numExtensions = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
    for (GLint i = 0; i < numExtensions; ++i)
    {
        const char* ext =
            reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, i));
        if (!ext)
            continue;
        if (strcmp(ext, "GL_EXT_color_buffer_float") == 0)
            f.colorBufferFloat = true;
        else if (strcmp(ext, "GL_EXT_texture_filter_anisotropic") == 0)
            f.anisotropicFiltering = true;
        else if (strcmp(ext, "GL_KHR_texture_compression_astc_ldr") == 0)
            f.astc = true;
#ifdef RIVE_DESKTOP_GL
        else if (strcmp(ext, "GL_EXT_texture_compression_s3tc") == 0)
            f.bc = true;
#endif
    }

    return ctx;
}

// ============================================================================
// beginFrame / endFrame
// ============================================================================

// Drain resources deferred during the previous frame. Safe to call when the
// caller has ensured the previous frame's GPU work has completed — either via
// our own fence wait (owned-CB mode) or via the host's fence wait before
// re-entering Ore (external-CB mode).
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
    // Release deferred BindGroups from last frame. By beginFrame() the
    // caller has waited for the previous frame's GPU work to complete.
    m_deferredBindGroups.clear();

    if (m_backendType == BackendType::GL)
    {
        glGetIntegerv(GL_CURRENT_PROGRAM, &m_savedState.program);
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &m_savedState.arrayBuffer);
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &m_savedState.framebuffer);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &m_savedState.vertexArray);
        // NOTE: GL_ELEMENT_ARRAY_BUFFER is VAO state — not saved/restored
        // separately to avoid corrupting the host renderer's VAO bindings.
        return;
    }

    // If the previous frame was external, its deferred lists weren't drained
    // at its endFrame(). Host contract: prior frame's fence is waited before
    // re-entering Ore, so draining now is safe.
    if (!m_vkDeferredFramebuffers.empty() ||
        !m_vkDeferredStagingBuffers.empty())
    {
        vkDrainDeferred();
    }

    m_vkExternalCmdBuf = false;

    // VK path: reset pools, begin command buffer.
    // No fence wait here — endFrame() already waited for GPU completion.

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
    assert(m_backendType == BackendType::Vulkan);

    m_deferredBindGroups.clear();

    // Prior-frame cleanup: safe under the external-CB host contract (host
    // has waited on the prior submission's fence before re-entering Ore).
    vkDrainDeferred();

    // Reset the descriptor pool — all sets from last frame are invalid.
    m_vk.ResetDescriptorPool(m_vkDevice, m_vkDescriptorPool, 0);

    // The host has already called BeginCommandBuffer on this CB. We just
    // record into it.
    m_vkCommandBuffer = externalCb;
    m_vkExternalCmdBuf = true;

    // Flush any lazy-init barriers queued before this frame.
    vkFlushPendingInitialTransitions();
}

void Context::waitForGPU()
{
    if (m_backendType == BackendType::GL)
        return; // GL is synchronous — no-op.

    if (m_vkFrameFence != VK_NULL_HANDLE)
        m_vk.WaitForFences(m_vkDevice, 1, &m_vkFrameFence, VK_TRUE, UINT64_MAX);
}

void Context::endFrame()
{
    if (m_backendType == BackendType::GL)
    {
        // Guard each restore with the matching `glIs*` probe so Mac
        // ANGLE's strict validation doesn't fire if the host renderer
        // deleted the saved object between beginFrame and now. Name ==
        // 0 always refers to the default object (no validation error).
        // See ore_context_gl.cpp::endFrame for the mirror comment.
        if (m_savedState.program == 0 ||
            glIsProgram(m_savedState.program) == GL_TRUE)
        {
            glUseProgram(m_savedState.program);
        }
        if (m_savedState.vertexArray == 0 ||
            glIsVertexArray(m_savedState.vertexArray) == GL_TRUE)
        {
            glBindVertexArray(m_savedState.vertexArray);
        }
        if (m_savedState.arrayBuffer == 0 ||
            glIsBuffer(m_savedState.arrayBuffer) == GL_TRUE)
        {
            glBindBuffer(GL_ARRAY_BUFFER, m_savedState.arrayBuffer);
        }
        // NOTE: GL_ELEMENT_ARRAY_BUFFER is VAO state — not restored here.
        // Restoring the VAO above already restores its element buffer.
        if (m_savedState.framebuffer == 0 ||
            glIsFramebuffer(m_savedState.framebuffer) == GL_TRUE)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, m_savedState.framebuffer);
        }
        return;
    }

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
// getOrCreateRenderPass (VK-only helper, called from VK code paths)
// ============================================================================

VkRenderPass Context::getOrCreateRenderPass(const VKRenderPassKey& key)
{
    // Linear scan — cache is typically <10 entries.
    for (auto& [k, rp] : m_vkRenderPassCache)
    {
        if (k == key)
            return rp;
    }

    // Build attachment descriptions. See ore_context_vulkan.cpp's
    // mirror for the rationale; this is the vk+gl combined-build copy.
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
    if (m_backendType == BackendType::GL)
    {
        auto buffer = rcp<Buffer>(new Buffer(desc.size, desc.usage));
        glGenBuffers(1, &buffer->m_glBuffer);
        switch (desc.usage)
        {
            case BufferUsage::vertex:
                buffer->m_glTarget = GL_ARRAY_BUFFER;
                break;
            case BufferUsage::index:
                buffer->m_glTarget = GL_ELEMENT_ARRAY_BUFFER;
                break;
            case BufferUsage::uniform:
                buffer->m_glTarget = GL_UNIFORM_BUFFER;
                break;
        }
        glBindBuffer(buffer->m_glTarget, buffer->m_glBuffer);
        GLenum glUsage =
            (desc.data != nullptr) ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW;
        glBufferData(buffer->m_glTarget, desc.size, desc.data, glUsage);
        glBindBuffer(buffer->m_glTarget, 0);
        return buffer;
    }

    // VK path.
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
    if (m_backendType == BackendType::GL)
    {
        auto texture = rcp<Texture>(new Texture(desc));
        glGenTextures(1, &texture->m_glTexture);
        texture->m_glTarget = oreTextureTypeToGLTarget(desc.type);
        GLenum internalFormat = oreFormatToGLInternal(desc.format);
        glBindTexture(texture->m_glTarget, texture->m_glTexture);
        switch (desc.type)
        {
            case TextureType::texture2D:
                glTexStorage2D(GL_TEXTURE_2D,
                               desc.numMipmaps,
                               internalFormat,
                               desc.width,
                               desc.height);
                break;
            case TextureType::cube:
                glTexStorage2D(GL_TEXTURE_CUBE_MAP,
                               desc.numMipmaps,
                               internalFormat,
                               desc.width,
                               desc.height);
                break;
            case TextureType::texture3D:
                glTexStorage3D(GL_TEXTURE_3D,
                               desc.numMipmaps,
                               internalFormat,
                               desc.width,
                               desc.height,
                               desc.depthOrArrayLayers);
                break;
            case TextureType::array2D:
                glTexStorage3D(GL_TEXTURE_2D_ARRAY,
                               desc.numMipmaps,
                               internalFormat,
                               desc.width,
                               desc.height,
                               desc.depthOrArrayLayers);
                break;
        }
        glTexParameteri(texture->m_glTarget, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(texture->m_glTarget, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(texture->m_glTarget,
                        GL_TEXTURE_WRAP_S,
                        GL_CLAMP_TO_EDGE);
        glTexParameteri(texture->m_glTarget,
                        GL_TEXTURE_WRAP_T,
                        GL_CLAMP_TO_EDGE);
        glBindTexture(texture->m_glTarget, 0);
        return texture;
    }

    // VK path.
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
    if (m_backendType == BackendType::GL)
    {
        Texture* tex = desc.texture;
        assert(tex != nullptr);
        auto view = rcp<TextureView>(new TextureView(ref_rcp(tex), desc));
        view->m_glTextureView = 0;
        return view;
    }

    // VK path.
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
    if (m_backendType == BackendType::GL)
    {
        auto sampler = rcp<Sampler>(new Sampler());
        glGenSamplers(1, &sampler->m_glSampler);
        GLuint s = sampler->m_glSampler;
        if (desc.maxLod > 0.0f)
        {
            glSamplerParameteri(
                s,
                GL_TEXTURE_MIN_FILTER,
                oreMipmapFilterToGL(desc.minFilter, desc.mipmapFilter));
        }
        else
        {
            glSamplerParameteri(s,
                                GL_TEXTURE_MIN_FILTER,
                                oreFilterToGL(desc.minFilter));
        }
        glSamplerParameteri(s,
                            GL_TEXTURE_MAG_FILTER,
                            oreFilterToGL(desc.magFilter));
        glSamplerParameteri(s, GL_TEXTURE_WRAP_S, oreWrapToGL(desc.wrapU));
        glSamplerParameteri(s, GL_TEXTURE_WRAP_T, oreWrapToGL(desc.wrapV));
        glSamplerParameteri(s, GL_TEXTURE_WRAP_R, oreWrapToGL(desc.wrapW));
        glSamplerParameterf(s, GL_TEXTURE_MIN_LOD, desc.minLod);
        glSamplerParameterf(s, GL_TEXTURE_MAX_LOD, desc.maxLod);
        if (desc.compare != CompareFunction::none)
        {
            glSamplerParameteri(s,
                                GL_TEXTURE_COMPARE_MODE,
                                GL_COMPARE_REF_TO_TEXTURE);
            glSamplerParameteri(s,
                                GL_TEXTURE_COMPARE_FUNC,
                                oreCompareFunctionToGL(desc.compare));
        }
        return sampler;
    }

    // VK path.
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
    if (m_backendType == BackendType::GL)
    {
        auto module = rcp<ShaderModule>(new ShaderModule());
        const char* source = static_cast<const char*>(desc.code);
        bool isVertex = (strstr(source, "gl_Position") != nullptr);
        module->m_glShaderType =
            isVertex ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER;
        module->m_glShader = glCreateShader(module->m_glShaderType);
        GLint length = static_cast<GLint>(desc.codeSize);
        glShaderSource(module->m_glShader, 1, &source, &length);
        glCompileShader(module->m_glShader);
        GLint status = 0;
        glGetShaderiv(module->m_glShader, GL_COMPILE_STATUS, &status);
        if (!status)
        {
            char log[1024] = {};
            GLint logLen = 0;
            glGetShaderiv(module->m_glShader, GL_INFO_LOG_LENGTH, &logLen);
            if (logLen > 0)
            {
                glGetShaderInfoLog(module->m_glShader,
                                   std::min(logLen, (GLint)sizeof(log)),
                                   nullptr,
                                   log);
            }
            setLastError("Ore GL shader compile error: %s", log);
            glDeleteShader(module->m_glShader);
            return nullptr;
        }
        module->applyBindingMapFromDesc(desc);
        return module;
    }

    // VK path.
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

    if (m_backendType == BackendType::GL)
    {
        // GL has no native layout object — entries-only suffices.
        return layout;
    }

    // VK path.
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
// makePipeline
// ============================================================================

// Pipeline::onRefCntReachedZero is provided by ore_resources_vk_gl.cpp.

rcp<Pipeline> Context::makePipeline(const PipelineDesc& desc,
                                    std::string* outError)
{
    if (m_backendType == BackendType::GL)
    {
        auto pipeline = rcp<Pipeline>(new Pipeline(desc));
        // Validate user-supplied layouts against shader binding map.
        std::string err;
        if (!validateLayoutsAgainstBindingMap(pipeline->m_bindingMap,
                                              desc.bindGroupLayouts,
                                              desc.bindGroupLayoutCount,
                                              &err))
        {
            if (outError)
                *outError = err;
            else
                setLastError("makePipeline: %s", err.c_str());
            return nullptr;
        }
        pipeline->m_glProgram = glCreateProgram();
        glAttachShader(pipeline->m_glProgram, desc.vertexModule->m_glShader);
        glAttachShader(pipeline->m_glProgram, desc.fragmentModule->m_glShader);
        for (uint32_t bufIdx = 0; bufIdx < desc.vertexBufferCount; ++bufIdx)
        {
            const auto& layout = desc.vertexBuffers[bufIdx];
            for (uint32_t attrIdx = 0; attrIdx < layout.attributeCount;
                 ++attrIdx)
            {
                const auto& attr = layout.attributes[attrIdx];
                char name[32];
                snprintf(name, sizeof(name), "a_attr%u", attr.shaderSlot);
                glBindAttribLocation(pipeline->m_glProgram,
                                     attr.shaderSlot,
                                     name);
            }
        }
        glLinkProgram(pipeline->m_glProgram);
        GLint status = 0;
        glGetProgramiv(pipeline->m_glProgram, GL_LINK_STATUS, &status);
        if (!status)
        {
            std::string linkLog = "GL program link failed";
            GLint logLen = 0;
            glGetProgramiv(pipeline->m_glProgram, GL_INFO_LOG_LENGTH, &logLen);
            if (logLen > 0)
            {
                char log[1024];
                glGetProgramInfoLog(pipeline->m_glProgram,
                                    std::min(logLen, (GLint)sizeof(log)),
                                    nullptr,
                                    log);
                linkLog = log;
            }
            setLastError("Ore GL program link error: %s", linkLog.c_str());
            if (outError)
                *outError = std::move(linkLog);
            return nullptr;
        }
        oreGLFixupProgramBindings(pipeline->m_glProgram,
                                  desc.vertexModule,
                                  desc.fragmentModule);

        // (Removed: a `m_glState` cache that was populated here but
        // never consulted by `setPipeline`. Mirror of the dead-cache
        // cleanup in ore_context_gl.cpp.)
        return pipeline;
    }

    // VK path.
    auto pipeline = rcp<Pipeline>(new Pipeline(desc));
    pipeline->m_vkDevice = m_vkDevice;
    pipeline->m_vkOreContext = this;
    pipeline->m_vkTopology = oreTopologyToVk(desc.topology);

    // --- Validate user-supplied layouts against shader binding map ---
    {
        std::string err;
        if (!validateLayoutsAgainstBindingMap(pipeline->m_bindingMap,
                                              desc.bindGroupLayouts,
                                              desc.bindGroupLayoutCount,
                                              &err))
        {
            if (outError)
                *outError = err;
            else
                setLastError("makePipeline: %s", err.c_str());
            return nullptr;
        }
    }

    // --- Pipeline layout — composed from user-supplied BGLs ---
    // Null slots get the shared empty DSL (Vulkan VUID 06753 forbids
    // VK_NULL_HANDLE in pSetLayouts without graphicsPipelineLibrary).
    VkDescriptorSetLayout dsls[kVkMaxGroups] = {};
    VkDescriptorSetLayout emptyDSL = VK_NULL_HANDLE;
    for (uint32_t g = 0; g < desc.bindGroupLayoutCount && g < kVkMaxGroups; ++g)
    {
        if (desc.bindGroupLayouts[g] != nullptr)
        {
            dsls[g] = desc.bindGroupLayouts[g]->m_vkDSL;
        }
        else
        {
            if (emptyDSL == VK_NULL_HANDLE)
                emptyDSL = vkGetOrCreateEmptyDSL();
            dsls[g] = emptyDSL;
        }
    }
    VkPipelineLayoutCreateInfo layoutCI{};
    layoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutCI.setLayoutCount = desc.bindGroupLayoutCount;
    layoutCI.pSetLayouts = desc.bindGroupLayoutCount > 0 ? dsls : nullptr;
    m_vk.CreatePipelineLayout(m_vkDevice,
                              &layoutCI,
                              nullptr,
                              &pipeline->m_vkPipelineLayout);
    pipeline->m_vkDestroyPipelineLayout = m_vk.DestroyPipelineLayout;

    // --- Shader stages ---
    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = desc.vertexModule->m_vkShaderModule;
    stages[0].pName = desc.vertexEntryPoint;

    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = desc.fragmentModule->m_vkShaderModule;
    stages[1].pName = desc.fragmentEntryPoint;

    // --- Vertex input ---
    constexpr uint32_t kMaxBindings = 8;
    constexpr uint32_t kMaxAttribs = 32;
    VkVertexInputBindingDescription bindings[kMaxBindings];
    VkVertexInputAttributeDescription attribs[kMaxAttribs];
    uint32_t attribIdx = 0;

    for (uint32_t b = 0; b < desc.vertexBufferCount; ++b)
    {
        const VertexBufferLayout& layout = desc.vertexBuffers[b];
        bindings[b].binding = b;
        bindings[b].stride = layout.stride;
        bindings[b].inputRate = (layout.stepMode == VertexStepMode::instance)
                                    ? VK_VERTEX_INPUT_RATE_INSTANCE
                                    : VK_VERTEX_INPUT_RATE_VERTEX;
        for (uint32_t a = 0; a < layout.attributeCount; ++a)
        {
            assert(attribIdx < kMaxAttribs);
            const VertexAttribute& attr = layout.attributes[a];
            attribs[attribIdx].location = attr.shaderSlot;
            attribs[attribIdx].binding = b;
            attribs[attribIdx].format = oreVertexFormatToVk(attr.format);
            attribs[attribIdx].offset = attr.offset;
            ++attribIdx;
        }
    }

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = desc.vertexBufferCount;
    vertexInput.pVertexBindingDescriptions = bindings;
    vertexInput.vertexAttributeDescriptionCount = attribIdx;
    vertexInput.pVertexAttributeDescriptions = attribs;

    // --- Input assembly ---
    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = pipeline->m_vkTopology;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    // --- Viewport / scissor (dynamic) ---
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    // --- Rasterization ---
    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = oreCullModeToVk(desc.cullMode);
    raster.frontFace = oreWindingToVk(desc.winding);
    raster.lineWidth = 1.0f;
    if (desc.depthStencil.depthBias != 0 ||
        desc.depthStencil.depthBiasSlopeScale != 0.0f)
    {
        raster.depthBiasEnable = VK_TRUE;
        raster.depthBiasConstantFactor =
            static_cast<float>(desc.depthStencil.depthBias);
        raster.depthBiasSlopeFactor = desc.depthStencil.depthBiasSlopeScale;
        raster.depthBiasClamp = desc.depthStencil.depthBiasClamp;
    }

    // --- Multisample ---
    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType =
        VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples =
        static_cast<VkSampleCountFlagBits>(desc.sampleCount);

    // --- Depth / stencil ---
    // Use rgba8unorm as a sentinel for "no depth/stencil attachment".
    // Vulkan forbids enabling depth/stencil test when the render pass has no
    // depth/stencil attachment
    // (VUID-VkGraphicsPipelineCreateInfo-renderPass-07752).
    const bool hasDepthStencil =
        (desc.depthStencil.format != TextureFormat::rgba8unorm);

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType =
        VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable =
        (hasDepthStencil &&
         desc.depthStencil.depthCompare != CompareFunction::always)
            ? VK_TRUE
            : VK_FALSE;
    depthStencil.depthWriteEnable =
        (hasDepthStencil && desc.depthStencil.depthWriteEnabled) ? VK_TRUE
                                                                 : VK_FALSE;
    depthStencil.depthCompareOp =
        oreCompareFuncToVk(desc.depthStencil.depthCompare);
    depthStencil.stencilTestEnable =
        hasDepthStencil && hasStencilLocal(desc.depthStencil.format) ? VK_TRUE
                                                                     : VK_FALSE;
    depthStencil.front = oreStencilFaceToVk(desc.stencilFront,
                                            desc.stencilReadMask,
                                            desc.stencilWriteMask);
    depthStencil.back = oreStencilFaceToVk(desc.stencilBack,
                                           desc.stencilReadMask,
                                           desc.stencilWriteMask);

    // --- Color blend ---
    VkPipelineColorBlendAttachmentState blendAttachments[4]{};
    for (uint32_t i = 0; i < desc.colorCount; ++i)
    {
        const ColorTargetState& ct = desc.colorTargets[i];
        VkPipelineColorBlendAttachmentState& ba = blendAttachments[i];
        ba.colorWriteMask = oreColorWriteMaskToVk(ct.writeMask);
        ba.blendEnable = ct.blendEnabled ? VK_TRUE : VK_FALSE;
        if (ct.blendEnabled)
        {
            ba.srcColorBlendFactor = oreBlendFactorToVk(ct.blend.srcColor);
            ba.dstColorBlendFactor = oreBlendFactorToVk(ct.blend.dstColor);
            ba.colorBlendOp = oreBlendOpToVk(ct.blend.colorOp);
            ba.srcAlphaBlendFactor = oreBlendFactorToVk(ct.blend.srcAlpha);
            ba.dstAlphaBlendFactor = oreBlendFactorToVk(ct.blend.dstAlpha);
            ba.alphaBlendOp = oreBlendOpToVk(ct.blend.alphaOp);
        }
    }

    VkPipelineColorBlendStateCreateInfo colorBlend{};
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount = desc.colorCount;
    colorBlend.pAttachments = blendAttachments;

    // --- Dynamic state ---
    constexpr VkDynamicState kDynamicStates[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_STENCIL_REFERENCE,
        VK_DYNAMIC_STATE_BLEND_CONSTANTS,
    };
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(
        sizeof(kDynamicStates) / sizeof(kDynamicStates[0]));
    dynamicState.pDynamicStates = kDynamicStates;

    // --- Render pass (format-only, DONT_CARE ops for pipeline compatibility)
    // --- Build a VKRenderPassKey from the pipeline's color/depth formats.
    struct VKRenderPassKey rpKey{};
    rpKey.colorCount = desc.colorCount;
    rpKey.sampleCount = desc.sampleCount;
    for (uint32_t i = 0; i < desc.colorCount; ++i)
    {
        rpKey.colorFormats[i] = desc.colorTargets[i].format;
        rpKey.colorLoadOps[i] = LoadOp::dontCare;
        rpKey.colorStoreOps[i] = StoreOp::discard;
    }
    if (desc.depthStencil.format != TextureFormat::rgba8unorm)
    {
        rpKey.depthFormat = desc.depthStencil.format;
        rpKey.depthLoadOp = LoadOp::dontCare;
        rpKey.depthStoreOp = StoreOp::discard;
        rpKey.hasDepth = true;
    }

    VkRenderPass compatRenderPass = getOrCreateRenderPass(rpKey);

    // --- Assemble VkGraphicsPipelineCreateInfo ---
    VkGraphicsPipelineCreateInfo pipelineCI{};
    pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCI.stageCount = 2;
    pipelineCI.pStages = stages;
    pipelineCI.pVertexInputState = &vertexInput;
    pipelineCI.pInputAssemblyState = &inputAssembly;
    pipelineCI.pViewportState = &viewportState;
    pipelineCI.pRasterizationState = &raster;
    pipelineCI.pMultisampleState = &multisample;
    pipelineCI.pDepthStencilState = &depthStencil;
    pipelineCI.pColorBlendState = &colorBlend;
    pipelineCI.pDynamicState = &dynamicState;
    pipelineCI.layout = pipeline->m_vkPipelineLayout;
    pipelineCI.renderPass = compatRenderPass;
    pipelineCI.subpass = 0;

    VkResult vkr = m_vk.CreateGraphicsPipelines(m_vkDevice,
                                                VK_NULL_HANDLE,
                                                1,
                                                &pipelineCI,
                                                nullptr,
                                                &pipeline->m_vkPipeline);
    if (vkr != VK_SUCCESS)
    {
        setLastError("Ore Vulkan: vkCreateGraphicsPipelines failed (%d)", vkr);
        if (outError)
            *outError = m_lastError;
        return nullptr;
    }
    pipeline->m_vkDestroyPipeline = m_vk.DestroyPipeline;

    return pipeline;
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

    // Resolve native slot from layout entry. GL is stage-shared; prefer
    // VS slot, fall back to FS. kNativeSlotAbsent means the layout
    // wasn't shader-resolved (caller forgot to use makeLayoutFromShader).
    auto nativeSlotForGL =
        [&](uint32_t binding, BindingKind expected, uint32_t* outSlot) -> bool {
        const BindGroupLayoutEntry* le = layout->findEntry(binding);
        if (le == nullptr)
        {
            setLastError("makeBindGroup: (group=%u, binding=%u) not declared "
                         "in BindGroupLayout",
                         groupIndex,
                         binding);
            return false;
        }
        const bool kindOK = le->kind == expected ||
                            ((le->kind == BindingKind::sampler ||
                              le->kind == BindingKind::comparisonSampler) &&
                             (expected == BindingKind::sampler ||
                              expected == BindingKind::comparisonSampler));
        if (!kindOK)
        {
            setLastError("makeBindGroup: (group=%u, binding=%u) layout kind "
                         "mismatch",
                         groupIndex,
                         binding);
            return false;
        }
        uint32_t slot =
            (le->nativeSlotVS != BindGroupLayoutEntry::kNativeSlotAbsent)
                ? le->nativeSlotVS
                : le->nativeSlotFS;
        if (slot == BindGroupLayoutEntry::kNativeSlotAbsent)
        {
            setLastError("makeBindGroup: (group=%u, binding=%u) layout has "
                         "no resolved native slot — call makeLayoutFromShader",
                         groupIndex,
                         binding);
            return false;
        }
        *outSlot = slot;
        return true;
    };

    if (m_backendType == BackendType::GL)
    {
        auto bg = rcp<BindGroup>(new BindGroup());
        bg->m_context = this;
        bg->m_layoutRef = ref_rcp(layout);

        for (uint32_t i = 0; i < std::min(desc.uboCount, 8u); i++)
        {
            const auto& u = desc.ubos[i];
            BindGroup::GLUBOBinding binding{};
            binding.buffer = u.buffer->m_glBuffer;
            binding.offset = u.offset;
            binding.size = u.size ? u.size : u.buffer->size();
            binding.binding = u.slot;
            if (!nativeSlotForGL(u.slot,
                                 BindingKind::uniformBuffer,
                                 &binding.slot))
                continue;
            binding.hasDynamicOffset = layout->hasDynamicOffset(u.slot);
            if (binding.hasDynamicOffset)
                bg->m_dynamicOffsetCount++;
            bg->m_glUBOs.push_back(binding);
            bg->m_retainedBuffers.push_back(ref_rcp(u.buffer));
        }
        // Sort UBOs by WGSL @binding so `dynamicOffsets[]` is consumed in
        // BGL-entry order regardless of caller-supplied `desc.ubos[]` shape.
        std::sort(bg->m_glUBOs.begin(),
                  bg->m_glUBOs.end(),
                  [](const BindGroup::GLUBOBinding& a,
                     const BindGroup::GLUBOBinding& b) {
                      return a.binding < b.binding;
                  });

        for (uint32_t i = 0; i < std::min(desc.textureCount, 8u); i++)
        {
            const auto& t = desc.textures[i];
            BindGroup::GLTexBinding binding{};
            binding.texture = t.view->m_glTextureView != 0
                                  ? t.view->m_glTextureView
                                  : t.view->texture()->m_glTexture;
            binding.target = t.view->texture()->m_glTarget;
            if (!nativeSlotForGL(t.slot,
                                 BindingKind::sampledTexture,
                                 &binding.slot))
                continue;
            bg->m_glTextures.push_back(binding);
            bg->m_retainedViews.push_back(ref_rcp(t.view));
        }

        for (uint32_t i = 0; i < std::min(desc.samplerCount, 8u); i++)
        {
            const auto& s = desc.samplers[i];
            BindGroup::GLSamplerBinding binding{};
            binding.sampler = s.sampler->m_glSampler;
            if (!nativeSlotForGL(s.slot, BindingKind::sampler, &binding.slot))
                continue;
            bg->m_glSamplers.push_back(binding);
            bg->m_retainedSamplers.push_back(ref_rcp(s.sampler));
        }
        return bg;
    }

    // VK path.
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
    // numbers equal WGSL `@binding` directly. Validate against layout.
    constexpr uint32_t kMaxWrites = 32;
    VkWriteDescriptorSet writes[kMaxWrites] = {};
    VkDescriptorBufferInfo bufferInfos[kMaxWrites] = {};
    VkDescriptorImageInfo imageInfos[kMaxWrites] = {};
    uint32_t writeIdx = 0;
    uint32_t bufInfoIdx = 0;
    uint32_t imgInfoIdx = 0;

    // See ore_context_vulkan.cpp for the SPIR-V compaction rationale.
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

    if (m_backendType == BackendType::GL)
    {
        RenderPass pass;
        pass.m_context = this;
        pass.populateAttachmentMetadata(desc);

        // Save the host renderer's VAO and FBO so we can restore them in
        // finish().
        GLint prevVAO = 0, prevFBO = 0;
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prevVAO);
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
        pass.m_prevVAO = static_cast<unsigned int>(prevVAO);
        pass.m_prevFBO = static_cast<unsigned int>(prevFBO);

        // Create an FBO for this render pass.
        glGenFramebuffers(1, &pass.m_glFBO);
        pass.m_ownsFBO = true;
        glBindFramebuffer(GL_FRAMEBUFFER, pass.m_glFBO);

        // Attach color targets.
        GLenum drawBuffers[4] = {};
        for (uint32_t i = 0; i < desc.colorCount; ++i)
        {
            const auto& ca = desc.colorAttachments[i];
            if (ca.view)
            {
                Texture* tex = ca.view->texture();
                GLenum attachment = GL_COLOR_ATTACHMENT0 + i;

                if (tex->m_glRenderbuffer != 0)
                {
                    glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                                              attachment,
                                              GL_RENDERBUFFER,
                                              tex->m_glRenderbuffer);
                }
                else if (tex->type() == TextureType::cube)
                {
                    glFramebufferTexture2D(GL_FRAMEBUFFER,
                                           attachment,
                                           GL_TEXTURE_CUBE_MAP_POSITIVE_X +
                                               ca.view->baseLayer(),
                                           tex->m_glTexture,
                                           ca.view->baseMipLevel());
                }
                else if (tex->type() == TextureType::array2D ||
                         tex->type() == TextureType::texture3D)
                {
                    glFramebufferTextureLayer(GL_FRAMEBUFFER,
                                              attachment,
                                              tex->m_glTexture,
                                              ca.view->baseMipLevel(),
                                              ca.view->baseLayer());
                }
                else
                {
                    glFramebufferTexture2D(GL_FRAMEBUFFER,
                                           attachment,
                                           GL_TEXTURE_2D,
                                           tex->m_glTexture,
                                           ca.view->baseMipLevel());
                }

                // Record resolve target for MSAA blit in finish().
                if (ca.resolveTarget)
                {
                    auto& r = pass.m_glResolves[pass.m_glResolveCount++];
                    r.colorIndex = i;
                    r.resolveTex = ca.resolveTarget->texture()->m_glTexture;
                    r.resolveTarget = ca.resolveTarget->texture()->m_glTarget;
                    r.width = tex->width();
                    r.height = tex->height();
                }

                drawBuffers[i] = attachment;
            }
        }

        if (desc.colorCount > 0)
        {
            glDrawBuffers(desc.colorCount, drawBuffers);
        }

        // Attach depth/stencil.
        if (desc.depthStencil.view)
        {
            Texture* depthTex = desc.depthStencil.view->texture();
            TextureFormat depthFmt = depthTex->format();
            bool hasStencil = (depthFmt == TextureFormat::depth24plusStencil8 ||
                               depthFmt == TextureFormat::depth32floatStencil8);
            GLenum depthAttachment =
                hasStencil ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;

            if (depthTex->m_glRenderbuffer != 0)
            {
                glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                                          depthAttachment,
                                          GL_RENDERBUFFER,
                                          depthTex->m_glRenderbuffer);
            }
            else
            {
                glFramebufferTexture2D(GL_FRAMEBUFFER,
                                       depthAttachment,
                                       GL_TEXTURE_2D,
                                       depthTex->m_glTexture,
                                       desc.depthStencil.view->baseMipLevel());
            }
        }

        // Verify completeness (debug only).
        assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) ==
                   GL_FRAMEBUFFER_COMPLETE &&
               "Ore GL FBO incomplete");

        // Handle clear ops.
        for (uint32_t i = 0; i < desc.colorCount; ++i)
        {
            const auto& ca = desc.colorAttachments[i];
            if (ca.loadOp == LoadOp::clear)
            {
                GLfloat clearColor[4] = {ca.clearColor.r,
                                         ca.clearColor.g,
                                         ca.clearColor.b,
                                         ca.clearColor.a};
                glClearBufferfv(GL_COLOR, i, clearColor);
            }
        }

        if (desc.depthStencil.view)
        {
            TextureFormat depthFmt =
                desc.depthStencil.view->texture()->format();
            bool hasStencil = (depthFmt == TextureFormat::depth24plusStencil8 ||
                               depthFmt == TextureFormat::depth32floatStencil8);

            if (desc.depthStencil.depthLoadOp == LoadOp::clear && hasStencil &&
                desc.depthStencil.stencilLoadOp == LoadOp::clear)
            {
                glDepthMask(GL_TRUE);
                glStencilMask(0xFF);
                GLfloat depth = desc.depthStencil.depthClearValue;
                GLint stencil =
                    static_cast<GLint>(desc.depthStencil.stencilClearValue);
                glClearBufferfi(GL_DEPTH_STENCIL, 0, depth, stencil);
            }
            else
            {
                if (desc.depthStencil.depthLoadOp == LoadOp::clear)
                {
                    glDepthMask(GL_TRUE);
                    GLfloat depth = desc.depthStencil.depthClearValue;
                    glClearBufferfv(GL_DEPTH, 0, &depth);
                }
                if (hasStencil &&
                    desc.depthStencil.stencilLoadOp == LoadOp::clear)
                {
                    glStencilMask(0xFF);
                    GLint stencil =
                        static_cast<GLint>(desc.depthStencil.stencilClearValue);
                    glClearBufferiv(GL_STENCIL, 0, &stencil);
                }
            }
        }

        // Create a per-pass VAO so Ore vertex attrib state doesn't
        // contaminate the host renderer's VAO.
        glGenVertexArrays(1, &pass.m_glVAO);
        pass.m_ownsVAO = true;
        glBindVertexArray(pass.m_glVAO);

        // Set a default viewport from the first color or depth attachment.
        uint32_t defaultW = 0, defaultH = 0;
        if (desc.colorCount > 0 && desc.colorAttachments[0].view)
        {
            defaultW = desc.colorAttachments[0].view->texture()->width();
            defaultH = desc.colorAttachments[0].view->texture()->height();
        }
        else if (desc.depthStencil.view)
        {
            defaultW = desc.depthStencil.view->texture()->width();
            defaultH = desc.depthStencil.view->texture()->height();
        }
        if (defaultW > 0 && defaultH > 0)
        {
            glViewport(0, 0, defaultW, defaultH);
            pass.m_viewportWidth = defaultW;
            pass.m_viewportHeight = defaultH;
        }

        return pass;
    }

    // VK path.
    // Flush lazy-init barriers before we enter the render pass — barriers
    // inside a render pass are restricted to declared self-dependencies.
    vkFlushPendingInitialTransitions();

    RenderPass pass;
    pass.m_context = this;
    pass.m_vkContext = this;
    pass.m_vkCmdBuf = m_vkCommandBuffer;
    pass.populateAttachmentMetadata(desc);

    // Build render pass key from the actual load/store ops + formats.
    // See ore_context_vulkan.cpp's mirror — same attachment ordering:
    // [color × N][resolve × R][depth?].
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
    VkClearValue clearValues[9]{};
    for (uint32_t i = 0; i < desc.colorCount; ++i)
    {
        const ClearColor& c = desc.colorAttachments[i].clearColor;
        clearValues[i].color = {{c.r, c.g, c.b, c.a}};
    }
    if (key.hasDepth)
    {
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

    if (m_backendType == BackendType::GL)
    {
        auto* glTarget =
            static_cast<gpu::TextureRenderTargetGL*>(canvas->renderTarget());
        GLuint texID = glTarget->externalTextureID();
        assert(texID != 0);

        TextureDesc texDesc{};
        texDesc.width = canvas->width();
        texDesc.height = canvas->height();
        texDesc.format = TextureFormat::rgba8unorm;
        texDesc.type = TextureType::texture2D;
        texDesc.renderTarget = true;
        texDesc.numMipmaps = 1;
        texDesc.sampleCount = 1;

        auto texture = rcp<Texture>(new Texture(texDesc));
        texture->m_glTexture = texID;
        texture->m_glTarget = GL_TEXTURE_2D;
        texture->m_glOwnsTexture = false;

        TextureViewDesc viewDesc{};
        viewDesc.texture = texture.get();
        viewDesc.dimension = TextureViewDimension::texture2D;
        viewDesc.baseMipLevel = 0;
        viewDesc.mipCount = 1;
        viewDesc.baseLayer = 0;
        viewDesc.layerCount = 1;
        return rcp<TextureView>(new TextureView(std::move(texture), viewDesc));
    }

    // VK path.
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

    if (m_backendType == BackendType::GL)
    {
        GLuint texID = static_cast<GLuint>(
            reinterpret_cast<uintptr_t>(gpuTex->nativeHandle()));
        if (texID == 0)
            return nullptr;

        TextureDesc texDesc{};
        texDesc.width = w;
        texDesc.height = h;
        texDesc.format = TextureFormat::rgba8unorm;
        texDesc.type = TextureType::texture2D;
        texDesc.renderTarget = false;
        texDesc.numMipmaps = 1;
        texDesc.sampleCount = 1;

        auto texture = rcp<Texture>(new Texture(texDesc));
        texture->m_glTexture = texID;
        texture->m_glTarget = GL_TEXTURE_2D;
        texture->m_glOwnsTexture = false;

        TextureViewDesc viewDesc{};
        viewDesc.texture = texture.get();
        viewDesc.dimension = TextureViewDimension::texture2D;
        viewDesc.baseMipLevel = 0;
        viewDesc.mipCount = 1;
        viewDesc.baseLayer = 0;
        viewDesc.layerCount = 1;
        return rcp<TextureView>(new TextureView(std::move(texture), viewDesc));
    }

    // VK path: gpu::Texture is vkutil::Texture2D on Vulkan.
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

} // namespace rive::ore

#endif // ORE_BACKEND_VK && ORE_BACKEND_GL
