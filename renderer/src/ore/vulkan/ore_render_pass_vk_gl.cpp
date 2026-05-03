/*
 * Copyright 2025 Rive
 */

// Combined VK + GL RenderPass implementation for dual-backend builds.
// Compiled when both ORE_BACKEND_VK and ORE_BACKEND_GL are active.
// Provides all RenderPass method definitions with runtime dispatch:
// passes initialized via the Vulkan context path (m_vkContext != nullptr) use
// Vulkan; passes initialized via the GL context path (m_context != nullptr)
// use GL.

#if defined(ORE_BACKEND_VK) && defined(ORE_BACKEND_GL)

// Pull in VK static helpers (layout transitions, etc.).
// The !defined(ORE_BACKEND_GL) guard in ore_render_pass_vulkan.cpp excludes
// RenderPass method bodies, so we get only the static helper functions.
#include "ore_render_pass_vulkan.cpp"

// Pull in GL static helpers (oreTopologyToGL, oreBlendFactorToGL, etc.).
// The !defined(ORE_BACKEND_METAL) && !defined(ORE_BACKEND_VK) guard in
// ore_render_pass_gl.cpp excludes RenderPass method bodies, so we get only
// the static helper functions.
#include "../gl/ore_render_pass_gl.cpp"

#include "rive/renderer/gl/load_gles_extensions.hpp"
#include "rive/renderer/ore/ore_render_pass.hpp"
#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_texture.hpp"
#include "rive/renderer/ore/ore_sampler.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/renderer/ore/ore_context.hpp" // for RenderPass inline bodies

