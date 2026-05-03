/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/ore/ore_render_pass.hpp"
#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_context.hpp"
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_texture.hpp"
#include "rive/renderer/ore/ore_sampler.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/rive_types.hpp"

namespace rive::ore
{

// ============================================================================
// RenderPass
// ============================================================================

RenderPass::~RenderPass()
{
    if (!m_finished && m_wgpuPassEncoder != nullptr)
    {
        finish();
    }
}

RenderPass::RenderPass(RenderPass&& other) noexcept :
    m_currentPipeline(std::move(other.m_currentPipeline)),
    m_wgpuContext(other.m_wgpuContext),
    m_wgpuPassEncoder(std::move(other.m_wgpuPassEncoder)),
    m_wgpuIndexBuffer(std::move(other.m_wgpuIndexBuffer)),
    m_wgpuIndexFormat(other.m_wgpuIndexFormat),
    m_wgpuIndexOffset(other.m_wgpuIndexOffset)
{
    moveCrossBackendFieldsFrom(other);
    other.m_wgpuContext = nullptr;
}

RenderPass& RenderPass::operator=(RenderPass&& other) noexcept
{
    if (this != &other)
    {
        if (!m_finished && m_wgpuPassEncoder != nullptr)
            finish();

        moveCrossBackendFieldsFrom(other);
        m_wgpuContext = other.m_wgpuContext;
        m_currentPipeline = std::move(other.m_currentPipeline);
        m_wgpuPassEncoder = std::move(other.m_wgpuPassEncoder);
        m_wgpuIndexBuffer = std::move(other.m_wgpuIndexBuffer);
        m_wgpuIndexFormat = other.m_wgpuIndexFormat;
        m_wgpuIndexOffset = other.m_wgpuIndexOffset;

        other.m_wgpuContext = nullptr;
    }
    return *this;
}

void RenderPass::validate() const
{
    assert(!m_finished && "RenderPass already finished");
    assert(m_wgpuPassEncoder != nullptr);
}

void RenderPass::setPipeline(Pipeline* pipeline)
{
    validate();
    if (!checkPipelineCompat(pipeline))
        return;
    m_currentPipeline = ref_rcp(pipeline);
    m_wgpuPassEncoder.SetPipeline(pipeline->m_wgpuPipeline);
}

void RenderPass::setVertexBuffer(uint32_t slot, Buffer* buffer, uint32_t offset)
{
    validate();
    m_wgpuPassEncoder.SetVertexBuffer(slot,
                                      buffer->m_wgpuBuffer,
                                      offset,
                                      buffer->size() - offset);
}

void RenderPass::setIndexBuffer(Buffer* buffer,
                                IndexFormat format,
                                uint32_t offset)
{
    validate();
    wgpu::IndexFormat wFmt = (format == IndexFormat::uint32)
                                 ? wgpu::IndexFormat::Uint32
                                 : wgpu::IndexFormat::Uint16;
    m_wgpuIndexBuffer = buffer->m_wgpuBuffer;
    m_wgpuIndexFormat = wFmt;
    m_wgpuIndexOffset = offset;
    m_wgpuPassEncoder.SetIndexBuffer(buffer->m_wgpuBuffer,
                                     wFmt,
                                     offset,
                                     buffer->size() - offset);
}

void RenderPass::setBindGroup(uint32_t groupIndex,
                              BindGroup* bg,
                              const uint32_t* dynamicOffsets,
                              uint32_t dynamicOffsetCount)
{
    validate();
    assert(bg != nullptr);

    m_wgpuPassEncoder.SetBindGroup(groupIndex,
                                   bg->m_wgpuBindGroup,
                                   dynamicOffsetCount,
                                   dynamicOffsets);

    // Hold a strong ref so the BindGroup (and its retained resources) stay
    // alive until the render pass is finished.
    m_boundGroups[groupIndex] = ref_rcp(bg);
}

void RenderPass::setViewport(float x,
                             float y,
                             float width,
                             float height,
                             float minDepth,
                             float maxDepth)
{
    validate();
    m_wgpuPassEncoder.SetViewport(x, y, width, height, minDepth, maxDepth);
}

void RenderPass::setScissorRect(uint32_t x,
                                uint32_t y,
                                uint32_t width,
                                uint32_t height)
{
    validate();
    m_wgpuPassEncoder.SetScissorRect(x, y, width, height);
}

void RenderPass::setStencilReference(uint32_t ref)
{
    validate();
    m_wgpuPassEncoder.SetStencilReference(ref);
}

void RenderPass::setBlendColor(float r, float g, float b, float a)
{
    validate();
    wgpu::Color color{r, g, b, a};
    m_wgpuPassEncoder.SetBlendConstant(&color);
}

void RenderPass::draw(uint32_t vertexCount,
                      uint32_t instanceCount,
                      uint32_t firstVertex,
                      uint32_t firstInstance)
{
    validate();
    assert(m_currentPipeline != nullptr && "setPipeline must be called first");

    m_wgpuPassEncoder.Draw(vertexCount,
                           instanceCount,
                           firstVertex,
                           firstInstance);
}

void RenderPass::drawIndexed(uint32_t indexCount,
                             uint32_t instanceCount,
                             uint32_t firstIndex,
                             int32_t baseVertex,
                             uint32_t firstInstance)
{
    validate();
    assert(m_currentPipeline != nullptr && "setPipeline must be called first");

    m_wgpuPassEncoder.DrawIndexed(indexCount,
                                  instanceCount,
                                  firstIndex,
                                  baseVertex,
                                  firstInstance);
}

void RenderPass::finish()
{
    if (m_finished)
        return;
    m_finished = true;

    if (m_wgpuPassEncoder != nullptr)
    {
        m_wgpuPassEncoder.End();
        m_wgpuPassEncoder = nullptr;
    }

    m_wgpuContext = nullptr;
    m_currentPipeline = nullptr;
}

} // namespace rive::ore
