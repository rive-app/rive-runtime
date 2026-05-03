/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/gl/load_gles_extensions.hpp"
#include "rive/renderer/ore/ore_render_pass.hpp"
#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_texture.hpp"
#include "rive/renderer/ore/ore_sampler.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/renderer/ore/ore_context.hpp" // for RenderPass inline bodies
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

// Vertex format → GL type, component count, normalized.
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
        case VertexFormat::uint32x2:
            return {GL_UNSIGNED_INT, 2, GL_FALSE};
        case VertexFormat::uint32x3:
            return {GL_UNSIGNED_INT, 3, GL_FALSE};
        case VertexFormat::uint32x4:
            return {GL_UNSIGNED_INT, 4, GL_FALSE};
        case VertexFormat::sint32:
            return {GL_INT, 1, GL_FALSE};
        case VertexFormat::sint32x2:
            return {GL_INT, 2, GL_FALSE};
        case VertexFormat::sint32x3:
            return {GL_INT, 3, GL_FALSE};
        case VertexFormat::sint32x4:
            return {GL_INT, 4, GL_FALSE};
    }
    RIVE_UNREACHABLE();
}

// ============================================================================
// RenderPass
// ============================================================================

// When both Metal and GL are compiled (macOS), ore_render_pass_metal_gl.mm
// provides all RenderPass method bodies with runtime dispatch. This file
// only contributes the static helper functions in that case.
#if defined(ORE_BACKEND_GL) && !defined(ORE_BACKEND_METAL) &&                  \
    !defined(ORE_BACKEND_VK)

RenderPass::~RenderPass()
{
    if (!m_finished && m_context != nullptr)
    {
        finish();
    }
}

RenderPass::RenderPass(RenderPass&& other) noexcept :
    m_glFBO(other.m_glFBO),
    m_glVAO(other.m_glVAO),
    m_prevVAO(other.m_prevVAO),
    m_prevFBO(other.m_prevFBO),
    m_ownsFBO(other.m_ownsFBO),
    m_ownsVAO(other.m_ownsVAO),
    m_currentPipeline(std::move(other.m_currentPipeline)),
    m_viewportWidth(other.m_viewportWidth),
    m_viewportHeight(other.m_viewportHeight),
    m_maxSamplerSlot(other.m_maxSamplerSlot),
    m_maxAttribSlot(other.m_maxAttribSlot),
    m_usedSamplers(other.m_usedSamplers),
    m_usedAttribs(other.m_usedAttribs),
    m_glResolveCount(other.m_glResolveCount)
{
    moveCrossBackendFieldsFrom(other);
    for (uint32_t i = 0; i < m_glResolveCount; ++i)
        m_glResolves[i] = other.m_glResolves[i];
    other.m_ownsFBO = false;
    other.m_ownsVAO = false;
    other.m_glResolveCount = 0;
}

RenderPass& RenderPass::operator=(RenderPass&& other) noexcept
{
    if (this != &other)
    {
        if (!m_finished && m_context != nullptr)
        {
            finish();
        }
        moveCrossBackendFieldsFrom(other);
        m_glFBO = other.m_glFBO;
        m_glVAO = other.m_glVAO;
        m_prevVAO = other.m_prevVAO;
        m_prevFBO = other.m_prevFBO;
        m_ownsFBO = other.m_ownsFBO;
        m_ownsVAO = other.m_ownsVAO;
        m_currentPipeline = std::move(other.m_currentPipeline);
        m_viewportWidth = other.m_viewportWidth;
        m_viewportHeight = other.m_viewportHeight;
        m_maxSamplerSlot = other.m_maxSamplerSlot;
        m_maxAttribSlot = other.m_maxAttribSlot;
        m_usedSamplers = other.m_usedSamplers;
        m_usedAttribs = other.m_usedAttribs;
        m_glResolveCount = other.m_glResolveCount;
        for (uint32_t i = 0; i < m_glResolveCount; ++i)
            m_glResolves[i] = other.m_glResolves[i];
        other.m_ownsFBO = false;
        other.m_ownsVAO = false;
        other.m_glResolveCount = 0;
    }
    return *this;
}

void RenderPass::validate() const
{
    assert(!m_finished && "RenderPass already finished");
    assert(m_context != nullptr);
}

