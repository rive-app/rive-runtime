/*
 * Copyright 2025 Rive
 */

// Unified resource method implementations for VK+GL dual-backend builds.
// Each resource knows which backend created it by which handle is non-null.
// No dispatch abstraction -- just direct handle checks.

#if defined(ORE_BACKEND_VK) && defined(ORE_BACKEND_GL)

#include "rive/renderer/gl/load_gles_extensions.hpp"
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_texture.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/renderer/ore/ore_sampler.hpp"
#include "rive/renderer/ore/ore_shader_module.hpp"
#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_context.hpp"

#include "ore_vulkan_dsl.hpp" // for kVkMaxGroups

#include <vk_mem_alloc.h>
#include <cassert>
#include <cstring>

namespace rive::ore
{

// ============================================================================
// Buffer
// ============================================================================

void Buffer::update(const void* data, uint32_t size, uint32_t offset)
{
    assert(offset + size <= m_size);

    if (m_glBuffer != 0)
    {
        if (m_usage == BufferUsage::index)
        {
            // WebGL forbids binding a buffer to GL_ELEMENT_ARRAY_BUFFER if it
            // was ever bound to a different target, so index buffers must use
            // their native target. GL_ELEMENT_ARRAY_BUFFER is VAO state, so we
            // save/restore the current binding to avoid clobbering the host
            // renderer's VAO.
            GLint prevEBO;
            glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &prevEBO);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_glBuffer);
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, offset, size, data);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, prevEBO);
        }
        else
        {
            // Non-index buffers use GL_COPY_WRITE_BUFFER to avoid disturbing
            // the host renderer's VAO element-buffer binding.
            glBindBuffer(GL_COPY_WRITE_BUFFER, m_glBuffer);
            glBufferSubData(GL_COPY_WRITE_BUFFER, offset, size, data);
            glBindBuffer(GL_COPY_WRITE_BUFFER, 0);
        }
        return;
    }

    assert(m_vkMappedPtr != nullptr);
    memcpy(static_cast<uint8_t*>(m_vkMappedPtr) + offset, data, size);
}

void Buffer::onRefCntReachedZero() const
{
    if (m_glBuffer != 0)
    {
        GLuint buf = m_glBuffer;
        glDeleteBuffers(1, &buf);
        delete this;
        return;
    }

    VmaAllocator allocator = m_vmaAllocator;
    VkBuffer vkBuf = m_vkBuffer;
    VmaAllocation alloc = m_vmaAllocation;
    Context* ctx = m_vkOreContext;

    auto destroy = [=]() {
        if (vkBuf != VK_NULL_HANDLE)
            vmaDestroyBuffer(allocator, vkBuf, alloc);
    };

    delete this;

    if (ctx != nullptr)
        ctx->vkDeferDestroy(std::move(destroy));
    else
        destroy();
}

// ============================================================================
// Texture -- local helpers
// ============================================================================

// GL format + type for upload (local to this TU).
static GLenum oreFormatToGLFormat(TextureFormat fmt)
{
    switch (fmt)
    {
        case TextureFormat::r8unorm:
        case TextureFormat::r16float:
        case TextureFormat::r32float:
            return GL_RED;
        case TextureFormat::rg8unorm:
        case TextureFormat::rg16float:
        case TextureFormat::rg32float:
            return GL_RG;
        case TextureFormat::rgba8unorm:
        case TextureFormat::rgba8snorm:
        case TextureFormat::bgra8unorm:
        case TextureFormat::rgba16float:
        case TextureFormat::rgba32float:
            return GL_RGBA;
        case TextureFormat::rgb10a2unorm:
            return GL_RGBA;
        case TextureFormat::r11g11b10float:
            return GL_RGB;
        case TextureFormat::depth16unorm:
        case TextureFormat::depth32float:
            return GL_DEPTH_COMPONENT;
        case TextureFormat::depth24plusStencil8:
        case TextureFormat::depth32floatStencil8:
            return GL_DEPTH_STENCIL;
        default:
            RIVE_UNREACHABLE();
    }
}

