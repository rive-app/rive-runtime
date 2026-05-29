#pragma once
#include "rive/renderer/ore/ore_render_pass.hpp"
#include "rive/renderer/ore/ore_context_d3d12.hpp"

#include <d3d12.h>
#include <wrl/client.h>

namespace rive::ore
{
class PipelineD3D12;
class RenderPassD3D12 : public RenderPass
{
public:
    void setPipeline(Pipeline* pipeline) override;

    void setVertexBuffer(uint32_t slot,
                         Buffer* buffer,
                         uint32_t offset = 0) override;
    void setIndexBuffer(Buffer* buffer,
                        IndexFormat format,
                        uint32_t offset = 0) override;

    // Bind a pre-created BindGroup, optionally overriding dynamic UBO offsets.
    // Dynamic-ness is declared on the pipeline (`PipelineDesc::dynamicUBOs`)
    // and cached per-slot on the BindGroup at creation.
    // dynamicOffsets: one uint32 per dynamic UBO in this group, in the order
    // they appear in `BindGroupDesc::ubos` (matches WebGPU semantics when
    // the caller lists UBOs in ascending-slot order, the typical pattern).
    void setBindGroup(uint32_t groupIndex,
                      BindGroup* bg,
                      const uint32_t* dynamicOffsets = nullptr,
                      uint32_t dynamicOffsetCount = 0) override;

    void setViewport(float x,
                     float y,
                     float width,
                     float height,
                     float minDepth = 0.0f,
                     float maxDepth = 1.0f) override;
    void setScissorRect(uint32_t x,
                        uint32_t y,
                        uint32_t width,
                        uint32_t height) override;

    void setStencilReference(uint32_t ref) override;
    void setBlendColor(float r, float g, float b, float a) override;

    void draw(uint32_t vertexCount,
              uint32_t instanceCount = 1,
              uint32_t firstVertex = 0,
              uint32_t firstInstance = 0) override;

    void drawIndexed(uint32_t indexCount,
                     uint32_t instanceCount = 1,
                     uint32_t firstIndex = 0,
                     int32_t baseVertex = 0,
                     uint32_t firstInstance = 0) override;

    void validate() const override;

    void finish() override;

    RenderPassD3D12(RenderPassD3D12&& other) noexcept;
    RenderPassD3D12& operator=(RenderPassD3D12&&) noexcept;
    RenderPassD3D12(ContextD3D12* context) : RenderPass(context) {}
    ~RenderPassD3D12() override;

private:
    // D3D12 setBindGroup body — shared between the D3D12-only translation
    // unit (ore_render_pass_d3d12.cpp) and the combined D3D11+D3D12 one
    // (ore_render_pass_d3d11_d3d12.cpp). Defined inline in
    // ore_d3d12_bind_group_apply.hpp, which both .cpp files include.
    inline void d3d12ApplyBindGroup(uint32_t groupIndex,
                                    BindGroupD3D12* bg,
                                    const uint32_t* dynamicOffsets,
                                    uint32_t dynamicOffsetCount);

    ID3D12GraphicsCommandList* m_d3dCmdList =
        nullptr;                          // Weak ref (owned by Context).
    ID3D12Device* m_d3dDevice = nullptr;  // Weak ref.
    ContextD3D12* m_d3dContext = nullptr; // Weak ref.
    // Strong ref — same Lua-GC reasoning as the cross-backend
    // `m_currentPipeline` above. The D3D12-only field exists because
    // D3D12's setPipeline path is split between this TU and the
    // d3d11+d3d12 combined TU; both share this storage.
    rcp<PipelineD3D12> m_d3dCurrentPipeline;
    // Root signature currently bound on the command list. Compared against
    // each new pipeline's `m_d3dRootSigOwned` in `setPipeline` so we only
    // call `SetGraphicsRootSignature` when the sig actually changes (each
    // Ore D3D12 pipeline owns its own per-pipeline root sig — RFC v5
    // §3.2.2). Weak ref; freed by the owning Pipeline.
    ID3D12RootSignature* m_d3dCurrentRootSig = nullptr;
    // Pending descriptor state (accumulated per setPipeline/setBindGroup).
    D3D12_GPU_VIRTUAL_ADDRESS m_d3dPendingUBOs[8] = {};
    D3D12_CPU_DESCRIPTOR_HANDLE m_d3dPendingSrvHandles[8] = {};
    D3D12_CPU_DESCRIPTOR_HANDLE m_d3dPendingSamplerHandles[8] = {};
    DXGI_FORMAT m_d3dIndexFormat = DXGI_FORMAT_R16_UINT;
    uint32_t m_d3dIndexOffset = 0;
    uint32_t m_d3dStencilRef = 0;
    float m_d3dBlendFactor[4] = {};
    // Color attachment resources for barrier in finish() (RT → final).
    // Parallel arrays: m_d3dColorTextures provides the Ore Texture back-ref so
    // finish() can keep Texture::m_d3dCurrentState in sync with the actual
    // resource state after the end-of-pass transition.
    ID3D12Resource* m_d3dColorResources[4] = {};
    TextureD3D12* m_d3dColorTextures[4] = {};
    D3D12_RESOURCE_STATES m_d3dColorFinalStates[4] = {};
    uint32_t m_d3dColorCount = 0;
    ID3D12Resource* m_d3dDepthResource = nullptr;
    TextureD3D12* m_d3dDepthTexture = nullptr;
    D3D12_RESOURCE_STATES m_d3dDepthFinalState = D3D12_RESOURCE_STATE_COMMON;
    // MSAA resolve pairs (populated at beginRenderPass, executed in finish).
    // D3D12 subresource index is mipLevel + arraySlice * mipCount for the
    // common single-plane case.
    struct D3D12ResolveEntry
    {
        ID3D12Resource* msaa = nullptr;
        ID3D12Resource* resolve = nullptr;
        // Back-ref to the resolve target's Ore Texture so finish() can update
        // Texture::m_d3dCurrentState after the resolve barrier chain.
        TextureD3D12* resolveTex = nullptr;
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
        uint32_t msaaSubresource = 0;
        uint32_t resolveSubresource = 0;
        // Resolve target's prior state, to restore after resolve.
        D3D12_RESOURCE_STATES resolvePriorState = D3D12_RESOURCE_STATE_COMMON;
        // Resolve target's final state (what finish() should leave it in).
        D3D12_RESOURCE_STATES resolveFinalState =
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    };
    D3D12ResolveEntry m_d3dResolves[4] = {};
    uint32_t m_d3dResolveCount = 0;
    friend class ContextD3D12;
};

} // namespace rive::ore