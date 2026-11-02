#pragma once
#include "rive/renderer/ore/ore_texture.hpp"
#include "rive/renderer/ore/ore_context_d3d12.hpp"
#include <d3d12.h>
#include <wrl/client.h>

namespace rive::ore
{
class ContextD3D12;
class BufferD3D12;

// The mip and layers a pass attaches.
struct D3D12AttachedRange
{
  uint32_t mip = 0;
  uint32_t baseLayer = 0;
  uint32_t layerCount = 1;
};

// Covers fragment and vertex stage samples, since a bind group does not
// know which stages read it.
constexpr D3D12_RESOURCE_STATES
    kD3D12ShaderResourceStates = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE |
                                 D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

class TextureD3D12 : public LITE_RTTI_OVERRIDE(Texture, TextureD3D12)
{
public:
  TextureD3D12(
      rcp<rive::gpu::GPUResourceManager> manager,
      const TextureDesc& desc)
      : lite_rtti_override(std::move(manager), desc)
  {}
  ~TextureD3D12() override = default; // ComPtr released automatically
  void uploadImpl(const TextureDataDesc& data) override;

private:
  friend class ContextD3D12;
  friend class RenderPassD3D12;

  uint32_t d3dSubresource(uint32_t mip, uint32_t layer) const
  {
    return mip + layer * numMipmaps();
  }
  D3D12AttachedRange d3dAttachedRange(const TextureView* view) const
  {
    // A 3D view's layers are depth slices within one subresource.
    if (m_d3dArraySize == 1)
      return {view->baseMipLevel(), 0, 1};
    return {view->baseMipLevel(), view->baseLayer(), view->layerCount()};
  }
  // Moves only the attached range on every plane, so a pass can render one
  // mip while sampling another.
  void d3dTransition(
      ID3D12GraphicsCommandList* cmdList,
      const D3D12AttachedRange& range,
      D3D12_RESOURCE_STATES before,
      D3D12_RESOURCE_STATES after) const;

  Microsoft::WRL::ComPtr<ID3D12Resource> m_d3dTexture;
  // Every subresource rests here outside a pass; a pass moves only what it
  // attaches and puts it back at finish.
  D3D12_RESOURCE_STATES m_d3dCurrentState = D3D12_RESOURCE_STATE_COMMON;
  // Two for depth stencil formats, whose stencil is its own plane.
  uint32_t m_d3dPlaneCount = 1;
  // The resource's array size, which a 2D texture with layers can exceed
  // arrayLayers() by.
  uint32_t m_d3dArraySize = 1;
  bool m_d3dIsExternal = false; // true for wrapCanvasTexture.
  // The state its owner expects an external texture back in after a pass.
  D3D12_RESOURCE_STATES m_d3dExternalState = D3D12_RESOURCE_STATE_COMMON;
  ID3D12Device* m_d3dDevice = nullptr; // Weak ref.
  // ContextD3D12. Weak ref. actually used for barriers
  ContextD3D12* m_d3dOreContext = nullptr;
  // Upload buffers. Gets destroyed with safe frame number
  rcp<BufferD3D12> m_uploadBuffer;
};

class TextureViewD3D12
    : public LITE_RTTI_OVERRIDE(TextureView, TextureViewD3D12)
{
public:
  TextureViewD3D12(
      rcp<rive::gpu::GPUResourceManager> manager,
      rcp<Texture> texture,
      const TextureViewDesc& desc)
      : lite_rtti_override(std::move(manager), std::move(texture), desc)
  {}
  ~TextureViewD3D12() override = default;

private:
  friend class ContextD3D12;
  D3D12_CPU_DESCRIPTOR_HANDLE m_d3dSrvHandle = {};
  D3D12_CPU_DESCRIPTOR_HANDLE m_d3dRtvHandle = {};
  D3D12_CPU_DESCRIPTOR_HANDLE m_d3dDsvHandle = {};
  // The slots behind the handles, given back when the view goes. Empty
  // for the shared host target RTV.
  D3D12DescriptorSlot m_d3dSrvSlot;
  D3D12DescriptorSlot m_d3dRtvSlot;
  D3D12DescriptorSlot m_d3dDsvSlot;
};
} // namespace rive::ore
