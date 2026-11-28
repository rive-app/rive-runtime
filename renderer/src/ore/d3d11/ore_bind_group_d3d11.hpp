#pragma once
#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_binding_map.hpp"
#include <d3d11.h>
#include <wrl/client.h>
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
    ID3D11Buffer*
        buffer = nullptr; // raw ptr; ComPtr in ShaderModule keeps it alive
    uint32_t binding = 0; // WGSL @binding, for sort
    uint32_t offset = 0;
    uint32_t size = 0;
    bool hasDynamicOffset = false;
  };
  struct D3D11TexBinding
  {
    ID3D11ShaderResourceView* srv = nullptr;
    uint32_t binding = 0;
  };
  struct D3D11SamplerBinding
  {
    ID3D11SamplerState* sampler = nullptr;
    uint32_t binding = 0;
  };
  // A raw view of the bound range, `uav` where the shader may write it and
  // `srv` where it only reads.
  struct D3D11StorageBinding
  {
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    Microsoft::WRL::ComPtr<ID3D11UnorderedAccessView> uav;
    uint32_t binding = 0;
  };

  BindGroupD3D11() = default;
  ~BindGroupD3D11() override = default;

private:
  friend class ContextD3D11;
  friend class RenderPassD3D11;
  friend class ComputePassD3D11;
  std::vector<D3D11UBOBinding> m_d3d11UBOs;
  std::vector<D3D11TexBinding> m_d3d11Textures;
  std::vector<D3D11SamplerBinding> m_d3d11Samplers;
  std::vector<D3D11StorageBinding> m_d3d11Storage;
};

// A uniform buffer range as *SetConstantBuffers1 takes it, in 16 byte
// constants. NVIDIA drops the offset unless the count is a multiple of 16.
struct D3D11ConstantRange
{
  UINT first = 0;
  UINT count = 0;
};
inline D3D11ConstantRange d3d11ConstantRange(uint32_t offset, uint32_t size)
{
  const UINT constants = (size + 15) / 16;
  return {offset / 16, constants < 16 ? 16u : ((constants + 15u) & ~15u)};
}

// For clearing writable views, which Direct3D takes as an array.
inline ID3D11UnorderedAccessView* const kD3D11NoUAVs[D3D11_1_UAV_SLOT_COUNT] =
    {};
} // namespace rive::ore
