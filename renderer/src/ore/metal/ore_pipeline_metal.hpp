#pragma once
#include "rive/renderer/ore/ore_pipeline.hpp"
#import <Metal/Metal.h>

namespace rive::ore
{
class ContextMetal;
class RenderPassMetal;

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
};
} // namespace rive::ore