namespace rive::ore
{

RenderPass::~RenderPass()
{
    if (!m_finished)
    {
        if (m_vkContext != nullptr || m_context != nullptr)
            finish();
    }
}

RenderPass::RenderPass(RenderPass&& other) noexcept :
    m_currentPipeline(std::move(other.m_currentPipeline)),
    // VK members
    m_vkContext(other.m_vkContext),
    m_vkCmdBuf(other.m_vkCmdBuf),
    m_vkFramebuffer(other.m_vkFramebuffer),
    m_vkIndexBuffer(other.m_vkIndexBuffer),
    m_vkIndexType(other.m_vkIndexType),
    m_vkIndexOffset(other.m_vkIndexOffset),
    m_vkColorCount(other.m_vkColorCount),
    m_vkDepthImage(other.m_vkDepthImage),
    m_vkDepthBaseLayer(other.m_vkDepthBaseLayer),
    m_vkDepthLayerCount(other.m_vkDepthLayerCount),
    // GL members
    m_glFBO(other.m_glFBO),
    m_glVAO(other.m_glVAO),
    m_prevVAO(other.m_prevVAO),
    m_prevFBO(other.m_prevFBO),
    m_ownsFBO(other.m_ownsFBO),
    m_ownsVAO(other.m_ownsVAO),
    m_viewportWidth(other.m_viewportWidth),
    m_viewportHeight(other.m_viewportHeight),
    m_maxSamplerSlot(other.m_maxSamplerSlot),
    m_maxAttribSlot(other.m_maxAttribSlot),
    m_usedSamplers(other.m_usedSamplers),
    m_usedAttribs(other.m_usedAttribs),
    m_glIndexFormat(other.m_glIndexFormat),
    m_glResolveCount(other.m_glResolveCount)
{
    moveCrossBackendFieldsFrom(other);
    // VK arrays
    memcpy(m_vkColorImages, other.m_vkColorImages, sizeof(m_vkColorImages));
    memcpy(m_vkColorBaseLayer,
           other.m_vkColorBaseLayer,
           sizeof(m_vkColorBaseLayer));
    memcpy(m_vkColorLayerCount,
           other.m_vkColorLayerCount,
           sizeof(m_vkColorLayerCount));
    memcpy(m_vkColorRenderTargets,
           other.m_vkColorRenderTargets,
           sizeof(m_vkColorRenderTargets));
    // GL resolve array
    for (uint32_t i = 0; i < m_glResolveCount; ++i)
        m_glResolves[i] = other.m_glResolves[i];
    other.m_vkContext = nullptr;
    other.m_vkFramebuffer = VK_NULL_HANDLE;
    other.m_ownsFBO = false;
    other.m_ownsVAO = false;
    other.m_glResolveCount = 0;
}

RenderPass& RenderPass::operator=(RenderPass&& other) noexcept
{
    if (this != &other)
    {
        if (!m_finished && (m_vkContext != nullptr || m_context != nullptr))
            finish();
        moveCrossBackendFieldsFrom(other);
        m_currentPipeline = std::move(other.m_currentPipeline);
        // VK members
        m_vkContext = other.m_vkContext;
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
        m_vkDepthImage = other.m_vkDepthImage;
        m_vkDepthBaseLayer = other.m_vkDepthBaseLayer;
        m_vkDepthLayerCount = other.m_vkDepthLayerCount;
        // GL members
        m_glFBO = other.m_glFBO;
        m_glVAO = other.m_glVAO;
        m_prevVAO = other.m_prevVAO;
        m_prevFBO = other.m_prevFBO;
        m_ownsFBO = other.m_ownsFBO;
        m_ownsVAO = other.m_ownsVAO;
        m_viewportWidth = other.m_viewportWidth;
        m_viewportHeight = other.m_viewportHeight;
        m_maxSamplerSlot = other.m_maxSamplerSlot;
        m_maxAttribSlot = other.m_maxAttribSlot;
        m_usedSamplers = other.m_usedSamplers;
        m_usedAttribs = other.m_usedAttribs;
        m_glIndexFormat = other.m_glIndexFormat;
        m_glResolveCount = other.m_glResolveCount;
        for (uint32_t i = 0; i < m_glResolveCount; ++i)
            m_glResolves[i] = other.m_glResolves[i];
        other.m_vkContext = nullptr;
        other.m_vkFramebuffer = VK_NULL_HANDLE;
        other.m_ownsFBO = false;
        other.m_ownsVAO = false;
        other.m_glResolveCount = 0;
    }
    return *this;
}

void RenderPass::validate() const
{
    assert(!m_finished && "RenderPass already finished");
    assert(m_vkContext != nullptr || m_context != nullptr);
}

void RenderPass::setPipeline(Pipeline* pipeline)
{
    validate();
    if (!checkPipelineCompat(pipeline))
        return;
    if (m_vkContext != nullptr)
    {
        m_currentPipeline = ref_rcp(pipeline);
        m_vkContext->m_vk.CmdBindPipeline(m_vkCmdBuf,
                                          VK_PIPELINE_BIND_POINT_GRAPHICS,
                                          pipeline->m_vkPipeline);
        return;
    }

    // GL path.
    m_currentPipeline = ref_rcp(pipeline);
    const auto& desc = pipeline->desc();
    glUseProgram(pipeline->m_glProgram);

    if (desc.cullMode == CullMode::none)
    {
        glDisable(GL_CULL_FACE);
    }
    else
    {
        glEnable(GL_CULL_FACE);
        glCullFace(desc.cullMode == CullMode::front ? GL_FRONT : GL_BACK);
    }

    glFrontFace(desc.winding == FaceWinding::counterClockwise ? GL_CCW : GL_CW);

    if (desc.depthStencil.depthCompare != CompareFunction::always ||
        desc.depthStencil.depthWriteEnabled)
    {
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(oreCompareFunctionToGL(desc.depthStencil.depthCompare));
        glDepthMask(desc.depthStencil.depthWriteEnabled ? GL_TRUE : GL_FALSE);
    }
    else
    {
        glDisable(GL_DEPTH_TEST);
    }

    if (desc.depthStencil.depthBias != 0 ||
        desc.depthStencil.depthBiasSlopeScale != 0.0f)
    {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(desc.depthStencil.depthBiasSlopeScale,
                        static_cast<float>(desc.depthStencil.depthBias));
    }
    else
    {
        glDisable(GL_POLYGON_OFFSET_FILL);
    }

    bool hasStencil = (desc.stencilFront.compare != CompareFunction::always ||
                       desc.stencilFront.failOp != StencilOp::keep ||
                       desc.stencilFront.depthFailOp != StencilOp::keep ||
                       desc.stencilFront.passOp != StencilOp::keep ||
                       desc.stencilBack.compare != CompareFunction::always ||
                       desc.stencilBack.failOp != StencilOp::keep ||
                       desc.stencilBack.depthFailOp != StencilOp::keep ||
                       desc.stencilBack.passOp != StencilOp::keep);

    if (hasStencil)
    {
        glEnable(GL_STENCIL_TEST);
        glStencilMaskSeparate(GL_FRONT, desc.stencilWriteMask);
        glStencilMaskSeparate(GL_BACK, desc.stencilWriteMask);
        glStencilFuncSeparate(GL_FRONT,
                              oreCompareFunctionToGL(desc.stencilFront.compare),
                              static_cast<GLint>(m_glStencilRef),
                              desc.stencilReadMask);
        glStencilFuncSeparate(GL_BACK,
                              oreCompareFunctionToGL(desc.stencilBack.compare),
                              static_cast<GLint>(m_glStencilRef),
                              desc.stencilReadMask);
        glStencilOpSeparate(GL_FRONT,
                            oreStencilOpToGL(desc.stencilFront.failOp),
                            oreStencilOpToGL(desc.stencilFront.depthFailOp),
                            oreStencilOpToGL(desc.stencilFront.passOp));
        glStencilOpSeparate(GL_BACK,
                            oreStencilOpToGL(desc.stencilBack.failOp),
                            oreStencilOpToGL(desc.stencilBack.depthFailOp),
                            oreStencilOpToGL(desc.stencilBack.passOp));
    }
    else
    {
        glDisable(GL_STENCIL_TEST);
    }

    if (desc.colorCount > 0 && desc.colorTargets[0].blendEnabled)
    {
        glEnable(GL_BLEND);
        const auto& b = desc.colorTargets[0].blend;
        glBlendFuncSeparate(oreBlendFactorToGL(b.srcColor),
                            oreBlendFactorToGL(b.dstColor),
                            oreBlendFactorToGL(b.srcAlpha),
                            oreBlendFactorToGL(b.dstAlpha));
        glBlendEquationSeparate(oreBlendOpToGL(b.colorOp),
                                oreBlendOpToGL(b.alphaOp));
    }
    else
    {
        glDisable(GL_BLEND);
    }

    if (desc.colorCount > 0)
    {
        auto mask = desc.colorTargets[0].writeMask;
        glColorMask((mask & ColorWriteMask::red) != ColorWriteMask::none,
                    (mask & ColorWriteMask::green) != ColorWriteMask::none,
                    (mask & ColorWriteMask::blue) != ColorWriteMask::none,
                    (mask & ColorWriteMask::alpha) != ColorWriteMask::none);
    }
}

void RenderPass::setVertexBuffer(uint32_t slot, Buffer* buffer, uint32_t offset)
{
    validate();
    if (m_vkContext != nullptr)
    {
        VkDeviceSize vkOffset = offset;
        m_vkContext->m_vk.CmdBindVertexBuffers(m_vkCmdBuf,
                                               slot,
                                               1,
                                               &buffer->m_vkBuffer,
                                               &vkOffset);
        return;
    }

    // GL path.
    assert(m_currentPipeline != nullptr && "setPipeline must be called first");
    assert(slot < m_currentPipeline->desc().vertexBufferCount);

    const auto& layout = m_currentPipeline->desc().vertexBuffers[slot];
    glBindBuffer(GL_ARRAY_BUFFER, buffer->m_glBuffer);

    for (uint32_t i = 0; i < layout.attributeCount; ++i)
    {
        const auto& attr = layout.attributes[i];
        GLVertexInfo info = oreVertexFormatToGL(attr.format);
        glEnableVertexAttribArray(attr.shaderSlot);

        bool isIntType =
            (info.type == GL_UNSIGNED_INT || info.type == GL_INT ||
             info.type == GL_UNSIGNED_BYTE || info.type == GL_BYTE ||
             info.type == GL_UNSIGNED_SHORT || info.type == GL_SHORT) &&
            !info.normalized;

        if (isIntType)
        {
            glVertexAttribIPointer(
                attr.shaderSlot,
                info.count,
                info.type,
                layout.stride,
                reinterpret_cast<const void*>(
                    static_cast<uintptr_t>(attr.offset + offset)));
        }
        else
        {
            glVertexAttribPointer(
                attr.shaderSlot,
                info.count,
                info.type,
                info.normalized,
                layout.stride,
                reinterpret_cast<const void*>(
                    static_cast<uintptr_t>(attr.offset + offset)));
        }

        if (layout.stepMode == VertexStepMode::instance)
            glVertexAttribDivisor(attr.shaderSlot, 1);
        else
            glVertexAttribDivisor(attr.shaderSlot, 0);

        if (!m_usedAttribs || attr.shaderSlot > m_maxAttribSlot)
            m_maxAttribSlot = attr.shaderSlot;
        m_usedAttribs = true;
    }
}

void RenderPass::setIndexBuffer(Buffer* buffer,
                                IndexFormat format,
                                uint32_t offset)
{
    validate();
    if (m_vkContext != nullptr)
    {
        m_vkIndexBuffer = buffer->m_vkBuffer;
        m_vkIndexType = (format == IndexFormat::uint32) ? VK_INDEX_TYPE_UINT32
                                                        : VK_INDEX_TYPE_UINT16;
        m_vkIndexOffset = offset;
        m_vkContext->m_vk.CmdBindIndexBuffer(m_vkCmdBuf,
                                             buffer->m_vkBuffer,
                                             offset,
                                             m_vkIndexType);
        return;
    }

    // GL path.
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer->m_glBuffer);
    m_glIndexFormat = format;
    (void)offset;
}

