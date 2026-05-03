/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/refcnt.hpp"
#include "rive/renderer/ore/ore_types.hpp"
#include "rive/renderer/ore/ore_bind_group_layout.hpp"
#include "rive/renderer/ore/ore_shader_module.hpp"

#include <vector>

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

class Pipeline : public RefCnt<Pipeline>
{
public:
    const PipelineDesc& desc() const { return m_desc; }

    // (group, binding) → per-backend native slot map for this pipeline's
    // resources. Copied from the vertex / fragment ShaderModule at
    // construction. Consumed by each flatten-backend's `makeBindGroup`
    // to translate a `BindGroupDesc::*Entry::slot` (= WGSL `@binding`)
    // into the backend's native slot (Metal buffer index, HLSL register,
    // GL binding point, D3D12 `BaseShaderRegister`).
    BindingMap m_bindingMap;

    // Layouts the pipeline was created with — one per `@group(N)`. Keeps
    // them alive (the per-backend native handles inside the layouts are
    // referenced by the pipeline's compiled state). Used by `setBindGroup`
    // to verify `bg->layout() == m_layouts[g]` (pointer-equality check
    // matching WebGPU's exact-layout requirement).
    rcp<BindGroupLayout> m_layouts[kMaxBindGroups];

private:
    friend class Context;
    friend class RenderPass;

    Pipeline(const PipelineDesc& desc) : m_desc(desc)
    {
        // Propagate the binding map from the VS module (or FS if VS is
        // absent — e.g. blit-only pipelines). The two modules are
        // compiled from a single WGSL source so their maps agree, and
        // every module is required to carry a populated map (sidecar
        // mandatory, RFC §14.4).
        if (desc.vertexModule != nullptr)
        {
            m_bindingMap = desc.vertexModule->m_bindingMap;
        }
        else if (desc.fragmentModule != nullptr)
        {
            m_bindingMap = desc.fragmentModule->m_bindingMap;
        }

        // Stash the user-supplied layouts. Backends overwrite NULL
        // entries (groups the shader doesn't bind) with a no-op
        // BindGroupLayout if needed for empty-set semantics.
        const uint32_t count = std::min(desc.bindGroupLayoutCount,
                                        static_cast<uint32_t>(kMaxBindGroups));
        for (uint32_t i = 0; i < count; ++i)
        {
            m_layouts[i] = ref_rcp(desc.bindGroupLayouts[i]);
        }
    }

    PipelineDesc m_desc;

#if defined(ORE_BACKEND_METAL)
    id<MTLRenderPipelineState> m_mtlPipeline = nil;
    id<MTLDepthStencilState> m_mtlDepthStencil = nil;
#endif
#if defined(ORE_BACKEND_GL)
    unsigned int m_glProgram = 0;
#endif
#if defined(ORE_BACKEND_D3D11)
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_d3dInputLayout;
    Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_d3dRasterizer;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_d3dDepthStencil;
    Microsoft::WRL::ComPtr<ID3D11BlendState> m_d3dBlend;
    ID3D11VertexShader* m_d3dVS = nullptr;
    ID3D11PixelShader* m_d3dPS = nullptr;
    // Strong refs to keep ShaderModules alive for the lifetime of the
    // Pipeline. Without these, Luau GC can destroy the ScriptedShader
    // (and its ShaderModules) while the Pipeline still references them,
    // causing use-after-free crashes in the D3D11 driver.
    rcp<ShaderModule> m_vsModule;
    rcp<ShaderModule> m_psModule;
#endif
#if defined(ORE_BACKEND_WGPU)
    wgpu::Device m_wgpuDevice; // Needed to create bind groups at draw time.
    wgpu::RenderPipeline m_wgpuPipeline;
    wgpu::PipelineLayout m_wgpuPipelineLayout;
#endif
#if defined(ORE_BACKEND_VK)
    VkDevice m_vkDevice = VK_NULL_HANDLE;
    VkPipelineLayout m_vkPipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_vkPipeline = VK_NULL_HANDLE;
    VkPrimitiveTopology m_vkTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    // Function pointers for cleanup (loaded by Context::makePipeline).
    PFN_vkDestroyPipeline m_vkDestroyPipeline = nullptr;
    PFN_vkDestroyPipelineLayout m_vkDestroyPipelineLayout = nullptr;
    // Back-ref so onRefCntReachedZero() can route the actual vkDestroy* calls
    // through Context::vkDeferDestroy() when an external command buffer is
    // active (the host hasn't submitted yet). Weak ref.
    Context* m_vkOreContext = nullptr;
#endif
#if defined(ORE_BACKEND_D3D12)
    Microsoft::WRL::ComPtr<ID3D12PipelineState> m_d3dPSO;
    // Per-pipeline root signature composed from per-group descriptor-table
    // shapes carried by each `m_layouts[g]->m_d3dGroupParams`. The root
    // sig itself is per-pipeline (cross-group) but consumes the layouts'
    // per-group params at build time.
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_d3dRootSigOwned;
    // Per-group root-parameter indices (set at root sig build time) +
    // per-kind register ranges (mirrored from each layout's
    // m_d3dGroupParams). Consumed at setBindGroup time to dispatch
    // descriptor writes into the right root parameter.
    D3D12GroupRootParams m_d3dGroupParams[kMaxBindGroups]{};
    D3D12_PRIMITIVE_TOPOLOGY m_d3dTopology =
        D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    UINT m_d3dVertexStrides[8] = {}; // Per-slot vertex strides.
    // Back-ref so onRefCntReachedZero() can route the ComPtr release through
    // Context::d3dDeferDestroy() when an external command list is active.
    // Weak ref.
    Context* m_d3dOreContext = nullptr;
#endif

public:
    void onRefCntReachedZero() const;
};

} // namespace rive::ore