static GLenum oreFormatToGLType(TextureFormat fmt)
{
    switch (fmt)
    {
        case TextureFormat::r8unorm:
        case TextureFormat::rg8unorm:
        case TextureFormat::rgba8unorm:
        case TextureFormat::bgra8unorm:
            return GL_UNSIGNED_BYTE;
        case TextureFormat::rgba8snorm:
            return GL_BYTE;
        case TextureFormat::rgba16float:
        case TextureFormat::rg16float:
        case TextureFormat::r16float:
            return GL_HALF_FLOAT;
        case TextureFormat::rgba32float:
        case TextureFormat::rg32float:
        case TextureFormat::r32float:
        case TextureFormat::depth32float:
            return GL_FLOAT;
        case TextureFormat::rgb10a2unorm:
            return GL_UNSIGNED_INT_2_10_10_10_REV;
        case TextureFormat::r11g11b10float:
            return GL_UNSIGNED_INT_10F_11F_11F_REV;
        case TextureFormat::depth16unorm:
            return GL_UNSIGNED_SHORT;
        case TextureFormat::depth24plusStencil8:
            return GL_UNSIGNED_INT_24_8;
        case TextureFormat::depth32floatStencil8:
            return GL_FLOAT_32_UNSIGNED_INT_24_8_REV;
        default:
            RIVE_UNREACHABLE();
    }
}

// VK helpers (local to this TU).
static bool isDepthStencilFormat(TextureFormat fmt)
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

static bool hasStencil(TextureFormat fmt)
{
    return fmt == TextureFormat::depth24plusStencil8 ||
           fmt == TextureFormat::depth32floatStencil8;
}

static VkImageAspectFlags aspectMask(TextureFormat fmt)
{
    if (isDepthStencilFormat(fmt))
    {
        VkImageAspectFlags flags = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (hasStencil(fmt))
            flags |= VK_IMAGE_ASPECT_STENCIL_BIT;
        return flags;
    }
    return VK_IMAGE_ASPECT_COLOR_BIT;
}

// Transition an image from one layout to another using a pipeline barrier.
static void transitionLayout(PFN_vkCmdPipelineBarrier pfnCmdPipelineBarrier,
                             VkCommandBuffer cmdBuf,
                             VkImage image,
                             VkImageAspectFlags aspect,
                             VkImageLayout oldLayout,
                             VkImageLayout newLayout,
                             uint32_t mipCount = 1,
                             uint32_t layerCount = 1)
{
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspect;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = mipCount;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = layerCount;

    VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
        newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
             newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL &&
             newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
             newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    }
    else
    {
        // Generic fallback -- conservative but correct.
        barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
        barrier.dstAccessMask =
            VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        dstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    }

    pfnCmdPipelineBarrier(cmdBuf,
                          srcStage,
                          dstStage,
                          0,
                          0,
                          nullptr,
                          0,
                          nullptr,
                          1,
                          &barrier);
}

// ============================================================================
// Texture
// ============================================================================

