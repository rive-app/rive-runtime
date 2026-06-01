#pragma once
#include "rive/renderer/ore/ore_texture.hpp"
#include <d3d12.h>
#include <wrl/client.h>

namespace rive::ore
{
class ContextD3D12;
class BufferD3D12;

class TextureD3D12 : public LITE_RTTI_OVERRIDE(Texture, TextureD3D12)
{
public:
    TextureD3D12(rcp<rive::gpu::GPUResourceManager> manager,
                 const TextureDesc& desc) :
        lite_rtti_override(std::move(manager), desc)
    {}
    ~TextureD3D12() override = default; // ComPtr released automatically
    void upload(const TextureDataDesc& data) override;

private:
    friend class ContextD3D12;
    friend class RenderPassD3D12;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_d3dTexture;
    D3D12_RESOURCE_STATES m_d3dCurrentState = D3D12_RESOURCE_STATE_COMMON;
    bool m_d3dIsExternal = false;        // true for wrapCanvasTexture.
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
    TextureViewD3D12(rcp<rive::gpu::GPUResourceManager> manager,
                     rcp<Texture> texture,
                     const TextureViewDesc& desc) :
        lite_rtti_override(std::move(manager), std::move(texture), desc)
    {}
    ~TextureViewD3D12() override = default;

private:
    friend class ContextD3D12;
    D3D12_CPU_DESCRIPTOR_HANDLE m_d3dSrvHandle = {};
    D3D12_CPU_DESCRIPTOR_HANDLE m_d3dRtvHandle = {};
    D3D12_CPU_DESCRIPTOR_HANDLE m_d3dDsvHandle = {};
};
} // namespace rive::ore
