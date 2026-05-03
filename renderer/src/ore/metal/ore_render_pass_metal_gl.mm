/*
 * Copyright 2025 Rive
 */

// Combined Metal + GL RenderPass implementation for macOS.
// Compiled when both ORE_BACKEND_METAL and ORE_BACKEND_GL are active.
// Provides all RenderPass method definitions with runtime dispatch:
// passes initialized via the Metal context path (m_mtlEncoder != nil) use
// Metal; passes initialized via the GL context path (m_context != nullptr)
// use GL.

#if defined(ORE_BACKEND_METAL) && defined(ORE_BACKEND_GL)

// Pull in Metal static helpers (orePrimitiveTopologyToMTL, etc.).
// The !defined(ORE_BACKEND_GL) guard in ore_render_pass_metal.mm excludes
// RenderPass method bodies, so we get only the static helper functions.
#include "ore_render_pass_metal.mm"

// Pull in GL static helpers (oreTopologyToGL, oreBlendFactorToGL, etc.).
// The !defined(ORE_BACKEND_METAL) guard in ore_render_pass_gl.cpp excludes
// RenderPass method bodies, so we get only the static helper functions.
#include "../gl/ore_render_pass_gl.cpp"

#include "rive/renderer/gl/load_gles_extensions.hpp"
#include "rive/renderer/ore/ore_render_pass.hpp"
#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_texture.hpp"
#include "rive/renderer/ore/ore_sampler.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"

