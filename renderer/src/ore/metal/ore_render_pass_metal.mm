/*
 * Copyright 2025 Rive
 */

#include "ore_render_pass_metal.hpp"
#include "ore_buffer_metal.hpp"
#include "ore_pipeline_metal.hpp"
#include "ore_bind_group_metal.hpp"
#include "rive/renderer/ore/ore_context_metal.hpp"
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

// Vertex buffers are bound at slots [kMetalVertexBufferBase, ...) to avoid
// colliding with uniform buffers mapped to the low buffer indices
// ([[buffer(0)]] etc). Must stay in sync with ore_context_metal.mm.
static constexpr uint32_t kMetalVertexBufferBase = 16;

// ============================================================================
// RenderPassMetal
// ============================================================================

RenderPassMetal::~RenderPassMetal()
{
    if (!m_finished && m_mtlEncoder != nil)
        finish();
}

RenderPassMetal::RenderPassMetal(RenderPassMetal&& other) noexcept :
    m_mtlEncoder(other.m_mtlEncoder),
    m_mtlCommandBuffer(other.m_mtlCommandBuffer),
    m_mtlIndexBuffer(other.m_mtlIndexBuffer),
    m_mtlIndexType(other.m_mtlIndexType),
    m_mtlIndexBufferOffset(other.m_mtlIndexBufferOffset),
    m_mtlPrimitiveType(other.m_mtlPrimitiveType),
    m_currentPipeline(std::move(other.m_currentPipeline))
{
    other.m_mtlEncoder = nil;
    other.m_mtlCommandBuffer = nil;
    other.m_mtlIndexBuffer = nil;
}

RenderPassMetal& RenderPassMetal::operator=(RenderPassMetal&& other) noexcept
{
    if (this != &other)
    {
        if (!m_finished && m_mtlEncoder != nil)
            finish();
        m_mtlEncoder = other.m_mtlEncoder;
        m_mtlCommandBuffer = other.m_mtlCommandBuffer;
        m_mtlIndexBuffer = other.m_mtlIndexBuffer;
        m_mtlIndexType = other.m_mtlIndexType;
        m_mtlIndexBufferOffset = other.m_mtlIndexBufferOffset;
        m_mtlPrimitiveType = other.m_mtlPrimitiveType;
        m_currentPipeline = std::move(other.m_currentPipeline);
        other.m_mtlEncoder = nil;
        other.m_mtlCommandBuffer = nil;
        other.m_mtlIndexBuffer = nil;
    }
    return *this;
}

void RenderPassMetal::validate() const
{
    assert(!m_finished && "RenderPassMetal already finished");
    assert(m_mtlEncoder != nil);
}

void RenderPassMetal::setPipeline(Pipeline* pipeline)
{
    validate();
    if (!checkPipelineCompat(pipeline))
        return;

    auto* p = static_cast<PipelineMetal*>(pipeline);
    [m_mtlEncoder setRenderPipelineState:p->m_mtlPipeline];
    [m_mtlEncoder setDepthStencilState:p->m_mtlDepthStencil];

    const auto& desc = pipeline->desc();
    [m_mtlEncoder setCullMode:oreCullModeToMTL(desc.cullMode)];
    [m_mtlEncoder setFrontFacingWinding:oreWindingToMTL(desc.winding)];
    m_mtlPrimitiveType = orePrimitiveTopologyToMTL(desc.topology);
    m_currentPipeline = ref_rcp(pipeline);

    if (desc.depthStencil.depthBias != 0 ||
        desc.depthStencil.depthBiasSlopeScale != 0.0f)
    {
        [m_mtlEncoder setDepthBias:(float)desc.depthStencil.depthBias
                        slopeScale:desc.depthStencil.depthBiasSlopeScale
                             clamp:desc.depthStencil.depthBiasClamp];
    }
}

void RenderPassMetal::setVertexBuffer(uint32_t slot,
                                      Buffer* buffer,
                                      uint32_t offset)
{
    validate();
    auto* b = static_cast<BufferMetal*>(buffer);
    [m_mtlEncoder setVertexBuffer:b->m_mtlBuffer
                           offset:offset
                          atIndex:slot + kMetalVertexBufferBase];
}