void RenderPass::setBindGroup(uint32_t groupIndex,
                              BindGroup* bg,
                              const uint32_t* dynamicOffsets,
                              uint32_t dynamicOffsetCount)
{
    validate();
    m_boundGroups[groupIndex] = ref_rcp(bg);

    if (m_vkContext != nullptr)
    {
        assert(bg != nullptr);
        assert(m_currentPipeline != nullptr &&
               "setPipeline must be called before setBindGroup");

        m_vkContext->m_vk.CmdBindDescriptorSets(
            m_vkCmdBuf,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            m_currentPipeline->m_vkPipelineLayout,
            groupIndex,
            1,
            &bg->m_vkDescriptorSet,
            dynamicOffsetCount,
            dynamicOffsets);
        return;
    }

    // GL path.
    (void)groupIndex;
    uint32_t dynIdx = 0;
    for (auto& b : bg->m_glUBOs)
    {
        uint32_t offset = b.offset;
        if (b.hasDynamicOffset && dynIdx < dynamicOffsetCount)
            offset += dynamicOffsets[dynIdx++];
        glBindBufferRange(GL_UNIFORM_BUFFER, b.slot, b.buffer, offset, b.size);
    }
    for (auto& t : bg->m_glTextures)
    {
        glActiveTexture(GL_TEXTURE0 + t.slot);
        glBindTexture(t.target, t.texture);
        if (!m_usedSamplers || t.slot > m_maxSamplerSlot)
            m_maxSamplerSlot = t.slot;
        m_usedSamplers = true;
    }
    for (auto& s : bg->m_glSamplers)
    {
        glBindSampler(s.slot, s.sampler);
    }
}

