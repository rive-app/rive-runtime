#pragma once
#include "rive/renderer/ore/ore_shader_module.hpp"
#include <webgpu/webgpu_cpp.h>

namespace rive::ore
{
class ContextWGPU;

class ShaderModuleWGPU
    : public LITE_RTTI_OVERRIDE(ShaderModule, ShaderModuleWGPU)
{
public:
    ShaderModuleWGPU() = default;
    ~ShaderModuleWGPU() override =
        default; // wgpu::ShaderModule RAII destructor releases

private:
    friend class ContextWGPU;
    wgpu::ShaderModule m_wgpuShaderModule;
};
} // namespace rive::ore
