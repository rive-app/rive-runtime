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
#endif

namespace rive::ore
{

class Context;
#if defined(ORE_BACKEND_VK)
class ContextVulkan;
#endif
#if defined(ORE_BACKEND_D3D12)
class ContextD3D12;
#endif

class Sampler : public RefCnt<Sampler>
{
private:
    friend class Context;
#if defined(ORE_BACKEND_METAL)
    friend class ContextMetal;
#endif
#if defined(ORE_BACKEND_GL)
    friend class ContextGL;
#endif
#if defined(ORE_BACKEND_D3D11)
    friend class ContextD3D11;
#endif
#if defined(ORE_BACKEND_D3D12)
    friend class ContextD3D12;
#endif
#if defined(ORE_BACKEND_WGPU)
    friend class ContextWGPU;
#endif
#if defined(ORE_BACKEND_VK)
    friend class ContextVulkan;
#endif
    friend class RenderPass;

    Sampler() = default;

#if defined(ORE_BACKEND_METAL)
    id<MTLSamplerState> m_mtlSampler = nil;
#endif
#if defined(ORE_BACKEND_GL)
    unsigned int m_glSampler = 0;
#endif
#if defined(ORE_BACKEND_D3D11)
    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_d3dSampler;
#endif
#if defined(ORE_BACKEND_WGPU)
    wgpu::Sampler m_wgpuSampler;
#endif
#if defined(ORE_BACKEND_VK)
    VkSampler m_vkSampler = VK_NULL_HANDLE;
    VkDevice m_vkDevice = VK_NULL_HANDLE; // Weak ref.
    PFN_vkDestroySampler m_vkDestroySampler = nullptr;
    // Back-ref so onRefCntReachedZero() can route destruction through
    // ContextVulkan::vkDeferDestroy() in external-CB mode. Weak ref.
    ContextVulkan* m_vkOreContext = nullptr;
#endif
#if defined(ORE_BACKEND_D3D12)
    D3D12_CPU_DESCRIPTOR_HANDLE m_d3dSamplerHandle = {}; // {0} = invalid.
    // Back-ref so onRefCntReachedZero() can route destruction through
    // ContextD3D12::d3dDeferDestroy() in external-CL mode. Weak ref.
    ContextD3D12* m_d3dOreContext = nullptr;
#endif

public:
    void onRefCntReachedZero() const;
};

} // namespace rive::ore
