#pragma once
#include "rive/renderer/ore/ore_texture.hpp"
#include <webgpu/webgpu_cpp.h>

namespace rive::ore
{
class ContextWGPU;

class TextureWGPU : public LITE_RTTI_OVERRIDE(Texture, TextureWGPU)
{
public:
    TextureWGPU(const TextureDesc& desc) : lite_rtti_override(desc) {}
    ~TextureWGPU() override = default; // wgpu::Texture RAII destructor releases
    void upload(const TextureDataDesc& data) override;

private:
    friend class ContextWGPU;
    wgpu::Texture m_wgpuTexture;
    wgpu::Queue m_wgpuQueue; // Weak ref (addref'd copy).
};

class TextureViewWGPU : public LITE_RTTI_OVERRIDE(TextureView, TextureViewWGPU)
{
public:
    TextureViewWGPU(rcp<Texture> texture, const TextureViewDesc& desc) :
        lite_rtti_override(std::move(texture), desc)
    {}
    ~TextureViewWGPU() override =
        default; // wgpu::TextureView RAII destructor releases

private:
    friend class ContextWGPU;
    wgpu::TextureView m_wgpuTextureView;
};
} // namespace rive::ore
