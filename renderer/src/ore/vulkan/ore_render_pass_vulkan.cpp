/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/ore/ore_context_vulkan.hpp"
#include "ore_render_pass_vulkan.hpp"
#include "ore_bind_group_vulkan.hpp"
#include "ore_buffer_vulkan.hpp"
#include "ore_texture_vulkan.hpp"
#include "ore_sampler_vulkan.hpp"
#include "ore_pipeline_vulkan.hpp"
#include "rive/renderer/vulkan/render_target_vulkan.hpp"
#include "rive/rive_types.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>

namespace rive::ore
{

// ============================================================================
// RenderPass methods
// ============================================================================

void RenderPassVulkan::setPipeline(Pipeline* inPipeline)
{
    if (!checkPipelineCompat(inPipeline))
        return;
    auto pipeline = lite_rtti_cast<PipelineVulkan*>(inPipeline);
    assert(pipeline != nullptr);
    m_currentPipeline = ref_rcp(pipeline);
    m_vkContext->m_vk->CmdBindPipeline(m_vkCmdBuf,
                                       VK_PIPELINE_BIND_POINT_GRAPHICS,
                                       pipeline->m_vkPipeline);
}

void RenderPassVulkan::setVertexBuffer(uint32_t slot,
                                       Buffer* inBuffer,
                                       uint32_t offset)
{
    VkDeviceSize vkOffset = offset;
    auto buffer = lite_rtti_cast<BufferVulkan*>(inBuffer);
    assert(buffer != nullptr);
    m_vkContext->m_vk->CmdBindVertexBuffers(m_vkCmdBuf,
                                            slot,
                                            1,
                                            &buffer->m_vkBuffer,
                                            &vkOffset);
}

void RenderPassVulkan::setIndexBuffer(Buffer* inBuffer,
                                      IndexFormat format,
                                      uint32_t offset)
{
    auto buffer = lite_rtti_cast<BufferVulkan*>(inBuffer);
    assert(buffer != nullptr);
    m_vkIndexBuffer = buffer->m_vkBuffer;
    m_vkIndexType = (format == IndexFormat::uint32) ? VK_INDEX_TYPE_UINT32
                                                    : VK_INDEX_TYPE_UINT16;
    m_vkIndexOffset = offset;
    m_vkContext->m_vk->CmdBindIndexBuffer(m_vkCmdBuf,
                                          buffer->m_vkBuffer,
                                          offset,
                                          m_vkIndexType);
}

void RenderPassVulkan::setBindGroup(uint32_t groupIndex,
                                    BindGroup* inBg,
                                    const uint32_t* dynamicOffsets,
                                    uint32_t dynamicOffsetCount)
{
    assert(inBg != nullptr);
    assert(m_currentPipeline != nullptr &&
           "setPipeline must be called before setBindGroup");

    auto bg = lite_rtti_cast<BindGroupVulkan*>(inBg);
    assert(bg != nullptr);
    m_vkContext->m_vk->CmdBindDescriptorSets(
        m_vkCmdBuf,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        m_currentPipeline->m_vkPipelineLayout,
        groupIndex,
        1,
        &bg->m_vkDescriptorSet,
        dynamicOffsetCount,
        dynamicOffsets);

    // Hold a strong ref so the BindGroup (and its retained resources) stay
    // alive until the render pass is finished.
    m_boundGroups[groupIndex] = ref_rcp(bg);
}

void RenderPassVulkan::setViewport(float x,
                                   float y,
                                   float width,
                                   float height,
                                   float minDepth,
                                   float maxDepth)
{
    // Ore NDC convention: Y-up clip space, depth in [0, 1].
    // Vulkan's clip space is Y-down, so we flip the viewport with a negative
    // height (VK_KHR_maintenance1). This is handled here rather than in the
    // shader so that WGSL authors see a consistent Y-up coordinate system
    // across all backends without any shader-level fixup.
    VkViewport vp{};
    vp.x = x;
    vp.y = y + height; // Start at the bottom.
    vp.width = width;
    vp.height = -height; // Negative height flips Y.
    vp.minDepth = minDepth;
    vp.maxDepth = maxDepth;
    m_vkContext->m_vk->CmdSetViewport(m_vkCmdBuf, 0, 1, &vp);

    // Match the other backends' implicit behaviour: scissor defaults to the
    // full viewport rectangle. Callers can override with setScissorRect().
    // Vulkan's `VkRect2D` is integer-typed so we have to discretize the
    // float-valued viewport. Use floor on the corner and ceil on the
    // far edge so the scissor never clips the requested area, and clamp
    // negative origins to 0 — `static_cast<uint32_t>` on a negative
    // float is UB, and the previous `int32_t` cast truncated fractional
    // pixels (e.g. y=0.5 → 0 vs height=255.5 → 255 dropped a pixel row
    // on every fractional sub-rect).
    const float x0 = std::floor(x);
    const float y0 = std::floor(y);
    const float x1 = std::ceil(x + width);
    const float y1 = std::ceil(y + height);
    VkRect2D scissor{};
    scissor.offset = {static_cast<int32_t>(std::max(0.0f, x0)),
                      static_cast<int32_t>(std::max(0.0f, y0))};
    scissor.extent = {static_cast<uint32_t>(std::max(0.0f, x1 - x0)),
                      static_cast<uint32_t>(std::max(0.0f, y1 - y0))};
    m_vkContext->m_vk->CmdSetScissor(m_vkCmdBuf, 0, 1, &scissor);
}

void RenderPassVulkan::setScissorRect(uint32_t x,
                                      uint32_t y,
                                      uint32_t width,
                                      uint32_t height)
{
    VkRect2D scissor{};
    scissor.offset = {static_cast<int32_t>(x), static_cast<int32_t>(y)};
    scissor.extent = {width, height};
    m_vkContext->m_vk->CmdSetScissor(m_vkCmdBuf, 0, 1, &scissor);
}

void RenderPassVulkan::setStencilReference(uint32_t ref)
{
    m_vkContext->m_vk->CmdSetStencilReference(m_vkCmdBuf,
                                              VK_STENCIL_FACE_FRONT_AND_BACK,
                                              ref);
}

void RenderPassVulkan::setBlendColor(float r, float g, float b, float a)
{
    float constants[4] = {r, g, b, a};
    m_vkContext->m_vk->CmdSetBlendConstants(m_vkCmdBuf, constants);
}

void RenderPassVulkan::draw(uint32_t vertexCount,
                            uint32_t instanceCount,
                            uint32_t firstVertex,
                            uint32_t firstInstance)
{
    m_vkContext->m_vk->CmdDraw(m_vkCmdBuf,
                               vertexCount,
                               instanceCount,
                               firstVertex,
                               firstInstance);
}

void RenderPassVulkan::drawIndexed(uint32_t indexCount,
                                   uint32_t instanceCount,
                                   uint32_t firstIndex,
                                   int32_t baseVertex,
                                   uint32_t firstInstance)
{
    m_vkContext->m_vk->CmdDrawIndexed(m_vkCmdBuf,
                                      indexCount,
                                      instanceCount,
                                      firstIndex,
                                      baseVertex,
                                      firstInstance);
}

void RenderPassVulkan::finish()
{
    if (m_finished)
        return;
    m_finished = true;

    m_vkContext->m_vk->CmdEndRenderPass(m_vkCmdBuf);

    // Transition color attachments COLOR_ATTACHMENT_OPTIMAL →
    // SHADER_READ_ONLY_OPTIMAL so callers (e.g. Rive drawImage) can sample.
    //
    // We tell Rive that the last access was a COLOR_ATTACHMENT_WRITE (not a
    // shader read), so that Rive's prepareForFragmentShaderRead() emits a
    // proper barrier in its own command buffer.  A barrier in Rive's CB with
    // srcStageMask=COLOR_ATTACHMENT_OUTPUT covers the writes from this earlier
    // same-queue submission (Vulkan 7.6.2), ensuring cross-submission memory
    // visibility.  Without this, Rive would skip its barrier (thinking the
    // image is already in READ state) and the fragment shader could read stale
    // cache data.
    constexpr gpu::vkutil::ImageAccess kColorAttachmentWriteAccess = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };
    for (uint32_t i = 0; i < m_vkColorCount; ++i)
    {
        if (m_vkColorImages[i] == VK_NULL_HANDLE)
            continue;
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = m_vkColorImages[i];
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT,
                                    0,
                                    1,
                                    m_vkColorBaseLayer[i],
                                    m_vkColorLayerCount[i]};
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = 0; // visibility established in Rive's CB
        m_vkContext->m_vk->CmdPipelineBarrier(
            m_vkCmdBuf,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, // just the layout transition
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &barrier);
        // Tell Rive the image was last written as a color attachment and is now
        // in SHADER_READ_ONLY_OPTIMAL.  Rive's prepareForFragmentShaderRead()
        // will emit the availability→visibility barrier in its own CB.
        if (m_vkColorRenderTargets[i] != nullptr)
            m_vkColorRenderTargets[i]->updateLastAccess(
                kColorAttachmentWriteAccess);
    }

    // Transition depth attachment DEPTH_STENCIL_ATTACHMENT_OPTIMAL →
    // SHADER_READ_ONLY_OPTIMAL so subsequent operations don't see stale
    // depth writes.  Without this barrier NVIDIA drivers report
    // ERROR_DEVICE_LOST during pixel readback.
    if (m_vkDepthImage != VK_NULL_HANDLE)
    {
        // Aspect mask must cover stencil when format contains one, else
        // strict drivers / validation layer fault on a missing-aspect
        // transition for depth+stencil-combined formats.
        VkImageAspectFlags depthAspect = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (m_depthFormat == TextureFormat::depth24plusStencil8 ||
            m_depthFormat == TextureFormat::depth32floatStencil8)
        {
            depthAspect |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }

        VkImageMemoryBarrier depthBarrier{};
        depthBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        depthBarrier.oldLayout =
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        depthBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        depthBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        depthBarrier.image = m_vkDepthImage;
        depthBarrier.subresourceRange = {depthAspect,
                                         0,
                                         1,
                                         m_vkDepthBaseLayer,
                                         m_vkDepthLayerCount};
        depthBarrier.srcAccessMask =
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        depthBarrier.dstAccessMask = 0;
        m_vkContext->m_vk->CmdPipelineBarrier(
            m_vkCmdBuf,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0,
            0,
            nullptr,
            0,
            nullptr,
            1,
            &depthBarrier);
    }

    if (m_vkFramebuffer != VK_NULL_HANDLE)
    {
        // Defer destruction — the command buffer still references this
        // framebuffer until endFrame() submits and waits.
        m_vkContext->m_vkDeferredFramebuffers.push_back(m_vkFramebuffer);
        m_vkFramebuffer = VK_NULL_HANDLE;
    }
}

