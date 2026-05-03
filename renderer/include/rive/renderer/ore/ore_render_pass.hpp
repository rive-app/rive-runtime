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
namespace rive::gpu
{
class RenderTargetVulkan;
}
#endif

namespace rive::ore
{

class Buffer;
class TextureView;
class Sampler;
class Pipeline;
class BindGroup;
class Context;

class RenderPass
{
public:
    void setPipeline(Pipeline* pipeline);

    void setVertexBuffer(uint32_t slot, Buffer* buffer, uint32_t offset = 0);
    void setIndexBuffer(Buffer* buffer,
                        IndexFormat format,
                        uint32_t offset = 0);

    // Bind a pre-created BindGroup, optionally overriding dynamic UBO offsets.
    // Dynamic-ness is declared on the pipeline (`PipelineDesc::dynamicUBOs`)
    // and cached per-slot on the BindGroup at creation.
    // dynamicOffsets: one uint32 per dynamic UBO in this group, in the order
    // they appear in `BindGroupDesc::ubos` (matches WebGPU semantics when
    // the caller lists UBOs in ascending-slot order, the typical pattern).
    void setBindGroup(uint32_t groupIndex,
                      BindGroup* bg,
                      const uint32_t* dynamicOffsets = nullptr,
                      uint32_t dynamicOffsetCount = 0);

    void setViewport(float x,
                     float y,
                     float width,
                     float height,
                     float minDepth = 0.0f,
                     float maxDepth = 1.0f);
    void setScissorRect(uint32_t x,
                        uint32_t y,
                        uint32_t width,
                        uint32_t height);

    void setStencilReference(uint32_t ref);
    void setBlendColor(float r, float g, float b, float a);

    void draw(uint32_t vertexCount,
              uint32_t instanceCount = 1,
              uint32_t firstVertex = 0,
              uint32_t firstInstance = 0);

    void drawIndexed(uint32_t indexCount,
                     uint32_t instanceCount = 1,
                     uint32_t firstIndex = 0,
                     int32_t baseVertex = 0,
                     uint32_t firstInstance = 0);

    void finish();
    bool isFinished() const { return m_finished; }

    ~RenderPass();

    RenderPass(RenderPass&& other) noexcept;
    RenderPass& operator=(RenderPass&&) noexcept;
    RenderPass(const RenderPass&) = delete;
    RenderPass& operator=(const RenderPass&) = delete;

private:
    friend class Context;

    RenderPass() = default;

    bool m_finished = false;

    // Attachment metadata — populated by beginRenderPass() from the
    // RenderPassDesc via populateAttachmentMetadata(). Used by setPipeline()
    // for format/sampleCount validation (matches WebGPU spec: validation at
    // setPipeline time).
    TextureFormat m_colorFormats[4] = {};
    uint32_t m_colorCount = 0;
    TextureFormat m_depthFormat = {};
    bool m_hasDepth = false;
    uint32_t m_sampleCount = 1;

    // Cross-backend back-pointer used by the pipeline-compat validator to
    // route errors through Context::setLastError(). Populated by every
    // backend's beginRenderPass() alongside the per-backend context fields
    // (m_vkContext, m_wgpuContext, ...). Weak ref.
    Context* m_context = nullptr;

    // Lifecycle: strong refs to bound BindGroups prevent Lua GC from freeing
    // them between setBindGroup() and finish(). Released in finish().
    rcp<BindGroup> m_boundGroups[kMaxBindGroups];

    void validate() const;

    // Populate attachment metadata from the descriptor. Called by every
    // backend's beginRenderPass() immediately after constructing the
    // RenderPass; defined inline because it only reads cross-backend
    // texture/view accessors that are uniform across backends.
    inline void populateAttachmentMetadata(const RenderPassDesc& desc);

    // WebGPU-spec pipeline/attachment compatibility check, invoked from
    // every backend's setPipeline(). Compares the pipeline's declared
    // color/depth formats and sampleCount against the attachments this
    // pass was begun with. Returns true on match; on mismatch populates
    // Context::lastError() and returns false.
    inline bool checkPipelineCompat(const Pipeline* pipeline) const;

    // Move cross-backend fields (m_context, attachment metadata,
    // m_boundGroups) from `other` into `*this` and clear them on `other`.
    // Called from every backend's move ctor / move-assign so that new
    // cross-backend fields only need to be wired once.
    inline void moveCrossBackendFieldsFrom(RenderPass& other)
    {
        m_finished = other.m_finished;
        for (uint32_t i = 0; i < 4; ++i)
            m_colorFormats[i] = other.m_colorFormats[i];
        m_colorCount = other.m_colorCount;
        m_depthFormat = other.m_depthFormat;
        m_hasDepth = other.m_hasDepth;
        m_sampleCount = other.m_sampleCount;
        m_context = other.m_context;
        for (uint32_t i = 0; i < kMaxBindGroups; ++i)
            m_boundGroups[i] = std::move(other.m_boundGroups[i]);
        other.m_finished = true;
        other.m_context = nullptr;
    }