void Texture::upload(const TextureDataDesc& data)
{
    if (m_glTexture != 0)
    {
        // GL path.
        assert(data.data != nullptr);

        glBindTexture(m_glTarget, m_glTexture);

        GLenum format = oreFormatToGLFormat(m_format);
        GLenum type = oreFormatToGLType(m_format);

        // Honor `bytesPerRow` / `rowsPerImage` for non-tightly-packed
        // source data via `GL_UNPACK_ROW_LENGTH` / `GL_UNPACK_IMAGE_HEIGHT`.
        // Save + restore so we don't trash the host's pixel-store state.
        // Pre-fix the combined VK+GL backend ignored these fields entirely
        // — uploads from a sub-rect of a larger source buffer (or from a
        // layout that pads rows) read garbage bytes between rows. The
        // standalone GL TU (`ore_texture_gl.cpp`) had this from the
        // Phase 3 GL fix, but this combined-backend TU was never
        // updated and silently shipped the broken behaviour on every
        // build that compiles both VK and GL (e.g. Android).
        GLint savedRowLength = 0, savedImageHeight = 0;
        glGetIntegerv(GL_UNPACK_ROW_LENGTH, &savedRowLength);
        glGetIntegerv(GL_UNPACK_IMAGE_HEIGHT, &savedImageHeight);
        const uint32_t bpt = textureFormatBytesPerTexel(m_format);
        if (data.bytesPerRow != 0 && bpt != 0 && (data.bytesPerRow % bpt) == 0)
        {
            glPixelStorei(GL_UNPACK_ROW_LENGTH,
                          static_cast<GLint>(data.bytesPerRow / bpt));
        }
        if (data.rowsPerImage != 0)
        {
            glPixelStorei(GL_UNPACK_IMAGE_HEIGHT,
                          static_cast<GLint>(data.rowsPerImage));
        }

        if (m_glTarget == GL_TEXTURE_3D || m_glTarget == GL_TEXTURE_2D_ARRAY)
        {
            glTexSubImage3D(m_glTarget,
                            data.mipLevel,
                            data.x,
                            data.y,
                            data.layer,
                            data.width,
                            data.height,
                            data.depth,
                            format,
                            type,
                            data.data);
        }
        else if (m_glTarget == GL_TEXTURE_CUBE_MAP)
        {
            glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + data.layer,
                            data.mipLevel,
                            data.x,
                            data.y,
                            data.width,
                            data.height,
                            format,
                            type,
                            data.data);
        }
        else
        {
            glTexSubImage2D(m_glTarget,
                            data.mipLevel,
                            data.x,
                            data.y,
                            data.width,
                            data.height,
                            format,
                            type,
                            data.data);
        }

        glPixelStorei(GL_UNPACK_ROW_LENGTH, savedRowLength);
        glPixelStorei(GL_UNPACK_IMAGE_HEIGHT, savedImageHeight);

        glBindTexture(m_glTarget, 0);
        return;
    }

    // VK staging upload path — resolve the current frame CB and VK
    // dispatch table from the owning Context (owned or host-external).
    assert(m_vkOreContext != nullptr);
    VkCommandBuffer cmdBuf = m_vkOreContext->m_vkCommandBuffer;
    auto pfnCmdPipelineBarrier = m_vkOreContext->m_vk.CmdPipelineBarrier;
    auto pfnCmdCopyBufferToImage = m_vkOreContext->m_vk.CmdCopyBufferToImage;
    assert(cmdBuf != VK_NULL_HANDLE);
    assert(m_vkImage != VK_NULL_HANDLE);

    // Determine byte size of the upload region.
    uint32_t bytesPerRow = data.bytesPerRow;
    uint32_t height = data.height > 0 ? data.height : m_height;
    uint32_t depth = data.depth > 0 ? data.depth : m_depthOrArrayLayers;
    VkDeviceSize uploadSize =
        static_cast<VkDeviceSize>(bytesPerRow) * height * depth;

    // Create a transient host-visible staging buffer.
    VkBufferCreateInfo bufCI{};
    bufCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufCI.size = uploadSize;
    bufCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo allocCI{};
    allocCI.usage = VMA_MEMORY_USAGE_CPU_ONLY;
    allocCI.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocationInfo allocInfo{};
    VkBuffer stagingBuf = VK_NULL_HANDLE;
    VmaAllocation stagingAlloc = VK_NULL_HANDLE;
    vmaCreateBuffer(m_vmaAllocator,
                    &bufCI,
                    &allocCI,
                    &stagingBuf,
                    &stagingAlloc,
                    &allocInfo);

    memcpy(allocInfo.pMappedData, data.data, static_cast<size_t>(uploadSize));

    // Transition to TRANSFER_DST.
    transitionLayout(pfnCmdPipelineBarrier,
                     cmdBuf,
                     m_vkImage,
                     aspectMask(m_format),
                     m_vkLayout,
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                     m_numMipmaps,
                     m_depthOrArrayLayers);
    m_vkLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    // Copy staging buffer -> image. `bufferRowLength` and
    // `bufferImageHeight` are in texels — 0 means "tightly packed at
    // imageExtent". When the caller's source has padding (e.g. a
    // sub-rect of a larger image, or `bytesPerRow > width *
    // bytesPerTexel`), the staging copy must honour that pitch or the
    // image comes back with the padding bytes interleaved into every
    // other row. Pre-fix this was hardcoded to 0 — the
    // `ore_array_upload` GM (Phase 4 witness) caught it on every
    // Android Vulkan target by uploading with a 2× row stride filled
    // with `0xCC` sentinel bytes that then leaked into the rendered
    // texture as alternating colour/grey stripes.
    const uint32_t bptVK = textureFormatBytesPerTexel(m_format);
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength =
        (data.bytesPerRow != 0 && bptVK != 0 && (data.bytesPerRow % bptVK) == 0)
            ? data.bytesPerRow / bptVK
            : 0;
    region.bufferImageHeight = data.rowsPerImage;
    region.imageSubresource.aspectMask = aspectMask(m_format);
    region.imageSubresource.mipLevel = data.mipLevel;
    region.imageSubresource.baseArrayLayer = data.layer;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {static_cast<int32_t>(data.x),
                          static_cast<int32_t>(data.y),
                          static_cast<int32_t>(data.z)};
    region.imageExtent = {data.width > 0 ? data.width : m_width, height, depth};

    pfnCmdCopyBufferToImage(cmdBuf,
                            stagingBuf,
                            m_vkImage,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            1,
                            &region);

    // Transition back to SHADER_READ_ONLY.
    transitionLayout(pfnCmdPipelineBarrier,
                     cmdBuf,
                     m_vkImage,
                     aspectMask(m_format),
                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                     m_numMipmaps,
                     m_depthOrArrayLayers);
    m_vkLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    // Defer staging buffer destruction to Context::endFrame(), which
    // submits the command buffer and waits on the fence before cleaning up.
    m_vkOreContext->m_vkDeferredStagingBuffers.push_back(
        {stagingBuf, stagingAlloc});
}