namespace rive::ore
{

RenderPass::~RenderPass()
{
    if (!m_finished)
    {
        if (m_mtlEncoder != nil || m_context != nullptr)
            finish();
    }
}

RenderPass::RenderPass(RenderPass&& other) noexcept :
    // Metal members
    m_mtlEncoder(other.m_mtlEncoder),
    m_mtlCommandBuffer(other.m_mtlCommandBuffer),
    m_mtlIndexBuffer(other.m_mtlIndexBuffer),
    m_mtlIndexType(other.m_mtlIndexType),
    m_mtlIndexBufferOffset(other.m_mtlIndexBufferOffset),
    m_mtlPrimitiveType(other.m_mtlPrimitiveType),
    // GL members
    m_glFBO(other.m_glFBO),
    m_glVAO(other.m_glVAO),
    m_ownsFBO(other.m_ownsFBO),
    m_ownsVAO(other.m_ownsVAO),
    m_currentPipeline(std::move(other.m_currentPipeline)),
    m_viewportWidth(other.m_viewportWidth),
    m_viewportHeight(other.m_viewportHeight),
    m_maxSamplerSlot(other.m_maxSamplerSlot),
    m_maxAttribSlot(other.m_maxAttribSlot),
    m_usedSamplers(other.m_usedSamplers),
    m_usedAttribs(other.m_usedAttribs)
{
    moveCrossBackendFieldsFrom(other);
    other.m_mtlEncoder = nil;
    other.m_mtlCommandBuffer = nil;
    other.m_mtlIndexBuffer = nil;
    other.m_ownsFBO = false;
    other.m_ownsVAO = false;
}

RenderPass& RenderPass::operator=(RenderPass&& other) noexcept
{
    if (this != &other)
    {
        if (!m_finished && (m_mtlEncoder != nil || m_context != nullptr))
            finish();
        moveCrossBackendFieldsFrom(other);
        m_mtlEncoder = other.m_mtlEncoder;
        m_mtlCommandBuffer = other.m_mtlCommandBuffer;
        m_mtlIndexBuffer = other.m_mtlIndexBuffer;
        m_mtlIndexType = other.m_mtlIndexType;
        m_mtlIndexBufferOffset = other.m_mtlIndexBufferOffset;
        m_mtlPrimitiveType = other.m_mtlPrimitiveType;
        m_glFBO = other.m_glFBO;
        m_glVAO = other.m_glVAO;
        m_ownsFBO = other.m_ownsFBO;
        m_ownsVAO = other.m_ownsVAO;
        m_currentPipeline = std::move(other.m_currentPipeline);
        m_viewportWidth = other.m_viewportWidth;
        m_viewportHeight = other.m_viewportHeight;
        m_maxSamplerSlot = other.m_maxSamplerSlot;
        m_maxAttribSlot = other.m_maxAttribSlot;
        m_usedSamplers = other.m_usedSamplers;
        m_usedAttribs = other.m_usedAttribs;
        other.m_mtlEncoder = nil;
        other.m_mtlCommandBuffer = nil;
        other.m_mtlIndexBuffer = nil;
        other.m_ownsFBO = false;
        other.m_ownsVAO = false;
    }
    return *this;
}

void RenderPass::validate() const
{
    assert(!m_finished && "RenderPass already finished");
    assert(m_mtlEncoder != nil || m_context != nullptr);
}

void RenderPass::setPipeline(Pipeline* pipeline)
{
    validate();
    if (!checkPipelineCompat(pipeline))
        return;
    if (m_mtlEncoder != nil)
    {
        mtlSetPipeline(pipeline);
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
                              0,
                              desc.stencilReadMask);
        glStencilFuncSeparate(GL_BACK,
                              oreCompareFunctionToGL(desc.stencilBack.compare),
                              0,
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
    if (m_mtlEncoder != nil)
    {
        mtlSetVertexBuffer(slot, buffer, offset);
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
    if (m_mtlEncoder != nil)
    {
        mtlSetIndexBuffer(buffer, format, offset);
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

    if (m_mtlEncoder != nil)
    {
        mtlSetBindGroup(bg, groupIndex, dynamicOffsets, dynamicOffsetCount);
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

void RenderPass::setViewport(
    float x, float y, float width, float height, float minDepth, float maxDepth)
{
    validate();
    if (m_mtlEncoder != nil)
    {
        mtlSetViewport(x, y, width, height, minDepth, maxDepth);
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
    if (m_mtlEncoder != nil)
    {
        mtlSetScissorRect(x, y, width, height);
        return;
    }

    // GL path.
    glEnable(GL_SCISSOR_TEST);
    glScissor(x, y, width, height);
}

void RenderPass::setStencilReference(uint32_t ref)
{
    validate();
    if (m_mtlEncoder != nil)
    {
        mtlSetStencilRef(ref);
        return;
    }

    // GL path.
    if (m_currentPipeline)
    {
        const auto& desc = m_currentPipeline->desc();
        glStencilFuncSeparate(GL_FRONT,
                              oreCompareFunctionToGL(desc.stencilFront.compare),
                              ref,
                              desc.stencilReadMask);
        glStencilFuncSeparate(GL_BACK,
                              oreCompareFunctionToGL(desc.stencilBack.compare),
                              ref,
                              desc.stencilReadMask);
    }
}

void RenderPass::setBlendColor(float r, float g, float b, float a)
{
    validate();
    if (m_mtlEncoder != nil)
    {
        mtlSetBlendColor(r, g, b, a);
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
    if (m_mtlEncoder != nil)
    {
        mtlDraw(vertexCount, instanceCount, firstVertex, firstInstance);
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
    if (m_mtlEncoder != nil)
    {
        mtlDrawIndexed(
            indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
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
        glDrawElementsInstanced(
            mode, indexCount, indexType, offset, instanceCount);
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

    if (m_mtlEncoder != nil)
    {
        mtlFinish();
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

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    // NOTE: GL_ELEMENT_ARRAY_BUFFER is VAO state — do NOT unbind it here.
    // The per-pass VAO deletion below handles cleanup. Unbinding it could
    // corrupt the host renderer's VAO on some WebGL2 implementations.

    if (m_ownsVAO && m_glVAO != 0)
    {
        glDeleteVertexArrays(1, &m_glVAO);
        m_glVAO = 0;
    }

    if (m_ownsFBO && m_glFBO != 0)
    {
        glDeleteFramebuffers(1, &m_glFBO);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    m_context = nullptr;
}

} // namespace rive::ore

#endif // ORE_BACKEND_METAL && ORE_BACKEND_GL