void RenderPassMetal::setIndexBuffer(Buffer* buffer,
                                     IndexFormat format,
                                     uint32_t offset)
{
    validate();
    auto* b = static_cast<BufferMetal*>(buffer);
    m_mtlIndexBuffer = b->m_mtlBuffer;
    m_mtlIndexType = oreIndexFormatToMTL(format);
    m_mtlIndexBufferOffset = offset;
}

void RenderPassMetal::setBindGroup(uint32_t groupIndex,
                                   BindGroup* bg,
                                   const uint32_t* dynamicOffsets,
                                   uint32_t dynamicOffsetCount)
{
    validate();
    m_boundGroups[groupIndex] = ref_rcp(bg);

    auto* bgMetal = static_cast<BindGroupMetal*>(bg);
    (void)groupIndex;
    uint32_t dynIdx = 0;
    for (auto& b : bgMetal->m_mtlBuffers)
    {
        uint32_t offset = b.offset;
        if (b.hasDynamicOffset && dynIdx < dynamicOffsetCount)
            offset += dynamicOffsets[dynIdx++];
        if (b.vsSlot != BindingMap::kAbsent)
            [m_mtlEncoder setVertexBuffer:b.buffer
                                   offset:offset
                                  atIndex:b.vsSlot];
        if (b.fsSlot != BindingMap::kAbsent)
            [m_mtlEncoder setFragmentBuffer:b.buffer
                                     offset:offset
                                    atIndex:b.fsSlot];
    }
    for (auto& t : bgMetal->m_mtlTextures)
    {
        if (t.vsSlot != BindingMap::kAbsent)
            [m_mtlEncoder setVertexTexture:t.texture atIndex:t.vsSlot];
        if (t.fsSlot != BindingMap::kAbsent)
            [m_mtlEncoder setFragmentTexture:t.texture atIndex:t.fsSlot];
    }
    for (auto& s : bgMetal->m_mtlSamplers)
    {
        if (s.vsSlot != BindingMap::kAbsent)
            [m_mtlEncoder setVertexSamplerState:s.sampler atIndex:s.vsSlot];
        if (s.fsSlot != BindingMap::kAbsent)
            [m_mtlEncoder setFragmentSamplerState:s.sampler atIndex:s.fsSlot];
    }
}

void RenderPassMetal::setViewport(
    float x, float y, float width, float height, float minDepth, float maxDepth)
{
    validate();
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

void RenderPassMetal::setScissorRect(uint32_t x,
                                     uint32_t y,
                                     uint32_t width,
                                     uint32_t height)
{
    validate();
    MTLScissorRect rect = {.x = x, .y = y, .width = width, .height = height};
    [m_mtlEncoder setScissorRect:rect];
}

void RenderPassMetal::setStencilReference(uint32_t ref)
{
    validate();
    [m_mtlEncoder setStencilReferenceValue:ref];
}

void RenderPassMetal::setBlendColor(float r, float g, float b, float a)
{
    validate();
    [m_mtlEncoder setBlendColorRed:r green:g blue:b alpha:a];
}

void RenderPassMetal::draw(uint32_t vertexCount,
                           uint32_t instanceCount,
                           uint32_t firstVertex,
                           uint32_t firstInstance)
{
    validate();
    [m_mtlEncoder drawPrimitives:m_mtlPrimitiveType
                     vertexStart:firstVertex
                     vertexCount:vertexCount
                   instanceCount:instanceCount
                    baseInstance:firstInstance];
}

void RenderPassMetal::drawIndexed(uint32_t indexCount,
                                  uint32_t instanceCount,
                                  uint32_t firstIndex,
                                  int32_t baseVertex,
                                  uint32_t firstInstance)
{
    validate();
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

void RenderPassMetal::finish()
{
    if (m_finished)
        return;
    m_finished = true;
    if (m_mtlEncoder != nil)
    {
        [m_mtlEncoder endEncoding];
        m_mtlEncoder = nil;
    }
    for (auto& bg : m_boundGroups)
        bg.reset();
    m_currentPipeline.reset();
}

} // namespace rive::ore
