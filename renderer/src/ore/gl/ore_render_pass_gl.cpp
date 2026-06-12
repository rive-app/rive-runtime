/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/gl/load_gles_extensions.hpp"
#include "ore_render_pass_gl.hpp"
#include "ore_buffer_gl.hpp"
#include "ore_pipeline_gl.hpp"
#include "ore_bind_group_gl.hpp"
#include "ore_texture_gl.hpp"
#include "rive/renderer/ore/ore_context_gl.hpp"
#include "rive/rive_types.hpp"

namespace rive::ore
{

static GLenum oreTopologyToGL(PrimitiveTopology topo)
{
    switch (topo)
    {
        case PrimitiveTopology::pointList:
            return GL_POINTS;
        case PrimitiveTopology::lineList:
            return GL_LINES;
        case PrimitiveTopology::lineStrip:
            return GL_LINE_STRIP;
        case PrimitiveTopology::triangleList:
            return GL_TRIANGLES;
        case PrimitiveTopology::triangleStrip:
            return GL_TRIANGLE_STRIP;
    }
    RIVE_UNREACHABLE();
}

static GLenum oreBlendFactorToGL(BlendFactor f)
{
    switch (f)
    {
        case BlendFactor::zero:
            return GL_ZERO;
        case BlendFactor::one:
            return GL_ONE;
        case BlendFactor::srcColor:
            return GL_SRC_COLOR;
        case BlendFactor::oneMinusSrcColor:
            return GL_ONE_MINUS_SRC_COLOR;
        case BlendFactor::srcAlpha:
            return GL_SRC_ALPHA;
        case BlendFactor::oneMinusSrcAlpha:
            return GL_ONE_MINUS_SRC_ALPHA;
        case BlendFactor::dstColor:
            return GL_DST_COLOR;
        case BlendFactor::oneMinusDstColor:
            return GL_ONE_MINUS_DST_COLOR;
        case BlendFactor::dstAlpha:
            return GL_DST_ALPHA;
        case BlendFactor::oneMinusDstAlpha:
            return GL_ONE_MINUS_DST_ALPHA;
        case BlendFactor::srcAlphaSaturated:
            return GL_SRC_ALPHA_SATURATE;
        case BlendFactor::blendColor:
            return GL_CONSTANT_COLOR;
        case BlendFactor::oneMinusBlendColor:
            return GL_ONE_MINUS_CONSTANT_COLOR;
    }
    RIVE_UNREACHABLE();
}

static GLenum oreBlendOpToGL(BlendOp op)
{
    switch (op)
    {
        case BlendOp::add:
            return GL_FUNC_ADD;
        case BlendOp::subtract:
            return GL_FUNC_SUBTRACT;
        case BlendOp::reverseSubtract:
            return GL_FUNC_REVERSE_SUBTRACT;
        case BlendOp::min:
            return GL_MIN;
        case BlendOp::max:
            return GL_MAX;
    }
    RIVE_UNREACHABLE();
}

static GLenum oreCompareFunctionToGL(CompareFunction fn)
{
    switch (fn)
    {
        case CompareFunction::none:
        case CompareFunction::never:
            return GL_NEVER;
        case CompareFunction::less:
            return GL_LESS;
        case CompareFunction::equal:
            return GL_EQUAL;
        case CompareFunction::lessEqual:
            return GL_LEQUAL;
        case CompareFunction::greater:
            return GL_GREATER;
        case CompareFunction::notEqual:
            return GL_NOTEQUAL;
        case CompareFunction::greaterEqual:
            return GL_GEQUAL;
        case CompareFunction::always:
            return GL_ALWAYS;
    }
    RIVE_UNREACHABLE();
}

static GLenum oreStencilOpToGL(StencilOp op)
{
    switch (op)
    {
        case StencilOp::keep:
            return GL_KEEP;
        case StencilOp::zero:
            return GL_ZERO;
        case StencilOp::replace:
            return GL_REPLACE;
        case StencilOp::incrementClamp:
            return GL_INCR;
        case StencilOp::decrementClamp:
            return GL_DECR;
        case StencilOp::invert:
            return GL_INVERT;
        case StencilOp::incrementWrap:
            return GL_INCR_WRAP;
        case StencilOp::decrementWrap:
            return GL_DECR_WRAP;
    }
    RIVE_UNREACHABLE();
}

struct GLVertexInfo
{
    GLenum type;
    GLint count;
    GLboolean normalized;
};

static GLVertexInfo oreVertexFormatToGL(VertexFormat fmt)
{
    switch (fmt)
    {
        case VertexFormat::float1:
            return {GL_FLOAT, 1, GL_FALSE};
        case VertexFormat::float2:
            return {GL_FLOAT, 2, GL_FALSE};
        case VertexFormat::float3:
            return {GL_FLOAT, 3, GL_FALSE};
        case VertexFormat::float4:
            return {GL_FLOAT, 4, GL_FALSE};
        case VertexFormat::uint8x4:
            return {GL_UNSIGNED_BYTE, 4, GL_FALSE};
        case VertexFormat::sint8x4:
            return {GL_BYTE, 4, GL_FALSE};
        case VertexFormat::unorm8x4:
            return {GL_UNSIGNED_BYTE, 4, GL_TRUE};
        case VertexFormat::snorm8x4:
            return {GL_BYTE, 4, GL_TRUE};
        case VertexFormat::uint16x2:
            return {GL_UNSIGNED_SHORT, 2, GL_FALSE};
        case VertexFormat::sint16x2:
            return {GL_SHORT, 2, GL_FALSE};
        case VertexFormat::unorm16x2:
            return {GL_UNSIGNED_SHORT, 2, GL_TRUE};
        case VertexFormat::snorm16x2:
            return {GL_SHORT, 2, GL_TRUE};
        case VertexFormat::uint16x4:
            return {GL_UNSIGNED_SHORT, 4, GL_FALSE};
        case VertexFormat::sint16x4:
            return {GL_SHORT, 4, GL_FALSE};
        case VertexFormat::float16x2:
            return {GL_HALF_FLOAT, 2, GL_FALSE};
        case VertexFormat::float16x4:
            return {GL_HALF_FLOAT, 4, GL_FALSE};
        case VertexFormat::uint32:
            return {GL_UNSIGNED_INT, 1, GL_FALSE};
    }
    RIVE_UNREACHABLE();
}

// ============================================================================
// RenderPassGL
// ============================================================================

// When both Metal and GL are compiled (macOS), ore_render_pass_metal_gl.mm
// provides all RenderPassGL method bodies with runtime dispatch. This file
// only contributes the static helper functions in that case.
#if defined(ORE_BACKEND_GL)

RenderPassGL::~RenderPassGL()
{
    if (!m_finished && m_context != nullptr)
    {
        finish();
    }
}

void RenderPassGL::validate() const
{
    assert(!m_finished && "RenderPass already finished");
    assert(m_context != nullptr);
}

void RenderPassGL::setPipeline(Pipeline* pipeline)
{
    validate();
    if (!checkPipelineCompat(pipeline))
        return;
    m_currentPipeline = ref_rcp(pipeline);

    auto* glPipeline = lite_rtti_cast<PipelineGL*>(pipeline);
    assert(glPipeline);
    const auto& desc = pipeline->desc();

    glUseProgram(glPipeline->m_glProgram);

    if (desc.cullMode == CullMode::none)
    {
        glDisable(GL_CULL_FACE);
    }
    else
    {
        glEnable(GL_CULL_FACE);
        glCullFace(desc.cullMode == CullMode::front ? GL_FRONT : GL_BACK);
    }

    // Face winding — inverted to compensate for the WGSL→GLSL Y-flip
    // baked into every vertex shader at compile time.
    glFrontFace(desc.winding == FaceWinding::counterClockwise ? GL_CW : GL_CCW);

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

void RenderPassGL::setVertexBuffer(uint32_t slot,
                                   Buffer* buffer,
                                   uint32_t offset)
{
    validate();
    assert(m_currentPipeline != nullptr && "setPipeline must be called first");
    assert(slot < m_currentPipeline->desc().vertexBufferCount);

    const auto& layout = m_currentPipeline->desc().vertexBuffers[slot];
    auto* glBuffer = lite_rtti_cast<BufferGL*>(buffer);
    assert(glBuffer);

    glBindBuffer(GL_ARRAY_BUFFER, glBuffer->m_glBuffer);

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

void RenderPassGL::setIndexBuffer(Buffer* buffer,
                                  IndexFormat format,
                                  uint32_t offset)
{
    validate();
    auto* glBuffer = lite_rtti_cast<BufferGL*>(buffer);
    assert(glBuffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glBuffer->m_glBuffer);
    m_glIndexFormat = format;
    (void)offset;
}

void RenderPassGL::setBindGroup(uint32_t groupIndex,
                                BindGroup* bg,
                                const uint32_t* dynamicOffsets,
                                uint32_t dynamicOffsetCount)
{
    validate();
    assert(bg != nullptr);

    m_boundGroups[groupIndex] = ref_rcp(bg);

    auto* glBg = lite_rtti_cast<BindGroupGL*>(bg);
    assert(glBg);
    uint32_t dynIdx = 0;

    for (const auto& ubo : glBg->m_glUBOs)
    {
        uint32_t offset = ubo.offset;
        if (ubo.hasDynamicOffset && dynIdx < dynamicOffsetCount)
        {
            offset += dynamicOffsets[dynIdx];
            ++dynIdx;
        }
        glBindBufferRange(GL_UNIFORM_BUFFER,
                          ubo.slot,
                          ubo.buffer,
                          offset,
                          ubo.size);
    }

    for (const auto& tex : glBg->m_glTextures)
    {
        glActiveTexture(GL_TEXTURE0 + tex.slot);
        glBindTexture(tex.target, tex.texture);

        if (!m_usedSamplers || tex.slot > m_maxSamplerSlot)
            m_maxSamplerSlot = tex.slot;
        m_usedSamplers = true;
    }

    for (const auto& samp : glBg->m_glSamplers)
    {
        glBindSampler(samp.slot, samp.sampler);

        if (!m_usedSamplers || samp.slot > m_maxSamplerSlot)
            m_maxSamplerSlot = samp.slot;
        m_usedSamplers = true;
    }
}

void RenderPassGL::setViewport(float x,
                               float y,
                               float width,
                               float height,
                               float minDepth,
                               float maxDepth)
{
    validate();
    m_viewportWidth = static_cast<uint32_t>(width);
    m_viewportHeight = static_cast<uint32_t>(height);
    glViewport(static_cast<GLint>(x),
               static_cast<GLint>(y),
               static_cast<GLsizei>(width),
               static_cast<GLsizei>(height));
    glDepthRangef(minDepth, maxDepth);
}

void RenderPassGL::setScissorRect(uint32_t x,
                                  uint32_t y,
                                  uint32_t width,
                                  uint32_t height)
{
    validate();
    glEnable(GL_SCISSOR_TEST);
    glScissor(x, y, width, height);
}

void RenderPassGL::setStencilReference(uint32_t ref)
{
    validate();
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

void RenderPassGL::setBlendColor(float r, float g, float b, float a)
{
    validate();
    glBlendColor(r, g, b, a);
}

void RenderPassGL::draw(uint32_t vertexCount,
                        uint32_t instanceCount,
                        uint32_t firstVertex,
                        uint32_t firstInstance)
{
    validate();
    assert(m_currentPipeline != nullptr);
    GLenum mode = oreTopologyToGL(m_currentPipeline->desc().topology);

    // drawBaseInstance=false on GL, so the Lua guard rejects firstInstance>0.
    // Wiring needs glDrawArraysInstancedBaseInstance (ANGLE).
    (void)firstInstance;

    if (instanceCount > 1)
        glDrawArraysInstanced(mode, firstVertex, vertexCount, instanceCount);
    else
        glDrawArrays(mode, firstVertex, vertexCount);
}

void RenderPassGL::drawIndexed(uint32_t indexCount,
                               uint32_t instanceCount,
                               uint32_t firstIndex,
                               int32_t baseVertex,
                               uint32_t firstInstance)
{
    validate();
    assert(m_currentPipeline != nullptr);
    GLenum mode = oreTopologyToGL(m_currentPipeline->desc().topology);
    GLenum indexType = (m_glIndexFormat == IndexFormat::uint32)
                           ? GL_UNSIGNED_INT
                           : GL_UNSIGNED_SHORT;
    uint32_t indexSize =
        (indexType == GL_UNSIGNED_INT) ? sizeof(uint32_t) : sizeof(uint16_t);
    const void* offset = reinterpret_cast<const void*>(
        static_cast<uintptr_t>(firstIndex * indexSize));

    // drawBaseInstance=false on GL, so the Lua guard rejects baseVertex!=0 and
    // firstInstance>0. Wiring needs
    // glDrawElementsInstancedBaseVertexBaseInstance (ANGLE).
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

void RenderPassGL::finish()
{
    if (m_finished)
        return;
    m_finished = true;

    m_currentPipeline = nullptr;
    for (auto& bg : m_boundGroups)
        bg.reset();

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

    if (m_ownsVAO && m_glVAO != 0)
    {
        glDeleteVertexArrays(1, &m_glVAO);
        m_glVAO = 0;
    }
    glBindVertexArray(m_prevVAO);

    if (m_glResolveCount > 0)
    {
        GLuint resolveFBO;
        glGenFramebuffers(1, &resolveFBO);
        for (uint32_t i = 0; i < m_glResolveCount; ++i)
        {
            const auto& r = m_glResolves[i];
            glBindFramebuffer(GL_READ_FRAMEBUFFER, m_glFBO);
            glReadBuffer(GL_COLOR_ATTACHMENT0 + r.colorIndex);

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

    if (m_ownsFBO && m_glFBO != 0)
        glDeleteFramebuffers(1, &m_glFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_prevFBO);

    m_context = nullptr;
}

#endif // ORE_BACKEND_GL

} // namespace rive::ore
