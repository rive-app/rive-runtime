#pragma once
#include "rive/renderer/ore/ore_bind_group.hpp"
#include <webgpu/webgpu_cpp.h>

namespace rive::ore
{
class ContextWGPU;

class BindGroupWGPU : public LITE_RTTI_OVERRIDE(BindGroup, BindGroupWGPU)
{
public:
    BindGroupWGPU() = default;
    ~BindGroupWGPU() override =
        default; // wgpu::BindGroup RAII destructor releases

private:
    friend class ContextWGPU;
    friend class RenderPassWGPU;
    wgpu::BindGroup m_wgpuBindGroup;
};
} // namespace rive::ore
