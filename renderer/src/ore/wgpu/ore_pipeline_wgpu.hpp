#pragma once
#include "rive/renderer/ore/ore_pipeline.hpp"
#include <webgpu/webgpu_cpp.h>

namespace rive::ore
{
class ContextWGPU;

class PipelineWGPU : public LITE_RTTI_OVERRIDE(Pipeline, PipelineWGPU)
{
public:
    PipelineWGPU(const PipelineDesc& desc) : lite_rtti_override(desc) {}
    ~PipelineWGPU() override = default; // RAII destructors release wgpu objects

private:
    friend class ContextWGPU;
    friend class RenderPassWGPU;
    wgpu::Device m_wgpuDevice;
    wgpu::RenderPipeline m_wgpuPipeline;
    wgpu::PipelineLayout m_wgpuPipelineLayout;
};
} // namespace rive::ore