    // Shared across all non-Metal backends that track the current
    // pipeline. Strong ref (rcp) — same lifetime guarantee as
    // `m_boundGroups[]`. The Lua bridge can GC its `Pipeline` userdata
    // between `setPipeline` and `finish`, so a raw pointer would
    // dangle by the next `draw()`. The rcp keeps the Pipeline alive
    // until `finish()` clears the field.
#if defined(ORE_BACKEND_GL) || defined(ORE_BACKEND_D3D11) ||                   \
    defined(ORE_BACKEND_WGPU) || defined(ORE_BACKEND_VK)
    rcp<Pipeline> m_currentPipeline;
#endif

    // GL keeps an explicit stencil-reference cache because
    // `glStencilFuncSeparate` bundles compare + ref + readMask into a
    // single call — re-applying compare from the new pipeline at
    // `setPipeline` time would otherwise drop any reference value the
    // caller set earlier on the pass. WebGPU semantics are pass-state:
    // `setStencilReference` persists across pipeline changes.
    // Other backends either keep stencil ref on the encoder/cmd-buf
    // (Metal, Vulkan dynamic state, D3D12 OMSetStencilRef) or rebind it
    // via the API directly without losing the value.
#if defined(ORE_BACKEND_GL)
    uint32_t m_glStencilRef = 0;
#endif

#if defined(ORE_BACKEND_METAL)
    // Metal implementation helpers — shared by metal-only and metal+gl builds.
    // Defined inline in ore_render_pass_metal.mm.
    inline void mtlSetPipeline(Pipeline* pipeline);
    inline void mtlSetVertexBuffer(uint32_t slot,
                                   Buffer* buffer,
                                   uint32_t offset);
    inline void mtlSetIndexBuffer(Buffer* buffer,
                                  IndexFormat format,
                                  uint32_t offset);
    inline void mtlSetBindGroup(BindGroup* bg,
                                uint32_t groupIndex,
                                const uint32_t* dynamicOffsets,
                                uint32_t dynamicOffsetCount);
    inline void mtlSetViewport(float x,
                               float y,
                               float width,
                               float height,
                               float minDepth,
                               float maxDepth);
    inline void mtlSetScissorRect(uint32_t x,
                                  uint32_t y,
                                  uint32_t width,
                                  uint32_t height);
    inline void mtlSetStencilRef(uint32_t ref);
    inline void mtlSetBlendColor(float r, float g, float b, float a);
    inline void mtlDraw(uint32_t vertexCount,
                        uint32_t instanceCount,
                        uint32_t firstVertex,
                        uint32_t firstInstance);
    inline void mtlDrawIndexed(uint32_t indexCount,
                               uint32_t instanceCount,
                               uint32_t firstIndex,
                               int32_t baseVertex,
                               uint32_t firstInstance);
    inline void mtlFinish();

    id<MTLRenderCommandEncoder> m_mtlEncoder = nil;
    id<MTLCommandBuffer> m_mtlCommandBuffer = nil;
    id<MTLBuffer> m_mtlIndexBuffer = nil;
    MTLIndexType m_mtlIndexType = MTLIndexTypeUInt16;
    uint32_t m_mtlIndexBufferOffset = 0;
    MTLPrimitiveType m_mtlPrimitiveType = MTLPrimitiveTypeTriangle;
#endif
#if defined(ORE_BACKEND_GL)
    unsigned int m_glFBO = 0;
    unsigned int m_glVAO = 0;
    unsigned int m_prevVAO = 0; // Saved host VAO to restore in finish().
    unsigned int m_prevFBO = 0; // Saved host FBO to restore in finish().
    bool m_ownsFBO = false;
    bool m_ownsVAO = false;
    // (Cross-backend m_context lives outside this guard — see line ~117.)
    uint32_t m_viewportWidth = 0;
    uint32_t m_viewportHeight = 0;
    uint32_t m_maxSamplerSlot = 0; // Track highest sampler slot for cleanup.
    uint32_t m_maxAttribSlot = 0;  // Track highest vertex attrib for cleanup.
    bool m_usedSamplers = false;
    bool m_usedAttribs = false;
    IndexFormat m_glIndexFormat = IndexFormat::uint16;