RenderPassVulkan::~RenderPassVulkan() { finish(); }

RenderPassVulkan::RenderPassVulkan(RenderPassVulkan&& other) noexcept
{
#if defined(ORE_BACKEND_VK)
    m_vkContext = other.m_vkContext;
    m_currentPipeline = std::move(other.m_currentPipeline);
    m_vkCmdBuf = other.m_vkCmdBuf;
    m_vkFramebuffer = other.m_vkFramebuffer;
    m_vkIndexBuffer = other.m_vkIndexBuffer;
    m_vkIndexType = other.m_vkIndexType;
    m_vkIndexOffset = other.m_vkIndexOffset;
    memcpy(m_vkColorImages, other.m_vkColorImages, sizeof(m_vkColorImages));
    memcpy(m_vkColorBaseLayer,
           other.m_vkColorBaseLayer,
           sizeof(m_vkColorBaseLayer));
    memcpy(m_vkColorLayerCount,
           other.m_vkColorLayerCount,
           sizeof(m_vkColorLayerCount));
    m_vkColorCount = other.m_vkColorCount;
    memcpy(m_vkColorRenderTargets,
           other.m_vkColorRenderTargets,
           sizeof(m_vkColorRenderTargets));
    other.m_vkFramebuffer = VK_NULL_HANDLE;
#endif
}

RenderPassVulkan& RenderPassVulkan::operator=(RenderPassVulkan&& other) noexcept
{
    if (this != &other)
    {
        finish();
        new (this) RenderPassVulkan(std::move(other));
    }
    return *this;
}

void RenderPassVulkan::validate() const
{
    assert(!m_finished && "RenderPass has already been finished");
}

} // namespace rive::ore
