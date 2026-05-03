/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/refcnt.hpp"
#include "rive/renderer/ore/ore_types.hpp"

#if defined(ORE_BACKEND_METAL)
#import <Metal/Metal.h>
#endif
// Note: load_gles_extensions.hpp (glad) is intentionally NOT included here.
// GL object handles are GLuint = unsigned int; no glad needed in the header.
#if defined(ORE_BACKEND_D3D11)
#include <d3d11.h>
#include <wrl/client.h>
#endif
#if defined(ORE_BACKEND_D3D12)
#include <d3d12.h>
#include <wrl/client.h>
#endif
#if defined(ORE_BACKEND_WGPU)
#include <webgpu/webgpu_cpp.h>
#endif
#if defined(ORE_BACKEND_VK)
#include <vulkan/vulkan.h>
VK_DEFINE_HANDLE(VmaAllocator);
VK_DEFINE_HANDLE(VmaAllocation);
namespace rive::gpu
{
class RenderTargetVulkan;
}
#endif

namespace rive::ore
{

class Context;

class Texture : public RefCnt<Texture>
{
public:
    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }
    uint32_t depthOrArrayLayers() const { return m_depthOrArrayLayers; }
    TextureFormat format() const { return m_format; }
    TextureType type() const { return m_type; }
    uint32_t numMipmaps() const { return m_numMipmaps; }
    uint32_t sampleCount() const { return m_sampleCount; }
    bool isRenderTarget() const { return m_renderTarget; }

    void upload(const TextureDataDesc& data);

#if defined(ORE_BACKEND_METAL)
    id<MTLTexture> mtlTexture() const { return m_mtlTexture; }
#endif

private:
    friend class Context;
    friend class TextureView;
    friend class RenderPass;

    Texture(const TextureDesc& desc) :
        m_width(desc.width),
        m_height(desc.height),
        m_depthOrArrayLayers(desc.depthOrArrayLayers),
        m_format(desc.format),
        m_type(desc.type),
        m_renderTarget(desc.renderTarget),
        m_numMipmaps(desc.numMipmaps),
        m_sampleCount(desc.sampleCount)
    {}

    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_depthOrArrayLayers;
    TextureFormat m_format;
    TextureType m_type;
    bool m_renderTarget;
    uint32_t m_numMipmaps;
    uint32_t m_sampleCount;

#if defined(ORE_BACKEND_METAL)
    id<MTLTexture> m_mtlTexture = nil;
#endif
#if defined(ORE_BACKEND_GL)
    unsigned int m_glTexture = 0;
    unsigned int m_glRenderbuffer = 0; // Non-zero for MSAA render targets
                                       // (renderbuffer-backed, not texture).
    unsigned int m_glTarget = 0;
    bool m_glOwnsTexture = true; // False for borrowed textures (e.g.
                                 // wrapCanvasTexture).
#endif
#if defined(ORE_BACKEND_D3D11)
    Microsoft::WRL::ComPtr<ID3D11Resource> m_d3d11Texture;
    ID3D11DeviceContext* m_d3d11Context =
        nullptr; // Weak ref, set by Context::makeTexture.
#endif
#if defined(ORE_BACKEND_WGPU)
    wgpu::Texture m_wgpuTexture;
    wgpu::Queue
        m_wgpuQueue; // Weak ref (addref'd copy), set by Context::makeTexture.
#endif
#if defined(ORE_BACKEND_VK)
    VkImage m_vkImage = VK_NULL_HANDLE;
    VmaAllocation m_vmaAllocation = VK_NULL_HANDLE;
    VkImageLayout m_vkLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkDevice m_vkDevice = VK_NULL_HANDLE;         // Weak ref.
    VmaAllocator m_vmaAllocator = VK_NULL_HANDLE; // Weak ref.
    // Back-pointer to the owning Context. upload() reads the current frame
    // CB and VK dispatch table through it, and onRefCntReachedZero() routes
    // vmaDestroyImage through Context::vkDeferDestroy() in external-CB mode.
    Context* m_vkOreContext = nullptr;
#endif
#if defined(ORE_BACKEND_D3D12)
    Microsoft::WRL::ComPtr<ID3D12Resource> m_d3dTexture;
    D3D12_RESOURCE_STATES m_d3dCurrentState = D3D12_RESOURCE_STATE_COMMON;
    bool m_d3dIsExternal = false;        // true for wrapCanvasTexture.
    ID3D12Device* m_d3dDevice = nullptr; // Weak ref.
    // Set by Context::makeTexture so upload() can schedule copy commands.
    Context* m_d3dOreContext = nullptr; // Weak ref.
#endif

public:
    void onRefCntReachedZero() const;
};

