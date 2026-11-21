#pragma once
#include "rive/renderer/ore/ore_pipeline.hpp"
#import <Metal/Metal.h>

namespace rive::ore
{
class ContextMetal;
class RenderPassMetal;

// Where vertex buffers start unless a buffer binding is in the way.
constexpr uint32_t kMetalVertexBufferBase = 16;

class PipelineMetal : public LITE_RTTI_OVERRIDE(Pipeline, PipelineMetal)
{
public:
  PipelineMetal(const PipelineDesc& desc) : lite_rtti_override(desc) {}
  ~PipelineMetal() override = default; // ARC releases Metal objects

private:
  friend class ContextMetal;
  friend class RenderPassMetal;
  id<MTLRenderPipelineState> m_mtlPipeline = nil;
  id<MTLDepthStencilState> m_mtlDepthStencil = nil;
  // Metal keeps vertex buffers and the buffers we bind on the vertex stage in
  // one table, so each vertex buffer takes a slot those leave free.
  uint8_t m_vertexBufferIndex[kMaxVertexBufferSlots] = {};
};
} // namespace rive::ore