void RenderPass::setPipeline(Pipeline* pipeline)
{
    validate();
    if (!checkPipelineCompat(pipeline))
        return;
    m_currentPipeline = ref_rcp(pipeline);

    const auto& desc = pipeline->desc();

    glUseProgram(pipeline->m_glProgram);

    // Cull mode.
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
    // baked into every vertex shader at compile time. The same approach
    // is used by wgpu and Dawn's GL backends.
    glFrontFace(desc.winding == FaceWinding::counterClockwise ? GL_CW : GL_CCW);

    // Depth.
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

    // Depth bias (polygon offset).
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

    // Stencil.
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

    // Blending — GLES3 only supports global blend (colorTargets[0]).
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

    // Color write mask (global on GLES3).
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
        {
            glVertexAttribDivisor(attr.shaderSlot, 1);
        }
        else
        {
            glVertexAttribDivisor(attr.shaderSlot, 0);
        }

        // Track for cleanup in finish().
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
    assert(bg != nullptr);

    // Hold a strong reference so the BindGroup stays alive until finish().
    m_boundGroups[groupIndex] = ref_rcp(bg);

    uint32_t dynIdx = 0;

    // Bind UBOs via glBindBufferRange. Dynamic-offset flag is cached
    // on the binding from `Pipeline::isDynamicUBO()` at makeBindGroup
    // time; consume one `dynamicOffsets[]` entry per dynamic UBO.
    for (const auto& ubo : bg->m_glUBOs)
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

    // Bind textures.
    for (const auto& tex : bg->m_glTextures)
    {
        glActiveTexture(GL_TEXTURE0 + tex.slot);
        glBindTexture(tex.target, tex.texture);

        // Track for cleanup in finish().
        if (!m_usedSamplers || tex.slot > m_maxSamplerSlot)
            m_maxSamplerSlot = tex.slot;
        m_usedSamplers = true;
    }

    // Bind samplers.
    for (const auto& samp : bg->m_glSamplers)
    {
        glBindSampler(samp.slot, samp.sampler);

        if (!m_usedSamplers || samp.slot > m_maxSamplerSlot)
            m_maxSamplerSlot = samp.slot;
        m_usedSamplers = true;
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
    m_viewportWidth = static_cast<uint32_t>(width);
    m_viewportHeight = static_cast<uint32_t>(height);
    // Ore NDC convention: Y-up clip space, depth in [0, 1].
    // GLES has Y-up natively, so no viewport flip is needed here (unlike
    // Vulkan). The depth range mismatch (GLES [-1,1] vs ore's [0,1]) and
    // the Y-flip required by the Vulkan-style WGSL coordinate space are
    // both corrected in the compiled vertex shader at WGSL→GLSL compile
    // time — no runtime fixup is needed.
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
    glEnable(GL_SCISSOR_TEST);
    glScissor(x, y, width, height);
}

void RenderPass::setStencilReference(uint32_t ref)
{
    validate();
    // Cache the ref so a subsequent `setPipeline` re-applies it. WebGPU
    // semantics: `setStencilReference` is pass-state and persists across
    // pipeline changes — calling it BEFORE the first `setPipeline` is
    // valid, so we must not depend on `m_currentPipeline` here.
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
    glBlendColor(r, g, b, a);
}

void RenderPass::draw(uint32_t vertexCount,
                      uint32_t instanceCount,
                      uint32_t firstVertex,
                      uint32_t firstInstance)
{
    validate();
    assert(m_currentPipeline != nullptr);
    GLenum mode = oreTopologyToGL(m_currentPipeline->desc().topology);

    (void)firstInstance; // GLES3 doesn't support base instance.

    if (instanceCount > 1)
    {
        glDrawArraysInstanced(mode, firstVertex, vertexCount, instanceCount);
    }
    else
    {
        glDrawArrays(mode, firstVertex, vertexCount);
    }
}

void RenderPass::drawIndexed(uint32_t indexCount,
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

    (void)baseVertex;    // Not supported on base GLES3.
    (void)firstInstance; // Not supported on GLES3.

    if (instanceCount > 1)
    {
        glDrawElementsInstanced(mode,
                                indexCount,
                                indexType,
                                offset,
                                instanceCount);
    }
    else
    {
        glDrawElements(mode, indexCount, indexType, offset);
    }
}

void RenderPass::finish()
{
    if (m_finished)
        return;
    m_finished = true;

    // Restore defaults.
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

    // Disable vertex attrib arrays that were enabled, so they don't
    // contaminate the host renderer's VAO state.
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

#endif // ORE_BACKEND_GL && !ORE_BACKEND_METAL

} // namespace rive::ore
