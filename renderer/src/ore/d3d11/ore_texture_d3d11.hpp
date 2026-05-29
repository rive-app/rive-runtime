#pragma once
#include "rive/renderer/ore/ore_texture.hpp"
#include <d3d11.h>
#include <wrl/client.h>

namespace rive::ore
{
class ContextD3D11;

class TextureD3D11 : public LITE_RTTI_OVERRIDE(Texture, TextureD3D11)
{
public:
    TextureD3D11(const TextureDesc& desc) : lite_rtti_override(desc) {}
    ~TextureD3D11() override = default; // ComPtr released automatically
    void upload(const TextureDataDesc& data) override;

private:
    friend class ContextD3D11;
    // ID3D11Resource accommodates both ID3D11Texture2D and ID3D11Texture3D.
    Microsoft::WRL::ComPtr<ID3D11Resource> m_d3d11Texture;
    ID3D11DeviceContext* m_d3d11Context = nullptr; // Weak ref.
};

class TextureViewD3D11
    : public LITE_RTTI_OVERRIDE(TextureView, TextureViewD3D11)
{
public:
    TextureViewD3D11(rcp<Texture> texture, const TextureViewDesc& desc) :
        lite_rtti_override(std::move(texture), desc)
    {}
    ~TextureViewD3D11() override = default; // ComPtrs released automatically

private:
    friend class ContextD3D11;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_d3dSRV;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_d3dRTV;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_d3dDSV;
};
} // namespace rive::ore