void Texture::onRefCntReachedZero() const
{
    if (m_glTexture != 0 || m_glRenderbuffer != 0)
    {
        // GL path.
        if (m_glRenderbuffer != 0 && m_glOwnsTexture)
        {
            GLuint rb = m_glRenderbuffer;
            glDeleteRenderbuffers(1, &rb);
        }
        if (m_glTexture != 0 && m_glOwnsTexture)
        {
            GLuint tex = m_glTexture;
            glDeleteTextures(1, &tex);
        }
        delete this;
        return;
    }

    // VK path — only destroy VMA-owned images (borrowed textures have
    // m_vmaAllocation==null).
    VmaAllocator allocator = m_vmaAllocator;
    VkImage image = m_vkImage;
    VmaAllocation alloc = m_vmaAllocation;
    Context* ctx = m_vkOreContext;

    auto destroy = [=]() {
        if (image != VK_NULL_HANDLE && alloc != VK_NULL_HANDLE)
            vmaDestroyImage(allocator, image, alloc);
    };

    delete this;

    if (ctx != nullptr)
        ctx->vkDeferDestroy(std::move(destroy));
    else
        destroy();
}

// ============================================================================
// TextureView
// ============================================================================

void TextureView::onRefCntReachedZero() const
{
    if (m_glTextureView != 0)
    {
        GLuint tex = m_glTextureView;
        glDeleteTextures(1, &tex);
        delete this;
        return;
    }

    VkDevice dev = m_vkDevice;
    VkImageView view = m_vkImageView;
    auto destroyFn = m_vkDestroyImageView;
    Context* ctx = m_vkOreContext;

    auto destroy = [=]() {
        if (view != VK_NULL_HANDLE && destroyFn != nullptr)
            destroyFn(dev, view, nullptr);
    };

    delete this;

    if (ctx != nullptr)
        ctx->vkDeferDestroy(std::move(destroy));
    else
        destroy();
}