    // MSAA resolve targets (populated by beginRenderPass when
    // ColorAttachment::resolveTarget is set).
    struct GLResolveInfo
    {
        uint32_t colorIndex = 0;
        unsigned int resolveTex = 0;
        unsigned int resolveTarget = 0; // GL target (e.g. GL_TEXTURE_2D).
        uint32_t width = 0;
        uint32_t height = 0;
    };
    GLResolveInfo m_glResolves[4] = {};
    uint32_t m_glResolveCount = 0;
#endif
#if defined(ORE_BACKEND_D3D11)
    ID3D11DeviceContext* m_d3d11Context = nullptr;
    D3D11_PRIMITIVE_TOPOLOGY m_d3d11Topology =
        D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    DXGI_FORMAT m_d3d11IndexFormat = DXGI_FORMAT_R16_UINT;
    uint32_t m_d3d11IndexOffset = 0;
    uint32_t m_d3d11StencilRef = 0;
    float m_d3d11BlendFactor[4] = {};
    // MSAA resolve: pairs of (msaaTexture, resolveTexture) + format +
    // per-view subresource indices. D3D11 subresource index is
    // mipLevel + (arraySlice * mipCount), so recording both endpoints
    // at pass-begin time (where the view desc is accessible) means
    // finish() just passes them through to ResolveSubresource.
    struct D3D11ResolveEntry
    {
        ID3D11Resource* msaa = nullptr;
        ID3D11Resource* resolve = nullptr;
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
        uint32_t msaaSubresource = 0;
        uint32_t resolveSubresource = 0;
    };
    D3D11ResolveEntry m_d3d11Resolves[4];
    uint32_t m_d3d11ResolveCount = 0;
#endif
#if defined(ORE_BACKEND_WGPU)
    Context* m_wgpuContext = nullptr;
    wgpu::RenderPassEncoder m_wgpuPassEncoder;
    wgpu::Buffer m_wgpuIndexBuffer;
    wgpu::IndexFormat m_wgpuIndexFormat = wgpu::IndexFormat::Uint16;
    uint64_t m_wgpuIndexOffset = 0;
#endif
#if defined(ORE_BACKEND_VK)
    Context* m_vkContext = nullptr;
    VkCommandBuffer m_vkCmdBuf = VK_NULL_HANDLE; // Weak ref (owned by Context).
    VkFramebuffer m_vkFramebuffer =
        VK_NULL_HANDLE; // Created per-pass, destroyed in finish().
    VkBuffer m_vkIndexBuffer = VK_NULL_HANDLE;
    VkIndexType m_vkIndexType = VK_INDEX_TYPE_UINT16;
    VkDeviceSize m_vkIndexOffset = 0;
    // Color attachment images for the post-render layout transition
    // (COLOR_ATTACHMENT_OPTIMAL → SHADER_READ_ONLY_OPTIMAL).
    VkImage m_vkColorImages[4] = {};
    uint32_t m_vkColorBaseLayer[4] = {};
    uint32_t m_vkColorLayerCount[4] = {};
    uint32_t m_vkColorCount = 0;
    // Weak back-refs to Rive's render targets so finish() can update their
    // lastAccess tracking after the layout transition.
    gpu::RenderTargetVulkan* m_vkColorRenderTargets[4] = {};
    // Depth attachment image for the post-render barrier
    // (DEPTH_STENCIL_ATTACHMENT_OPTIMAL → SHADER_READ_ONLY_OPTIMAL).
    VkImage m_vkDepthImage = VK_NULL_HANDLE;
    uint32_t m_vkDepthBaseLayer = 0;
    uint32_t m_vkDepthLayerCount = 0;
#endif
#if defined(ORE_BACKEND_D3D12)
    // D3D12 setBindGroup body — shared between the D3D12-only translation
    // unit (ore_render_pass_d3d12.cpp) and the combined D3D11+D3D12 one
    // (ore_render_pass_d3d11_d3d12.cpp). Defined inline in
    // ore_d3d12_bind_group_apply.hpp, which both .cpp files include.
    inline void d3d12ApplyBindGroup(uint32_t groupIndex,
                                    BindGroup* bg,
                                    const uint32_t* dynamicOffsets,
                                    uint32_t dynamicOffsetCount);

    ID3D12GraphicsCommandList* m_d3dCmdList =
        nullptr;                         // Weak ref (owned by Context).
    ID3D12Device* m_d3dDevice = nullptr; // Weak ref.
    Context* m_d3dContext = nullptr;     // Weak ref.
    // Strong ref — same Lua-GC reasoning as the cross-backend
    // `m_currentPipeline` above. The D3D12-only field exists because
    // D3D12's setPipeline path is split between this TU and the
    // d3d11+d3d12 combined TU; both share this storage.
    rcp<Pipeline> m_d3dCurrentPipeline;
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
    Texture* m_d3dColorTextures[4] = {};
    D3D12_RESOURCE_STATES m_d3dColorFinalStates[4] = {};
    uint32_t m_d3dColorCount = 0;
    ID3D12Resource* m_d3dDepthResource = nullptr;
    Texture* m_d3dDepthTexture = nullptr;
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
        Texture* resolveTex = nullptr;
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
#endif
};

} // namespace rive::ore