void RenderPass::setViewport(float x,
                             float y,
                             float width,
                             float height,
                             float minDepth,
                             float maxDepth)
{
    validate();
    if (m_vkContext != nullptr)
    {
        // Ore NDC convention: Y-up clip space, depth in [0, 1].
        // Vulkan's clip space is Y-down, so we flip the viewport with a
        // negative height (VK_KHR_maintenance1).
        VkViewport vp{};
        vp.x = x;
        vp.y = y + height; // Start at the bottom.
        vp.width = width;
        vp.height = -height; // Negative height flips Y.
        vp.minDepth = minDepth;
        vp.maxDepth = maxDepth;
        m_vkContext->m_vk.CmdSetViewport(m_vkCmdBuf, 0, 1, &vp);

        // Match the other backends' implicit behaviour: scissor defaults
        // to the full viewport rectangle. `VkRect2D` is integer-typed,
        // so floor the corner / ceil the far edge to avoid clipping
        // fractional sub-rects, and clamp negative origins to 0
        // (`static_cast<uint32_t>` of a negative float is UB).
        const float x0 = std::floor(x);
        const float y0 = std::floor(y);
        const float x1 = std::ceil(x + width);
        const float y1 = std::ceil(y + height);
        VkRect2D scissor{};
        scissor.offset = {static_cast<int32_t>(std::max(0.0f, x0)),
                          static_cast<int32_t>(std::max(0.0f, y0))};
        scissor.extent = {static_cast<uint32_t>(std::max(0.0f, x1 - x0)),
                          static_cast<uint32_t>(std::max(0.0f, y1 - y0))};
        m_vkContext->m_vk.CmdSetScissor(m_vkCmdBuf, 0, 1, &scissor);
        return;
    }

    // GL path.
    m_viewportWidth = static_cast<uint32_t>(width);
    m_viewportHeight = static_cast<uint32_t>(height);
    glViewport(static_cast<GLint>(x),
               static_cast<GLint>(y),
               static_cast<GLsizei>(width),
               static_cast<GLsizei>(height));
    glDepthRangef(minDepth, maxDepth);
}