// ============================================================================
// Pipeline
// ============================================================================

void Pipeline::onRefCntReachedZero() const
{
    if (m_glProgram != 0)
    {
        glDeleteProgram(m_glProgram);
        delete this;
        return;
    }

    // VK path: per-group VkDescriptorSetLayouts are owned by BindGroupLayout
    // (m_layouts[]) — Pipeline only destroys its own VkPipeline + Layout.
    VkDevice dev = m_vkDevice;
    VkPipeline pipe = m_vkPipeline;
    VkPipelineLayout layout = m_vkPipelineLayout;
    auto destroyPipe = m_vkDestroyPipeline;
    auto destroyLayout = m_vkDestroyPipelineLayout;
    Context* ctx = m_vkOreContext;

    auto destroy = [=]() {
        if (pipe != VK_NULL_HANDLE && destroyPipe != nullptr)
            destroyPipe(dev, pipe, nullptr);
        if (layout != VK_NULL_HANDLE && destroyLayout != nullptr)
            destroyLayout(dev, layout, nullptr);
    };

    delete this;

    if (ctx != nullptr)
        ctx->vkDeferDestroy(std::move(destroy));
    else
        destroy();
}

// ============================================================================
// BindGroupLayout
// ============================================================================

void BindGroupLayout::onRefCntReachedZero() const
{
    VkDevice dev = m_vkDevice;
    VkDescriptorSetLayout dsl = m_vkDSL;
    auto destroyDSL = m_vkDestroyDescriptorSetLayout;
    Context* ctx = m_context;

    auto destroy = [=]() {
        if (dsl != VK_NULL_HANDLE && destroyDSL != nullptr)
            destroyDSL(dev, dsl, nullptr);
    };

    delete this;

    if (ctx != nullptr)
        ctx->vkDeferDestroy(std::move(destroy));
    else
        destroy();
}

// ============================================================================
// Sampler
// ============================================================================

void Sampler::onRefCntReachedZero() const
{
    if (m_glSampler != 0)
    {
        GLuint samp = m_glSampler;
        glDeleteSamplers(1, &samp);
        delete this;
        return;
    }

    VkDevice dev = m_vkDevice;
    VkSampler samp = m_vkSampler;
    auto destroyFn = m_vkDestroySampler;
    Context* ctx = m_vkOreContext;

    auto destroy = [=]() {
        if (samp != VK_NULL_HANDLE && destroyFn != nullptr)
            destroyFn(dev, samp, nullptr);
    };

    delete this;

    if (ctx != nullptr)
        ctx->vkDeferDestroy(std::move(destroy));
    else
        destroy();
}

// ============================================================================
// ShaderModule
// ============================================================================

void ShaderModule::onRefCntReachedZero() const
{
    if (m_glShader != 0)
    {
        glDeleteShader(m_glShader);
    }
    else if (m_vkShaderModule != VK_NULL_HANDLE &&
             m_vkDestroyShaderModule != nullptr)
    {
        m_vkDestroyShaderModule(m_vkDevice, m_vkShaderModule, nullptr);
    }
    delete this;
}

// ============================================================================
// BindGroup
// ============================================================================

void BindGroup::onRefCntReachedZero() const
{
    // Deferred destruction is handled at the Lua GC level via
    // Context::deferBindGroupDestroy(), which keeps the rcp<> alive
    // until after endFrame(). By the time refcount reaches zero here,
    // the GPU is guaranteed to be done.
    delete this;
}

} // namespace rive::ore

#endif // ORE_BACKEND_VK && ORE_BACKEND_GL
