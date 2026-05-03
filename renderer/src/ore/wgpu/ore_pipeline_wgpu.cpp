/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/renderer/ore/ore_bind_group_layout.hpp"

namespace rive::ore
{

void Pipeline::onRefCntReachedZero() const
{
    // wgpu::RenderPipeline and wgpu::PipelineLayout RAII destructors release
    // the GPU resources. Per-group BGLs are owned by `m_layouts[g]` and freed
    // when those rcps drop.
    delete this;
}

void BindGroupLayout::onRefCntReachedZero() const
{
    // wgpu::BindGroupLayout RAII destructor releases the GPU resource.
    delete this;
}

} // namespace rive::ore