void RenderPass::setScissorRect(uint32_t x,
                                uint32_t y,
                                uint32_t width,
                                uint32_t height)
{
    validate();
    if (m_vkContext != nullptr)
    {
        VkRect2D scissor{};
        scissor.offset = {static_cast<int32_t>(x), static_cast<int32_t>(y)};
        scissor.extent = {width, height};
        m_vkContext->m_vk.CmdSetScissor(m_vkCmdBuf, 0, 1, &scissor);
        return;
    }

    // GL path.
    glEnable(GL_SCISSOR_TEST);
    glScissor(x, y, width, height);
}

void RenderPass::setStencilReference(uint32_t ref)
{
    validate();
    if (m_vkContext != nullptr)
    {
        m_vkContext->m_vk.CmdSetStencilReference(m_vkCmdBuf,
                                                 VK_STENCIL_FACE_FRONT_AND_BACK,
                                                 ref);
        return;
    }

    // GL path. Cache the ref so a subsequent `setPipeline` re-applies
    // it — WebGPU semantics make `setStencilReference` pass-state, so
    // calling it before the first `setPipeline` must not be a no-op.
    m_glStencilRef = ref;
    if (m_currentPipeline)
    {
        const auto& desc = m_currentPipeline->desc();
        glStencilFuncSeparate(GL_FRONT,
                              oreCompareFunctionToGL(desc.stencilFront.compare),
                              static_cast<GLint>(ref),
                              desc.stencilReadMask);
        glStencilFuncSeparate(GL_BACK,
                              oreCompareFunctionToGL(desc.stencilBack.compare),
                              static_cast<GLint>(ref),
                              desc.stencilReadMask);
    }
}

void RenderPass::setBlendColor(float r, float g, float b, float a)
{
    validate();
    if (m_vkContext != nullptr)
    {
        float constants[4] = {r, g, b, a};
        m_vkContext->m_vk.CmdSetBlendConstants(m_vkCmdBuf, constants);
        return;
    }

    // GL path.
    glBlendColor(r, g, b, a);
}

void RenderPass::draw(uint32_t vertexCount,
                      uint32_t instanceCount,
                      uint32_t firstVertex,
                      uint32_t firstInstance)
{
    validate();
    if (m_vkContext != nullptr)
    {
        m_vkContext->m_vk.CmdDraw(m_vkCmdBuf,
                                  vertexCount,
                                  instanceCount,
                                  firstVertex,
                                  firstInstance);
        return;
    }

    // GL path.
    assert(m_currentPipeline != nullptr);
    GLenum mode = oreTopologyToGL(m_currentPipeline->desc().topology);
    (void)firstInstance;
    if (instanceCount > 1)
        glDrawArraysInstanced(mode, firstVertex, vertexCount, instanceCount);
    else
        glDrawArrays(mode, firstVertex, vertexCount);
}

void RenderPass::drawIndexed(uint32_t indexCount,
                             uint32_t instanceCount,
                             uint32_t firstIndex,
                             int32_t baseVertex,
                             uint32_t firstInstance)
{
    validate();
    if (m_vkContext != nullptr)
    {
        m_vkContext->m_vk.CmdDrawIndexed(m_vkCmdBuf,
                                         indexCount,
                                         instanceCount,
                                         firstIndex,
                                         baseVertex,
                                         firstInstance);
        return;
    }

    // GL path.
    assert(m_currentPipeline != nullptr);
    GLenum mode = oreTopologyToGL(m_currentPipeline->desc().topology);
    GLenum indexType = (m_glIndexFormat == IndexFormat::uint32)
                           ? GL_UNSIGNED_INT
                           : GL_UNSIGNED_SHORT;
    uint32_t indexSize =
        (indexType == GL_UNSIGNED_INT) ? sizeof(uint32_t) : sizeof(uint16_t);
    const void* offset = reinterpret_cast<const void*>(
        static_cast<uintptr_t>(firstIndex * indexSize));
    (void)baseVertex;
    (void)firstInstance;
    if (instanceCount > 1)
        glDrawElementsInstanced(mode,
                                indexCount,
                                indexType,
                                offset,
                                instanceCount);
    else
        glDrawElements(mode, indexCount, indexType, offset);
}

