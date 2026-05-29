#pragma once
#include "rive/renderer/ore/ore_bind_group_layout.hpp"
#include <webgpu/webgpu_cpp.h>

namespace rive::ore
{
class ContextWGPU;

class BindGroupLayoutWGPU
    : public LITE_RTTI_OVERRIDE(BindGroupLayout, BindGroupLayoutWGPU)
{
public:
    BindGroupLayoutWGPU() = default;
    ~BindGroupLayoutWGPU() override =
        default; // wgpu::BindGroupLayout RAII destructor releases

private:
    friend class ContextWGPU;
    wgpu::BindGroupLayout m_wgpuBGL;
};
} // namespace rive::ore