class TextureView : public RefCnt<TextureView>
{
public:
    Texture* texture() const { return m_texture.get(); }
    TextureViewDimension dimension() const { return m_dimension; }
    TextureAspect aspect() const { return m_aspect; }
    uint32_t baseMipLevel() const { return m_baseMipLevel; }
    uint32_t mipCount() const { return m_mipCount; }
    uint32_t baseLayer() const { return m_baseLayer; }
    uint32_t layerCount() const { return m_layerCount; }

#if defined(ORE_BACKEND_METAL)
    id<MTLTexture> mtlTexture() const
    {
        return m_mtlTextureView ? m_mtlTextureView : m_texture->m_mtlTexture;
    }
#endif

private:
    friend class Context;
    friend class RenderPass;

    TextureView(rcp<Texture> texture, const TextureViewDesc& desc) :
        m_texture(std::move(texture)),
        m_dimension(desc.dimension),
        m_aspect(desc.aspect),
        m_baseMipLevel(desc.baseMipLevel),
        m_mipCount(desc.mipCount),
        m_baseLayer(desc.baseLayer),
        m_layerCount(desc.layerCount)
    {}

    rcp<Texture> m_texture;
    TextureViewDimension m_dimension;
    TextureAspect m_aspect;
    uint32_t m_baseMipLevel;
    uint32_t m_mipCount;
    uint32_t m_baseLayer;
    uint32_t m_layerCount;

#if defined(ORE_BACKEND_METAL)
    id<MTLTexture> m_mtlTextureView = nil;
#endif
#if defined(ORE_BACKEND_GL)
    unsigned int m_glTextureView = 0; // GL 4.3 view, or 0 (use base texture).
#endif
#if defined(ORE_BACKEND_D3D11)
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_d3dSRV;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_d3dRTV;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_d3dDSV;
#endif
#if defined(ORE_BACKEND_WGPU)
    wgpu::TextureView m_wgpuTextureView;
#endif
#if defined(ORE_BACKEND_VK)
    VkImageView m_vkImageView = VK_NULL_HANDLE;
    VkDevice m_vkDevice = VK_NULL_HANDLE; // Weak ref.
    PFN_vkDestroyImageView m_vkDestroyImageView = nullptr;
    // Set only for canvas-backed views (wrapCanvasTexture). Used by
    // RenderPass::finish() to update Rive's layout tracking.
    gpu::RenderTargetVulkan* m_vkRenderTarget = nullptr; // Weak ref.
    // Back-ref so onRefCntReachedZero() can route the destroy through
    // Context::vkDeferDestroy() in external-CB mode. Weak ref.
    Context* m_vkOreContext = nullptr;
#endif
#if defined(ORE_BACKEND_D3D12)
    D3D12_CPU_DESCRIPTOR_HANDLE m_d3dSrvHandle = {}; // {0} = invalid.
    D3D12_CPU_DESCRIPTOR_HANDLE m_d3dRtvHandle = {};
    D3D12_CPU_DESCRIPTOR_HANDLE m_d3dDsvHandle = {};
    // Back-ref so onRefCntReachedZero() can route destruction through
    // Context::d3dDeferDestroy() in external-CL mode. Weak ref.
    Context* m_d3dOreContext = nullptr;
#endif

public:
    void onRefCntReachedZero() const;
};

} // namespace rive::ore
