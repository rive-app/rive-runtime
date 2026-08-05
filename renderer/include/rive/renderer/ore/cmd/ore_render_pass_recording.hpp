/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/renderer/ore/cmd/ore_command_buffer.hpp"
#include "rive/renderer/ore/cmd/ore_deferred_resource.hpp"
#include "rive/renderer/ore/ore_context.hpp"
#include "rive/renderer/ore/ore_render_pass.hpp"
#include "utils/lite_rtti.hpp"

// Deferred mode ore::RenderPass. Each virtual runs the base class validation
// then appends the matching command to an OreCommandBuffer for later replay.
// Validation stays on the recording thread and failing commands are not
// appended, so replay only ever sees structurally valid streams.
namespace rive::ore::cmd
{

class RenderPassRecording : public RenderPass
{
public:
    // Mirrors a backend beginRenderPass so the validators see attachments.
    RenderPassRecording(Context* context,
                        OreCommandBuffer* cmd,
                        const RenderPassDesc& desc) :
        RenderPass(context), m_cmd(cmd)
    {
        populateAttachmentMetadata(desc);

        BeginRenderPassCmd begin{};
        begin.colorCount = desc.colorCount;
        for (uint32_t i = 0; i < desc.colorCount && i < 4; ++i)
        {
            const ColorAttachment& src = desc.colorAttachments[i];
            ColorAttachmentPOD& dst = begin.colors[i];
            dst.view = idOf(src.view);
            dst.resolveTarget = idOf(src.resolveTarget);
            dst.loadOp = src.loadOp;
            dst.storeOp = src.storeOp;
            dst.clearR = src.clearColor.r;
            dst.clearG = src.clearColor.g;
            dst.clearB = src.clearColor.b;
            dst.clearA = src.clearColor.a;
        }
        const DepthStencilAttachment& ds = desc.depthStencil;
        begin.depthStencil.view = idOf(ds.view);
        begin.depthStencil.depthLoadOp = ds.depthLoadOp;
        begin.depthStencil.depthStoreOp = ds.depthStoreOp;
        begin.depthStencil.depthClearValue = ds.depthClearValue;
        begin.depthStencil.stencilLoadOp = ds.stencilLoadOp;
        begin.depthStencil.stencilStoreOp = ds.stencilStoreOp;
        begin.depthStencil.stencilClearValue = ds.stencilClearValue;
        m_cmd->append(CommandType::beginRenderPass, begin);
    }

    void setPipeline(Pipeline* pipeline) override
    {
        if (!checkPipelineCompat(pipeline))
        {
            return;
        }
        m_cmd->append(CommandType::setPipeline, SetPipelineCmd{idOf(pipeline)});
    }

    void setVertexBuffer(uint32_t slot,
                         Buffer* buffer,
                         uint32_t offset = 0) override
    {
        m_cmd->append(CommandType::setVertexBuffer,
                      SetVertexBufferCmd{slot, idOf(buffer), offset});
    }

    void setIndexBuffer(Buffer* buffer,
                        IndexFormat format,
                        uint32_t offset = 0) override
    {
        m_cmd->append(CommandType::setIndexBuffer,
                      SetIndexBufferCmd{idOf(buffer), format, offset});
    }

    void setBindGroup(uint32_t groupIndex,
                      BindGroup* bg,
                      const uint32_t* dynamicOffsets = nullptr,
                      uint32_t dynamicOffsetCount = 0) override
    {
        // Hold a strong reference so GC cannot free the group before replay.
        if (groupIndex < kMaxBindGroups)
        {
            m_boundGroups[groupIndex] = ref_rcp(bg);
        }

        SetBindGroupCmd c{};
        c.groupIndex = groupIndex;
        c.bindGroup = idOf(bg);
        c.dynamicOffsetCount = dynamicOffsetCount;
        c.dynamicOffsetStart =
            dynamicOffsetCount > 0
                ? m_cmd->appendBlob(dynamicOffsets,
                                    dynamicOffsetCount * sizeof(uint32_t))
                : 0;
        m_cmd->append(CommandType::setBindGroup, c);
    }

    void setViewport(float x,
                     float y,
                     float width,
                     float height,
                     float minDepth = 0.0f,
                     float maxDepth = 1.0f) override
    {
        m_cmd->append(CommandType::setViewport,
                      SetViewportCmd{x, y, width, height, minDepth, maxDepth});
    }

    void setScissorRect(uint32_t x,
                        uint32_t y,
                        uint32_t width,
                        uint32_t height) override
    {
        m_cmd->append(CommandType::setScissorRect,
                      SetScissorRectCmd{x, y, width, height});
    }

    void setStencilReference(uint32_t ref) override
    {
        m_cmd->append(CommandType::setStencilReference,
                      SetStencilReferenceCmd{ref});
    }

    void setBlendColor(float r, float g, float b, float a) override
    {
        m_cmd->append(CommandType::setBlendColor, SetBlendColorCmd{r, g, b, a});
    }

    void draw(uint32_t vertexCount,
              uint32_t instanceCount = 1,
              uint32_t firstVertex = 0,
              uint32_t firstInstance = 0) override
    {
        m_cmd->append(
            CommandType::draw,
            DrawCmd{vertexCount, instanceCount, firstVertex, firstInstance});
    }

    void drawIndexed(uint32_t indexCount,
                     uint32_t instanceCount = 1,
                     uint32_t firstIndex = 0,
                     int32_t baseVertex = 0,
                     uint32_t firstInstance = 0) override
    {
        m_cmd->append(CommandType::drawIndexed,
                      DrawIndexedCmd{indexCount,
                                     instanceCount,
                                     firstIndex,
                                     baseVertex,
                                     firstInstance});
    }

    void finish() override
    {
        if (m_finished)
        {
            return;
        }
        m_cmd->appendOpcode(CommandType::finish);
        m_finished = true;
        for (uint32_t i = 0; i < kMaxBindGroups; ++i)
        {
            m_boundGroups[i] = nullptr;
        }
    }

private:
    // A deferred object self reports its creation id; a real resource falls
    // back to the buffer's keep alive capture.
    template <typename DeferredT, typename T> ResourceHandle idOfAs(T* r)
    {
        if (auto* d = lite_rtti_cast<DeferredT*>(r))
        {
            return d->clientHandle();
        }
        return m_cmd->capture(r);
    }
    ResourceHandle idOf(Buffer* b) { return idOfAs<DeferredBuffer>(b); }
    ResourceHandle idOf(Pipeline* p) { return idOfAs<DeferredPipeline>(p); }
    ResourceHandle idOf(TextureView* v)
    {
        return idOfAs<DeferredTextureView>(v);
    }
    ResourceHandle idOf(BindGroup* g) { return idOfAs<DeferredBindGroup>(g); }

    OreCommandBuffer* m_cmd;
};

} // namespace rive::ore::cmd
