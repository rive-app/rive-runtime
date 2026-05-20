#pragma once
#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_binding_map.hpp"
#include <d3d11.h>
#include <cstdint>
#include <vector>

namespace rive::ore
{
class ContextD3D11;

class BindGroupD3D11 : public LITE_RTTI_OVERRIDE(BindGroup, BindGroupD3D11)
{
public:
    struct D3D11UBOBinding
    {
        ID3D11Buffer* buffer =
            nullptr;          // raw ptr; ComPtr in ShaderModule keeps it alive
        uint32_t binding = 0; // WGSL @binding, for sort
        uint16_t vsSlot = BindingMap::kAbsent;
        uint16_t psSlot = BindingMap::kAbsent;
        uint32_t offset = 0;
        uint32_t size = 0;
        bool hasDynamicOffset = false;
    };
    struct D3D11TexBinding
    {
        ID3D11ShaderResourceView* srv = nullptr;
        uint16_t vsSlot = BindingMap::kAbsent;
        uint16_t psSlot = BindingMap::kAbsent;
    };
    struct D3D11SamplerBinding
    {
        ID3D11SamplerState* sampler = nullptr;
        uint16_t vsSlot = BindingMap::kAbsent;
        uint16_t psSlot = BindingMap::kAbsent;
    };

    BindGroupD3D11() = default;
    ~BindGroupD3D11() override = default;

private:
    friend class ContextD3D11;
    friend class RenderPassD3D11;
    std::vector<D3D11UBOBinding> m_d3d11UBOs;
    std::vector<D3D11TexBinding> m_d3d11Textures;
    std::vector<D3D11SamplerBinding> m_d3d11Samplers;
};
} // namespace rive::ore
