/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/ore/ore_render_pass.hpp"
#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_texture.hpp"
#include "rive/renderer/ore/ore_sampler.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/renderer/ore/ore_context.hpp" // for RenderPass inline bodies
#include "rive/rive_types.hpp"

#import <Metal/Metal.h>

namespace rive::ore
{

static MTLPrimitiveType orePrimitiveTopologyToMTL(PrimitiveTopology topo)
{
    switch (topo)
    {
        case PrimitiveTopology::pointList:
            return MTLPrimitiveTypePoint;
        case PrimitiveTopology::lineList:
            return MTLPrimitiveTypeLine;
        case PrimitiveTopology::lineStrip:
            return MTLPrimitiveTypeLineStrip;
        case PrimitiveTopology::triangleList:
            return MTLPrimitiveTypeTriangle;
        case PrimitiveTopology::triangleStrip:
            return MTLPrimitiveTypeTriangleStrip;
    }
    RIVE_UNREACHABLE();
}

static MTLIndexType oreIndexFormatToMTL(IndexFormat format)
{
    switch (format)
    {
        case IndexFormat::uint16:
            return MTLIndexTypeUInt16;
        case IndexFormat::uint32:
            return MTLIndexTypeUInt32;
        case IndexFormat::none:
            break;
    }
    RIVE_UNREACHABLE();
}

static MTLCullMode oreCullModeToMTL(CullMode mode)
{
    switch (mode)
    {
        case CullMode::none:
            return MTLCullModeNone;
        case CullMode::front:
            return MTLCullModeFront;
        case CullMode::back:
            return MTLCullModeBack;
    }
    RIVE_UNREACHABLE();
}

static MTLWinding oreWindingToMTL(FaceWinding winding)
{
    switch (winding)
    {
        case FaceWinding::clockwise:
            return MTLWindingClockwise;
        case FaceWinding::counterClockwise:
            return MTLWindingCounterClockwise;
    }
    RIVE_UNREACHABLE();
}

// Must match kMetalVertexBufferBase in ore_context_metal.mm — vertex buffers
// are bound at slots [kMetalVertexBufferBase, ...) to avoid colliding with
// uniform buffers mapped to the low buffer indices ([[buffer(0)]] etc).
static constexpr uint32_t kMetalVertexBufferBase = 16;

// ============================================================================
// Metal implementation helpers (inline)
// Shared between metal-only and metal+gl builds.
// ============================================================================

inline void RenderPass::mtlSetPipeline(Pipeline* pipeline)
{
    [m_mtlEncoder setRenderPipelineState:pipeline->m_mtlPipeline];
    [m_mtlEncoder setDepthStencilState:pipeline->m_mtlDepthStencil];

    const auto& desc = pipeline->desc();
    [m_mtlEncoder setCullMode:oreCullModeToMTL(desc.cullMode)];
    [m_mtlEncoder setFrontFacingWinding:oreWindingToMTL(desc.winding)];
    m_mtlPrimitiveType = orePrimitiveTopologyToMTL(desc.topology);

    if (desc.depthStencil.depthBias != 0 ||
        desc.depthStencil.depthBiasSlopeScale != 0.0f)
    {
        [m_mtlEncoder setDepthBias:(float)desc.depthStencil.depthBias
                        slopeScale:desc.depthStencil.depthBiasSlopeScale
                             clamp:desc.depthStencil.depthBiasClamp];
    }
}

inline void RenderPass::mtlSetVertexBuffer(uint32_t slot,
                                           Buffer* buffer,
                                           uint32_t offset)
{
    [m_mtlEncoder setVertexBuffer:buffer->m_mtlBuffer
                           offset:offset
                          atIndex:slot + kMetalVertexBufferBase];
}

inline void RenderPass::mtlSetIndexBuffer(Buffer* buffer,
                                          IndexFormat format,
                                          uint32_t offset)
{
    m_mtlIndexBuffer = buffer->m_mtlBuffer;
    m_mtlIndexType = oreIndexFormatToMTL(format);
    m_mtlIndexBufferOffset = offset;
}

inline void RenderPass::mtlSetBindGroup(BindGroup* bg,
                                        uint32_t groupIndex,
                                        const uint32_t* dynamicOffsets,
                                        uint32_t dynamicOffsetCount)
{
    (void)groupIndex;
    // m_mtlBuffers is sorted by WGSL `@binding` ascending at makeBindGroup
    // time, so `dynamicOffsets[i]` here pairs with the i-th dynamic UBO
    // in BindGroupLayout-entry order — matching WebGPU semantics
    // independently of the caller's `desc.ubos[]` order.
    uint32_t dynIdx = 0;
    for (auto& b : bg->m_mtlBuffers)
    {
        uint32_t offset = b.offset;
        if (b.hasDynamicOffset && dynIdx < dynamicOffsetCount)
            offset += dynamicOffsets[dynIdx++];
        // Per-stage emit: skip the stage whose slot is kAbsent so we
        // don't clobber another resource's slot in that stage's argument
        // table.
        if (b.vsSlot != BindingMap::kAbsent)
            [m_mtlEncoder setVertexBuffer:b.buffer
                                   offset:offset
                                  atIndex:b.vsSlot];
        if (b.fsSlot != BindingMap::kAbsent)
            [m_mtlEncoder setFragmentBuffer:b.buffer
                                     offset:offset
                                    atIndex:b.fsSlot];
    }
    for (auto& t : bg->m_mtlTextures)
    {
        if (t.vsSlot != BindingMap::kAbsent)
            [m_mtlEncoder setVertexTexture:t.texture atIndex:t.vsSlot];
        if (t.fsSlot != BindingMap::kAbsent)
            [m_mtlEncoder setFragmentTexture:t.texture atIndex:t.fsSlot];
    }
    for (auto& s : bg->m_mtlSamplers)
    {
        if (s.vsSlot != BindingMap::kAbsent)
            [m_mtlEncoder setVertexSamplerState:s.sampler atIndex:s.vsSlot];
        if (s.fsSlot != BindingMap::kAbsent)
            [m_mtlEncoder setFragmentSamplerState:s.sampler atIndex:s.fsSlot];
    }
}

inline void RenderPass::mtlSetViewport(
    float x, float y, float width, float height, float minDepth, float maxDepth)
{
    MTLViewport vp = {
        .originX = (double)x,
        .originY = (double)y,
        .width = (double)width,
        .height = (double)height,
        .znear = (double)minDepth,
        .zfar = (double)maxDepth,
    };
    [m_mtlEncoder setViewport:vp];
}

inline void RenderPass::mtlSetScissorRect(uint32_t x,
                                          uint32_t y,
                                          uint32_t width,
                                          uint32_t height)
{
    MTLScissorRect rect = {
        .x = x,
        .y = y,
        .width = width,
        .height = height,
    };
    [m_mtlEncoder setScissorRect:rect];
}

inline void RenderPass::mtlSetStencilRef(uint32_t ref)
{
    [m_mtlEncoder setStencilReferenceValue:ref];
}

inline void RenderPass::mtlSetBlendColor(float r, float g, float b, float a)
{
    [m_mtlEncoder setBlendColorRed:r green:g blue:b alpha:a];
}

inline void RenderPass::mtlDraw(uint32_t vertexCount,
                                uint32_t instanceCount,
                                uint32_t firstVertex,
                                uint32_t firstInstance)
{
    [m_mtlEncoder drawPrimitives:m_mtlPrimitiveType
                     vertexStart:firstVertex
                     vertexCount:vertexCount
                   instanceCount:instanceCount
                    baseInstance:firstInstance];
}

inline void RenderPass::mtlDrawIndexed(uint32_t indexCount,
                                       uint32_t instanceCount,
                                       uint32_t firstIndex,
                                       int32_t baseVertex,
                                       uint32_t firstInstance)
{
    assert(m_mtlIndexBuffer != nil &&
           "Must call setIndexBuffer before drawIndexed");

    uint32_t indexSize = (m_mtlIndexType == MTLIndexTypeUInt32)
                             ? sizeof(uint32_t)
                             : sizeof(uint16_t);
    NSUInteger indexBufferOffset =
        m_mtlIndexBufferOffset + firstIndex * indexSize;

    [m_mtlEncoder drawIndexedPrimitives:m_mtlPrimitiveType
                             indexCount:indexCount
                              indexType:m_mtlIndexType
                            indexBuffer:m_mtlIndexBuffer
                      indexBufferOffset:indexBufferOffset
                          instanceCount:instanceCount
                             baseVertex:baseVertex
                           baseInstance:firstInstance];
}

inline void RenderPass::mtlFinish()
{
    [m_mtlEncoder endEncoding];
    m_mtlEncoder = nil;
}

// ============================================================================
// RenderPass
// ============================================================================

// When both Metal and GL are compiled (macOS), ore_render_pass_metal_gl.mm
// provides all RenderPass method bodies with runtime dispatch. This file
// only contributes the static helper functions in that case.
#if !defined(ORE_BACKEND_GL)

RenderPass::~RenderPass()
{
    if (!m_finished && m_mtlEncoder != nil)
    {
        finish();
    }
}

RenderPass::RenderPass(RenderPass&& other) noexcept :
    m_mtlEncoder(other.m_mtlEncoder),
    m_mtlCommandBuffer(other.m_mtlCommandBuffer),
    m_mtlIndexBuffer(other.m_mtlIndexBuffer),
    m_mtlIndexType(other.m_mtlIndexType),
    m_mtlIndexBufferOffset(other.m_mtlIndexBufferOffset),
    m_mtlPrimitiveType(other.m_mtlPrimitiveType)
{
    moveCrossBackendFieldsFrom(other);
    other.m_mtlEncoder = nil;
    other.m_mtlCommandBuffer = nil;
    other.m_mtlIndexBuffer = nil;
}

RenderPass& RenderPass::operator=(RenderPass&& other) noexcept
{
    if (this != &other)
    {
        if (!m_finished && m_mtlEncoder != nil)
        {
            finish();
        }
        moveCrossBackendFieldsFrom(other);
        m_mtlEncoder = other.m_mtlEncoder;
        m_mtlCommandBuffer = other.m_mtlCommandBuffer;
        m_mtlIndexBuffer = other.m_mtlIndexBuffer;
        m_mtlIndexType = other.m_mtlIndexType;
        m_mtlIndexBufferOffset = other.m_mtlIndexBufferOffset;
        m_mtlPrimitiveType = other.m_mtlPrimitiveType;
        other.m_mtlEncoder = nil;
        other.m_mtlCommandBuffer = nil;
        other.m_mtlIndexBuffer = nil;
    }
    return *this;
}

void RenderPass::validate() const
{
    assert(!m_finished && "RenderPass already finished");
    assert(m_mtlEncoder != nil);
}

void RenderPass::setPipeline(Pipeline* pipeline)
{
    validate();
    if (!checkPipelineCompat(pipeline))
        return;
    mtlSetPipeline(pipeline);
}

void RenderPass::setVertexBuffer(uint32_t slot, Buffer* buffer, uint32_t offset)
{
    validate();
    mtlSetVertexBuffer(slot, buffer, offset);
}

void RenderPass::setIndexBuffer(Buffer* buffer,
                                IndexFormat format,
                                uint32_t offset)
{
    validate();
    mtlSetIndexBuffer(buffer, format, offset);
}

void RenderPass::setBindGroup(uint32_t groupIndex,
                              BindGroup* bg,
                              const uint32_t* dynamicOffsets,
                              uint32_t dynamicOffsetCount)
{
    validate();
    m_boundGroups[groupIndex] = ref_rcp(bg);
    mtlSetBindGroup(bg, groupIndex, dynamicOffsets, dynamicOffsetCount);
}

void RenderPass::setViewport(
    float x, float y, float width, float height, float minDepth, float maxDepth)
{
    validate();
    mtlSetViewport(x, y, width, height, minDepth, maxDepth);
}

void RenderPass::setScissorRect(uint32_t x,
                                uint32_t y,
                                uint32_t width,
                                uint32_t height)
{
    validate();
    mtlSetScissorRect(x, y, width, height);
}

void RenderPass::setStencilReference(uint32_t ref)
{
    validate();
    mtlSetStencilRef(ref);
}

void RenderPass::setBlendColor(float r, float g, float b, float a)
{
    validate();
    mtlSetBlendColor(r, g, b, a);
}

void RenderPass::draw(uint32_t vertexCount,
                      uint32_t instanceCount,
                      uint32_t firstVertex,
                      uint32_t firstInstance)
{
    validate();
    mtlDraw(vertexCount, instanceCount, firstVertex, firstInstance);
}

void RenderPass::drawIndexed(uint32_t indexCount,
                             uint32_t instanceCount,
                             uint32_t firstIndex,
                             int32_t baseVertex,
                             uint32_t firstInstance)
{
    validate();
    mtlDrawIndexed(
        indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
}

void RenderPass::finish()
{
    if (m_finished)
        return;
    m_finished = true;
    if (m_mtlEncoder != nil)
    {
        mtlFinish();
    }
    // Release bound BindGroup refs.
    for (auto& bg : m_boundGroups)
        bg.reset();
}

#endif // !ORE_BACKEND_GL

} // namespace rive::ore