void RenderPass::finish()
{
    if (m_finished)
        return;
    m_finished = true;

    // Release bound BindGroup refs.
    for (auto& bg : m_boundGroups)
        bg.reset();

    if (m_vkContext != nullptr)
    {
        m_vkContext->m_vk.CmdEndRenderPass(m_vkCmdBuf);

        // Transition color attachments COLOR_ATTACHMENT_OPTIMAL ->
        // SHADER_READ_ONLY_OPTIMAL so callers (e.g. Rive drawImage) can
        // sample.
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
            m_vkContext->m_vk.CmdPipelineBarrier(
                m_vkCmdBuf,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                0,
                0,
                nullptr,
                0,
                nullptr,
                1,
                &barrier);
            if (m_vkColorRenderTargets[i] != nullptr)
                m_vkColorRenderTargets[i]->updateLastAccess(
                    kColorAttachmentWriteAccess);
        }

        // Transition depth attachment DEPTH_STENCIL_ATTACHMENT_OPTIMAL ->
        // SHADER_READ_ONLY_OPTIMAL.
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
            m_vkContext->m_vk.CmdPipelineBarrier(
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
            // Defer destruction -- the command buffer still references this
            // framebuffer until endFrame() submits and waits.
            m_vkContext->m_vkDeferredFramebuffers.push_back(m_vkFramebuffer);
            m_vkFramebuffer = VK_NULL_HANDLE;
        }
        return;
    }

    // GL path.
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDepthMask(GL_TRUE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

    // Unbind sampler objects and textures so they don't override texture
    // parameters in subsequent draws by the host renderer (e.g. Rive's MSAA
    // path).
    if (m_usedSamplers)
    {
        for (uint32_t i = 0; i <= m_maxSamplerSlot; ++i)
        {
            glActiveTexture(GL_TEXTURE0 + i);
            glBindTexture(GL_TEXTURE_2D, 0);
            glBindTexture(GL_TEXTURE_CUBE_MAP, 0);
            glBindSampler(i, 0);
        }
        glActiveTexture(GL_TEXTURE0);
    }

    if (m_usedAttribs)
    {
        for (uint32_t i = 0; i <= m_maxAttribSlot; ++i)
            glDisableVertexAttribArray(i);
    }

    // Unbind the global array buffer. (GL_ELEMENT_ARRAY_BUFFER is VAO state
    // and will be cleaned up when the per-pass VAO is deleted below — we must
    // NOT unbind it here because on some WebGL2 implementations the EBO=0
    // can leak through the VAO delete/restore and corrupt the host VAO.)
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // Delete the per-pass VAO and restore the host renderer's VAO so its
    // element-array-buffer binding (and other attrib state) is intact.
    if (m_ownsVAO && m_glVAO != 0)
    {
        glDeleteVertexArrays(1, &m_glVAO);
        m_glVAO = 0;
    }
    glBindVertexArray(m_prevVAO);

    // Resolve MSAA renderbuffers → single-sample resolve targets.
    if (m_glResolveCount > 0)
    {
        GLuint resolveFBO;
        glGenFramebuffers(1, &resolveFBO);
        for (uint32_t i = 0; i < m_glResolveCount; ++i)
        {
            const auto& r = m_glResolves[i];
            // Read from the render pass FBO (MSAA renderbuffer attached).
            glBindFramebuffer(GL_READ_FRAMEBUFFER, m_glFBO);
            glReadBuffer(GL_COLOR_ATTACHMENT0 + r.colorIndex);

            // Write to a temp FBO with the single-sample resolve texture.
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, resolveFBO);
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER,
                                   GL_COLOR_ATTACHMENT0,
                                   r.resolveTarget,
                                   r.resolveTex,
                                   0);

            glBlitFramebuffer(0,
                              0,
                              r.width,
                              r.height,
                              0,
                              0,
                              r.width,
                              r.height,
                              GL_COLOR_BUFFER_BIT,
                              GL_NEAREST);
        }
        glDeleteFramebuffers(1, &resolveFBO);
    }

    // Delete our FBO and restore the host renderer's FBO.
    if (m_ownsFBO && m_glFBO != 0)
    {
        glDeleteFramebuffers(1, &m_glFBO);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, m_prevFBO);

    m_context = nullptr;
}

} // namespace rive::ore

#endif // ORE_BACKEND_VK && ORE_BACKEND_GL
