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

// A deferred object self reports its creation id; a real resource falls back
// to the buffer's keep alive capture.
template <typename DeferredT, typename T>
ResourceHandle recordedHandle(OreCommandBuffer& cmd, T* r)
{
  if (auto* d = lite_rtti_cast<DeferredT*>(r))
  {
    return d->clientHandle();
  }
  return cmd.capture(r);
}

// A pass takes commands only as the innermost on its stream, so the ones
// begun inside it there finish first and run ahead of it.
inline void settlePass(Context* context, Pass* pass, OreCommandBuffer& cmd)
{
  if (context != nullptr)
  {
    context->finishNestedPasses(pass, &cmd);
  }
}

inline void noteBoundStorage(
    Context* context,
    const Pass* pass,
    const BindGroup* group)
{
  if (context != nullptr && group != nullptr)
  {
    context->noteBoundStorage(pass, group);
  }
}

inline void recordSetBindGroup(
    OreCommandBuffer& cmd,
    uint32_t groupIndex,
    BindGroup* bg,
    const uint32_t* dynamicOffsets,
    uint32_t dynamicOffsetCount)
{
  SetBindGroupCmd c{};
  c.groupIndex = groupIndex;
  c.bindGroup = recordedHandle<DeferredBindGroup>(cmd, bg);
  c.dynamicOffsetCount = dynamicOffsetCount;
  c.dynamicOffsetStart = dynamicOffsetCount > 0
                             ? cmd.appendBlob(
                                   dynamicOffsets,
                                   dynamicOffsetCount * sizeof(uint32_t))
                             : 0;
  cmd.append(CommandType::setBindGroup, c);
}

class RenderPassRecording : public RenderPass
{
public:
  // Mirrors a backend beginRenderPass so the validators see attachments.
  RenderPassRecording(
      Context* context,
      OreCommandBuffer* cmd,
      const RenderPassDesc& desc)
      : RenderPass(context)
      , m_cmd(cmd)
  {
    populateAttachmentMetadata(desc);

    BeginRenderPassCmd begin{};
    begin.label = m_cmd->appendStringRef(desc.label);
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
    if (m_context != nullptr)
    {
      m_context->beginOpenPass(this, *m_cmd);
    }
    m_cmd->append(CommandType::beginRenderPass, begin);
  }

  // A begin with no finish would corrupt the stream at replay.
  ~RenderPassRecording() override
  {
    if (!m_finished)
    {
      finish();
    }
  }

  void setPipeline(Pipeline* pipeline) override
  {
    if (!settle() || !checkPipelineCompat(pipeline))
    {
      return;
    }
    m_cmd->append(CommandType::setPipeline, SetPipelineCmd{idOf(pipeline)});
  }

  void setVertexBuffer(uint32_t slot, Buffer* buffer, uint32_t offset = 0)
      override
  {
    if (settle())
    {
      m_cmd->append(
          CommandType::setVertexBuffer,
          SetVertexBufferCmd{slot, idOf(buffer), offset});
    }
  }

  void setIndexBuffer(Buffer* buffer, IndexFormat format, uint32_t offset = 0)
      override
  {
    if (!settle())
    {
      return;
    }
    SetIndexBufferCmd c{};
    c.buffer = idOf(buffer);
    c.format = format;
    c.offset = offset;
    m_cmd->append(CommandType::setIndexBuffer, c);
  }

  void setBindGroup(
      uint32_t groupIndex,
      BindGroup* bg,
      const uint32_t* dynamicOffsets = nullptr,
      uint32_t dynamicOffsetCount = 0) override
  {
    if (!settle())
    {
      return;
    }
    // Hold a strong reference so GC cannot free the group before replay.
    if (groupIndex < kMaxBindGroups)
    {
      m_boundGroups[groupIndex] = ref_rcp(bg);
    }
    noteBoundStorage(m_context, this, bg);
    recordSetBindGroup(
        *m_cmd,
        groupIndex,
        bg,
        dynamicOffsets,
        dynamicOffsetCount);
  }

  void setViewport(
      float x,
      float y,
      float width,
      float height,
      float minDepth = 0.0f,
      float maxDepth = 1.0f) override
  {
    record(
        CommandType::setViewport,
        SetViewportCmd{x, y, width, height, minDepth, maxDepth});
  }

  void setScissorRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
      override
  {
    record(CommandType::setScissorRect, SetScissorRectCmd{x, y, width, height});
  }

  void setStencilReference(uint32_t ref) override
  {
    record(CommandType::setStencilReference, SetStencilReferenceCmd{ref});
  }

  void setBlendColor(float r, float g, float b, float a) override
  {
    record(CommandType::setBlendColor, SetBlendColorCmd{r, g, b, a});
  }

  void draw(
      uint32_t vertexCount,
      uint32_t instanceCount = 1,
      uint32_t firstVertex = 0,
      uint32_t firstInstance = 0) override
  {
    record(
        CommandType::draw,
        DrawCmd{vertexCount, instanceCount, firstVertex, firstInstance});
  }

  void drawIndexed(
      uint32_t indexCount,
      uint32_t instanceCount = 1,
      uint32_t firstIndex = 0,
      int32_t baseVertex = 0,
      uint32_t firstInstance = 0) override
  {
    record(
        CommandType::drawIndexed,
        DrawIndexedCmd{
            indexCount,
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
    if (m_context != nullptr)
    {
      m_context->finishNestedPasses(this);
    }
    m_cmd->appendOpcode(CommandType::finish);
    m_finished = true;
    for (uint32_t i = 0; i < kMaxBindGroups; ++i)
    {
      m_boundGroups[i] = nullptr;
    }
    if (m_context != nullptr)
    {
      m_context->finishOpenPass(this);
    }
  }

private:
  // False for a finished pass, which records and retains nothing more.
  bool settle()
  {
    if (m_finished)
    {
      return false;
    }
    settlePass(m_context, this, *m_cmd);
    return true;
  }

  template <typename POD> void record(CommandType type, const POD& pod)
  {
    if (settle())
    {
      m_cmd->append(type, pod);
    }
  }

  ResourceHandle idOf(Buffer* b)
  {
    return recordedHandle<DeferredBuffer>(*m_cmd, b);
  }
  ResourceHandle idOf(Pipeline* p)
  {
    return recordedHandle<DeferredPipeline>(*m_cmd, p);
  }
  ResourceHandle idOf(TextureView* v)
  {
    return recordedHandle<DeferredTextureView>(*m_cmd, v);
  }

  OreCommandBuffer* m_cmd;
};

} // namespace rive::ore::cmd
