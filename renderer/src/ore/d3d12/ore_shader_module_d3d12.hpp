#pragma once
#include "rive/renderer/ore/ore_shader_module.hpp"
#include <cstdint>
#include <vector>

namespace rive::ore
{
class ContextD3D12;

class ShaderModuleD3D12
    : public LITE_RTTI_OVERRIDE(ShaderModule, ShaderModuleD3D12)
{
public:
    ShaderModuleD3D12() = default;
    ~ShaderModuleD3D12() override = default;

private:
    friend class ContextD3D12;
    std::vector<uint8_t> m_d3dBytecode;
    bool m_d3dIsVertex = false;
};
} // namespace rive::ore
