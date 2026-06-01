#pragma once
#include "rive/renderer/ore/ore_sampler.hpp"
#include <webgpu/webgpu_cpp.h>

namespace rive::ore
{
class ContextWGPU;

class SamplerWGPU : public LITE_RTTI_OVERRIDE(Sampler, SamplerWGPU)
{
public:
    SamplerWGPU() = default;
    ~SamplerWGPU() override = default; // wgpu::Sampler RAII destructor releases

private:
    friend class ContextWGPU;
    wgpu::Sampler m_wgpuSampler;
};
} // namespace rive::ore
