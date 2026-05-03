/*
 * Copyright 2025 Rive
 */

#pragma once

#include <cstdarg>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>
#include "rive/refcnt.hpp"
#include "rive/renderer/ore/ore_types.hpp"
#include "rive/renderer/ore/ore_buffer.hpp"
#include "rive/renderer/ore/ore_texture.hpp"
#include "rive/renderer/ore/ore_sampler.hpp"
#include "rive/renderer/ore/ore_shader_module.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/renderer/ore/ore_bind_group.hpp"
#include "rive/renderer/ore/ore_render_pass.hpp"

#if defined(ORE_BACKEND_METAL)
#import <Metal/Metal.h>
#endif
// Note: load_gles_extensions.hpp (glad) is intentionally NOT included here.
// The private GL state only needs 'int' (GLint is always int), keeping this
// header free of glad so it can be included without glad in the search path.
#if defined(ORE_BACKEND_D3D11)
#include <d3d11.h>
#include <d3d11_1.h> // ID3D11DeviceContext1 for offset-range CB binds
#include <wrl/client.h>
#endif
#if defined(ORE_BACKEND_D3D12)
#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#endif
#if defined(ORE_BACKEND_WGPU)
#include <webgpu/webgpu_cpp.h>
#ifndef RIVE_DAWN
#include <webgpu/webgpu_wagyu.h>
#endif
#endif

namespace rive::gpu
{
class RenderCanvas;
class Texture;
} // namespace rive::gpu

namespace rive::ore
{

class Context
{
public:
    rcp<Buffer> makeBuffer(const BufferDesc& desc);
    rcp<Texture> makeTexture(const TextureDesc& desc);
    rcp<TextureView> makeTextureView(const TextureViewDesc& desc);
    rcp<Sampler> makeSampler(const SamplerDesc& desc);
    rcp<ShaderModule> makeShaderModule(const ShaderModuleDesc& desc);
    rcp<BindGroupLayout> makeBindGroupLayout(const BindGroupLayoutDesc& desc);
    rcp<Pipeline> makePipeline(const PipelineDesc& desc,
                               std::string* outError = nullptr);
    rcp<BindGroup> makeBindGroup(const BindGroupDesc& desc);

    RenderPass beginRenderPass(const RenderPassDesc& desc,
                               std::string* outError = nullptr);

    const Features& features() const { return m_features; }

    // Defer destruction of a BindGroup until after the GPU fence in endFrame().
    // Call this instead of dropping the last rcp<> directly — keeps the object
    // alive until the GPU is done with any command buffers that reference it.
    void deferBindGroupDestroy(rcp<BindGroup> bg)
    {
        m_deferredBindGroups.push_back(std::move(bg));
    }

    // Last validation error — set by setPipeline() / setBindGroup() when
    // format or layout checks fail. Cleared at beginFrame(). The Lua layer
    // reads this after native calls and fires luaL_error() if non-empty.
    // Active render pass tracking — used by Lua bindings to auto-finish
    // stale passes and by backends that enforce one-encoder-at-a-time.
    RenderPass* activeRenderPass() const { return m_activeRenderPass; }
    void setActiveRenderPass(RenderPass* pass) { m_activeRenderPass = pass; }

    // Called at the top of every backend's beginRenderPass(). If a prior pass
    // is still open, finish it — matches the Lua binding's auto-finish
    // contract and means backends that enforce one-encoder-at-a-time (Metal,
    // D3D12) won't assert when a second beginRenderPass happens within the
    // same command buffer. Does not clear m_activeRenderPass, because the
    // pointer identity is owned by the Lua wrapper that called setActive…().
    inline void finishActiveRenderPass()
    {
        if (m_activeRenderPass && !m_activeRenderPass->isFinished())
        {
            m_activeRenderPass->finish();
        }
    }

    const std::string& lastError() const { return m_lastError; }
    void clearLastError() { m_lastError.clear(); }

    // Populate m_lastError with a printf-style message. Used by factory
    // methods to report construction failures to the Lua layer in lieu of
    // fprintf(stderr) / assert — which either spam the console or abort
    // the process.
    void setLastError(const char* fmt, ...)
#if defined(__GNUC__) || defined(__clang__)
        __attribute__((format(printf, 2, 3)))
#endif
    {
        va_list args;
        va_start(args, fmt);
        char buf[1024];
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);
        m_lastError = buf;
    }

    void beginFrame();
#if defined(ORE_BACKEND_VK)
    // External-CB mode: Ore records into the host's VkCommandBuffer (already
    // begun by the caller) instead of owning its own CB. The host retains
    // ownership of BeginCommandBuffer/EndCommandBuffer/QueueSubmit and the
    // frame fence. Contract: by the time beginFrame(externalCb) is called
    // again for a new frame, the host must have waited on the prior frame's
    // completion fence so deferred-destroy lists can be drained safely.
    // Enables cross-engine read-after-write (e.g. Rive renders into a canvas
    // then Ore samples it) that the owned-CB model cannot support, because
    // all recordings land in a single submission.
    void beginFrame(VkCommandBuffer externalCb);

    // Called by Ore resource onRefCntReachedZero() implementations. If the
    // context is currently in external-CB mode, the closure is queued to
    // run after the host has submitted and waited on the current CB. In
    // owned-CB mode the closure runs immediately.
    void vkDeferDestroy(std::function<void()> destroy);
#endif
#if defined(ORE_BACKEND_D3D12)
    // External-CL mode: Ore records into the host's
    // ID3D12GraphicsCommandList (already reset and open) instead of owning
    // its own. The host retains ownership of Close/ExecuteCommandLists/the
    // frame fence. Contract: the host must have waited on the prior frame's
    // completion fence before calling beginFrame(externalCl) again, so
    // deferred-destroy closures can be drained safely. Enables cross-engine
    // read-after-write (Rive writes, Ore reads) that the owned-CL model
    // cannot support.
    void beginFrame(ID3D12GraphicsCommandList* externalCl);

    // Called by Ore resource onRefCntReachedZero() implementations. If the
    // context is in external-CL mode, the closure is queued and drained at
    // the next beginFrame(), after the host has waited on the prior frame
    // fence. In owned-CL mode the closure runs immediately — Ore's endFrame()
    // does its own ExecuteCommandLists + WaitForFence, so by the time the
    // next owned frame begins, any objects referenced by the previous frame's
    // CL are safe to release.
    void d3dDeferDestroy(std::function<void()> destroy);
#endif
#if defined(ORE_BACKEND_WGPU)
    // External-encoder mode: Ore records into the host's wgpu::CommandEncoder
    // (already created by the caller) instead of owning its own. The host
    // retains ownership of Finish()/Submit() and frame sequencing. Unlike
    // Vulkan and D3D12, no deferred-destroy queue is needed: wgpu handles are
    // refcounted and command encoders / buffers pin referenced resources
    // until GPU completion, so dropping the C++ wrapper while the encoder
    // still references the resource is safe by WebGPU semantics.
    //
    // Enables cross-engine read-after-write (e.g. Rive renders into a canvas
    // then Ore samples it) that the owned-encoder model cannot support —
    // same motivation as the VK/D3D12 external overloads.
    void beginFrame(wgpu::CommandEncoder externalEncoder);
#endif
    void endFrame();

    // Block until the most recent endFrame() submission completes on the
    // GPU.  Call this before destroying Ore resources (textures, views,
    // pipelines) that were used in the current frame.  Not needed if
    // resources stay alive until the next beginFrame(), which waits
    // automatically.
    void waitForGPU();

    rcp<TextureView> wrapCanvasTexture(gpu::RenderCanvas* canvas);
    rcp<TextureView> wrapRiveTexture(gpu::Texture* gpuTex,
                                     uint32_t width,
                                     uint32_t height);

    ~Context();

    Context(Context&& other) noexcept;
    Context& operator=(Context&&) noexcept;
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;

#if defined(ORE_BACKEND_METAL)
    static Context createMetal(id<MTLDevice> device, id<MTLCommandQueue> queue);
#endif
#if defined(ORE_BACKEND_GL)
    static Context createGL();
#endif
#if defined(ORE_BACKEND_D3D11)
    static Context createD3D11(ID3D11Device* device,
                               ID3D11DeviceContext* context);
#endif
#if defined(ORE_BACKEND_D3D12)
    // Takes the D3D12 device and command queue. ORE shares the caller's
    // queue so that canvas-texture renders are visible to the host renderer
    // without cross-queue synchronization.
    static Context createD3D12(ID3D12Device* device, ID3D12CommandQueue* queue);
#endif
#if defined(ORE_BACKEND_WGPU)
    // backendType is wgpu::BackendType::OpenGLES or wgpu::BackendType::Vulkan,
    // obtained from RenderContextWebGPUImpl::capabilities().backendType.
    static Context createWGPU(wgpu::Device device,
                              wgpu::Queue queue,
                              wgpu::BackendType backendType);

    // Returns true when the runtime target is OpenGL ES (wagyu GLSLRAW path).
    // Use this to select between GLES and Vulkan GLSL shader source.
    bool isGLES() const { return m_wgpuBackend == WGPUBackend::OpenGLES; }
#endif
#if defined(ORE_BACKEND_VK)
    // Vulkan function dispatch table.
    // Populated by createVulkan(); all call sites use m_vk.Xxx(...) to avoid
    // direct vkXxx() symbols which are stripped when VK_NO_PROTOTYPES is
    // defined.
#define ORE_VK_INSTANCE_COMMANDS(F)                                            \
    F(GetDeviceProcAddr)                                                       \
    F(GetPhysicalDeviceProperties)                                             \
    F(GetPhysicalDeviceFeatures)

#define ORE_VK_DEVICE_COMMANDS(F)                                              \
    F(AllocateCommandBuffers)                                                  \
    F(AllocateDescriptorSets)                                                  \
    F(FreeDescriptorSets)                                                      \
    F(BeginCommandBuffer)                                                      \
    F(CmdBeginRenderPass)                                                      \
    F(CmdBindDescriptorSets)                                                   \
    F(CmdBindIndexBuffer)                                                      \
    F(CmdBindPipeline)                                                         \
    F(CmdBindVertexBuffers)                                                    \
    F(CmdCopyBufferToImage)                                                    \
    F(CmdDraw)                                                                 \
    F(CmdDrawIndexed)                                                          \
    F(CmdEndRenderPass)                                                        \
    F(CmdPipelineBarrier)                                                      \
    F(CmdSetBlendConstants)                                                    \
    F(CmdSetScissor)                                                           \
    F(CmdSetStencilReference)                                                  \
    F(CmdSetViewport)                                                          \
    F(CreateCommandPool)                                                       \
    F(CreateDescriptorPool)                                                    \
    F(CreateDescriptorSetLayout)                                               \
    F(CreateFramebuffer)                                                       \
    F(CreateGraphicsPipelines)                                                 \
    F(CreateImageView)                                                         \
    F(CreatePipelineLayout)                                                    \
    F(CreateRenderPass)                                                        \
    F(CreateSampler)                                                           \
    F(CreateShaderModule)                                                      \
    F(DestroyCommandPool)                                                      \
    F(DestroyDescriptorPool)                                                   \
    F(DestroyDescriptorSetLayout)                                              \
    F(DestroyFramebuffer)                                                      \
    F(DestroyImageView)                                                        \
    F(DestroyPipeline)                                                         \
    F(DestroyPipelineLayout)                                                   \
    F(DestroyRenderPass)                                                       \
    F(DestroySampler)                                                          \
    F(DestroyShaderModule)                                                     \
    F(EndCommandBuffer)                                                        \
    F(CreateFence)                                                             \
    F(DestroyFence)                                                            \
    F(WaitForFences)                                                           \
    F(ResetFences)                                                             \
    F(QueueSubmit)                                                             \
    F(QueueWaitIdle)                                                           \
    F(ResetCommandBuffer)                                                      \
    F(ResetDescriptorPool)                                                     \
    F(UpdateDescriptorSets)

    struct VkFn
    {
#define DECLARE_VK_CMD(CMD) PFN_vk##CMD CMD = nullptr;
        ORE_VK_INSTANCE_COMMANDS(DECLARE_VK_CMD)
        ORE_VK_DEVICE_COMMANDS(DECLARE_VK_CMD)
#undef DECLARE_VK_CMD
    };

    // Key used to cache VkRenderPass objects (one per unique format+load/store
    // combination). Stored by value — small enough for linear scan.
    //
    // `colorHasResolve[i]` flags MSAA color attachments that should resolve
    // into a single-sample target at end-of-pass. The render pass declares
    // a parallel set of resolve attachments and the framebuffer must
    // include the resolve image views (handled in beginRenderPass). A
    // resolve attachment is meaningful only when `sampleCount > 1`; a
    // single-sample render pass with `colorHasResolve[i] = true` produces
    // an invalid Vulkan render pass.
    struct VKRenderPassKey
    {
        TextureFormat colorFormats[4] = {};
        uint32_t colorCount = 0;
        LoadOp colorLoadOps[4] = {};
        StoreOp colorStoreOps[4] = {};
        bool colorHasResolve[4] = {};
        TextureFormat depthFormat = TextureFormat::depth24plusStencil8;
        LoadOp depthLoadOp = LoadOp::dontCare;
        StoreOp depthStoreOp = StoreOp::discard;
        bool hasDepth = false;
        uint32_t sampleCount = 1;

        bool operator==(const VKRenderPassKey& o) const
        {
            if (colorCount != o.colorCount || hasDepth != o.hasDepth ||
                sampleCount != o.sampleCount)
                return false;
            for (uint32_t i = 0; i < colorCount; ++i)
            {
                if (colorFormats[i] != o.colorFormats[i] ||
                    colorLoadOps[i] != o.colorLoadOps[i] ||
                    colorStoreOps[i] != o.colorStoreOps[i] ||
                    colorHasResolve[i] != o.colorHasResolve[i])
                    return false;
            }
            if (hasDepth &&
                (depthFormat != o.depthFormat || depthLoadOp != o.depthLoadOp ||
                 depthStoreOp != o.depthStoreOp))
                return false;
            return true;
        }
    };

    // The caller is responsible for creating all Vulkan objects and the VMA
    // allocator. ORE does not manage device lifetime.
    static Context createVulkan(
        VkInstance instance,
        VkPhysicalDevice physicalDevice,
        VkDevice device,
        VkQueue queue,
        uint32_t queueFamilyIndex,
        VmaAllocator allocator,
        PFN_vkGetInstanceProcAddr pfnGetInstanceProcAddr);

    // Look up or create a VkRenderPass compatible with the given key.
    // Called by makePipeline() (with DONT_CARE ops) and beginRenderPass().
    VkRenderPass getOrCreateRenderPass(const VKRenderPassKey&);
#endif

private:
    friend class RenderPass;
    friend class BindGroup;
    friend class Texture;

    Context();

    Features m_features;

    // Non-null while a RenderPass created by this context is still open.
    // beginRenderPass() auto-finishes any previous open pass so backends
    // that enforce one-encoder-at-a-time (Metal, D3D12) don't assert.
    RenderPass* m_activeRenderPass = nullptr;

    // Deferred destruction queues — cleared after endFrame() fence wait.
    // The rcp<> keeps objects alive until the GPU is done with them.
    std::vector<rcp<BindGroup>> m_deferredBindGroups;

    // Last validation error from setPipeline() / setBindGroup().
    std::string m_lastError;

#if defined(ORE_BACKEND_METAL)
    // Metal implementation helpers — shared by metal-only and metal+gl builds.
    // Defined inline in ore_context_metal.mm.
    inline void mtlPopulateFeatures(id<MTLDevice> device);
    inline rcp<Buffer> mtlMakeBuffer(const BufferDesc& desc);
    inline rcp<Texture> mtlMakeTexture(const TextureDesc& desc);
    inline rcp<TextureView> mtlMakeTextureView(const TextureViewDesc& desc);
    inline rcp<Sampler> mtlMakeSampler(const SamplerDesc& desc);
    inline rcp<ShaderModule> mtlMakeShaderModule(const ShaderModuleDesc& desc);
    inline rcp<Pipeline> mtlMakePipeline(const PipelineDesc& desc,
                                         std::string* outError);
    inline rcp<BindGroup> mtlMakeBindGroup(const BindGroupDesc& desc);
    inline RenderPass mtlBeginRenderPass(const RenderPassDesc& desc,
                                         std::string* outError);
    inline rcp<TextureView> mtlWrapCanvasTexture(gpu::RenderCanvas* canvas);
#endif

#if defined(ORE_BACKEND_METAL) && defined(ORE_BACKEND_GL)
    // Runtime backend tag: used when both Metal and GL coexist in the same
    // binary (macOS). createMetal() leaves this as Metal (default);
    // createGL() sets it to GL.
    enum class BackendType
    {
        Metal,
        GL
    };
    BackendType m_backendType = BackendType::Metal;
#elif defined(ORE_BACKEND_VK) && defined(ORE_BACKEND_GL)
    // Runtime backend tag: used when both Vulkan and GL coexist in the same
    // binary (Android). createVulkan() leaves this as Vulkan (default);
    // createGL() sets it to GL.
    enum class BackendType
    {
        Vulkan,
        GL
    };
    BackendType m_backendType = BackendType::Vulkan;
#endif
#if defined(ORE_BACKEND_D3D11) && defined(ORE_BACKEND_D3D12)
    // Runtime backend tag: used when both D3D11 and D3D12 coexist in the
    // same binary (Windows CI). createD3D11() sets D3D11; createD3D12()
    // sets D3D12.
    enum class BackendType
    {
        D3D11,
        D3D12
    };
    BackendType m_backendType = BackendType::D3D12;
#endif

#if defined(ORE_BACKEND_METAL)
    id<MTLDevice> m_mtlDevice = nil;
    id<MTLCommandQueue> m_mtlQueue = nil;
    id<MTLCommandBuffer> m_mtlCommandBuffer = nil;
#endif
#if defined(ORE_BACKEND_GL)
    // GL state tracking for save/restore at frame boundaries.
    // NOTE: GL_ELEMENT_ARRAY_BUFFER is intentionally excluded — it is VAO
    // state, so restoring the VAO implicitly restores the element buffer.
    struct GLSavedState
    {
        int program = 0;
        int arrayBuffer = 0;
        int framebuffer = 0;
        int vertexArray = 0;
    } m_savedState;
#endif
#if defined(ORE_BACKEND_D3D11)
    // D3D11 implementation helpers — shared by d3d11-only and d3d11+d3d12
    // builds. Defined in ore_context_d3d11.cpp.
    inline rcp<Buffer> d3d11MakeBuffer(const BufferDesc& desc);
    inline rcp<Texture> d3d11MakeTexture(const TextureDesc& desc);
    inline rcp<TextureView> d3d11MakeTextureView(const TextureViewDesc& desc);
    inline rcp<Sampler> d3d11MakeSampler(const SamplerDesc& desc);
    inline rcp<ShaderModule> d3d11MakeShaderModule(
        const ShaderModuleDesc& desc);
    inline rcp<Pipeline> d3d11MakePipeline(const PipelineDesc& desc,
                                           std::string* outError);
    inline rcp<BindGroup> d3d11MakeBindGroup(const BindGroupDesc& desc);
    inline rcp<BindGroupLayout> d3d11MakeBindGroupLayout(
        const BindGroupLayoutDesc& desc);
    inline RenderPass d3d11BeginRenderPass(const RenderPassDesc& desc,
                                           std::string* outError);
    inline rcp<TextureView> d3d11WrapCanvasTexture(gpu::RenderCanvas* canvas);
    inline rcp<TextureView> d3d11WrapRiveTexture(gpu::Texture* gpuTex,
                                                 uint32_t w,
                                                 uint32_t h);

    Microsoft::WRL::ComPtr<ID3D11Device> m_d3d11Device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_d3d11Context;
    // 11.1 context for offset-range constant-buffer binds
    // (VSSetConstantBuffers1 / PSSetConstantBuffers1). Acquired via QI at
    // createD3D11(); nullptr on pre-11.1 drivers, in which case static-offset
    // and dynamic-offset UBOs fall back to whole-buffer binds.
    Microsoft::WRL::ComPtr<ID3D11DeviceContext1> m_d3d11Context1;
#endif
#if defined(ORE_BACKEND_D3D12)
    // D3D12 implementation helpers — shared by d3d12-only and d3d11+d3d12
    // builds. Defined in ore_context_d3d12.cpp.
    inline rcp<Buffer> d3d12MakeBuffer(const BufferDesc& desc);
    inline rcp<Texture> d3d12MakeTexture(const TextureDesc& desc);
    inline rcp<TextureView> d3d12MakeTextureView(const TextureViewDesc& desc);
    inline rcp<Sampler> d3d12MakeSampler(const SamplerDesc& desc);
    inline rcp<ShaderModule> d3d12MakeShaderModule(
        const ShaderModuleDesc& desc);
    inline rcp<Pipeline> d3d12MakePipeline(const PipelineDesc& desc,
                                           std::string* outError);
    inline rcp<BindGroup> d3d12MakeBindGroup(const BindGroupDesc& desc);
    inline rcp<BindGroupLayout> d3d12MakeBindGroupLayout(
        const BindGroupLayoutDesc& desc);
    inline RenderPass d3d12BeginRenderPass(const RenderPassDesc& desc,
                                           std::string* outError);
    inline rcp<TextureView> d3d12WrapCanvasTexture(gpu::RenderCanvas* canvas);
    inline rcp<TextureView> d3d12WrapRiveTexture(gpu::Texture* gpuTex,
                                                 uint32_t w,
                                                 uint32_t h);

    Microsoft::WRL::ComPtr<ID3D12Device> m_d3dDevice;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_d3dQueue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_d3dAllocator;
    // ComPtr that owns the command list Ore allocates for owned-CL frames.
    // Not used directly by recording code — see m_d3dCmdList below.
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_d3dOwnedCmdList;
    // Active command list for the current frame. Points at m_d3dOwnedCmdList
    // in owned-CL mode and at the host-provided command list in external-CL
    // mode. All recording code reads through this pointer, so the two modes
    // share one code path.
    ID3D12GraphicsCommandList* m_d3dCmdList = nullptr;
    // Separate allocator/list for texture uploads (flushed at beginFrame).
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_d3dUploadAllocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_d3dUploadCmdList;
    bool m_d3dUploadListOpen = false;
    Microsoft::WRL::ComPtr<ID3D12Fence> m_d3dFence;
    uint64_t m_d3dFenceValue = 0;
    HANDLE m_d3dFenceEvent = nullptr;
    // CPU-visible descriptor heaps for creating descriptors at
    // resource-creation time.
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_d3dCpuSrvHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_d3dCpuRtvHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_d3dCpuDsvHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_d3dCpuSamplerHeap;
    UINT m_d3dCpuSrvAllocated = 0;
    UINT m_d3dCpuRtvAllocated = 0;
    UINT m_d3dCpuDsvAllocated = 0;
    UINT m_d3dCpuSamplerAllocated = 0;
    UINT m_d3dSrvDescSize = 0;
    UINT m_d3dRtvDescSize = 0;
    UINT m_d3dDsvDescSize = 0;
    UINT m_d3dSamplerDescSize = 0;
    // Null descriptors for unused descriptor-table slots.
    D3D12_CPU_DESCRIPTOR_HANDLE m_d3dNullSrv = {};
    D3D12_CPU_DESCRIPTOR_HANDLE m_d3dNullSampler = {};
    // GPU-visible heaps: reset at beginFrame, allocated linearly per draw.
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_d3dGpuSrvHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_d3dGpuSamplerHeap;
    UINT m_d3dGpuSrvAllocated = 0;
    UINT m_d3dGpuSamplerAllocated = 0;
    // Upload buffers kept alive until the frame fence-wait completes.
    std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> m_d3dPendingUploads;
    // True while the current frame is recording into a host-provided command
    // list. endFrame() skips Close/Execute/Wait when set; next beginFrame()
    // drains deferred destroys.
    bool m_d3dExternalCmdList = false;
    // Deferred-destroy closures (pipelines, samplers, buffers, textures,
    // views) queued while m_d3dExternalCmdList is true, drained at the next
    // beginFrame() or in the destructor after a GPU idle.
    std::vector<std::function<void()>> m_d3dDeferredDestroys;
    // Drain the deferred-destroy queue. Caller is responsible for ensuring
    // the prior GPU work has completed (via our fence or the host's).
    void d3dDrainDeferred();
    // Helpers called by RenderPass to allocate and resolve descriptor handles.
    UINT d3d12AllocGpuSrvSlots(UINT count);
    UINT d3d12AllocGpuSamplerSlots(UINT count);
    D3D12_CPU_DESCRIPTOR_HANDLE d3d12GpuSrvCpuHandle(UINT index) const;
    D3D12_GPU_DESCRIPTOR_HANDLE d3d12GpuSrvGpuHandle(UINT index) const;
    D3D12_CPU_DESCRIPTOR_HANDLE d3d12GpuSamplerCpuHandle(UINT index) const;
    D3D12_GPU_DESCRIPTOR_HANDLE d3d12GpuSamplerGpuHandle(UINT index) const;
    // Flush any open upload command list (called from beginFrame).
    void d3d12FlushUploads();
#endif
#if defined(ORE_BACKEND_WGPU)
    enum class WGPUBackend
    {
        OpenGLES,
        Vulkan
    };
    WGPUBackend m_wgpuBackend = WGPUBackend::Vulkan;
    wgpu::Device m_wgpuDevice;
    wgpu::Queue m_wgpuQueue;
    wgpu::CommandEncoder m_wgpuCommandEncoder;
    // True while the current frame is recording into a host-provided encoder.
    // endFrame() skips Finish()/Submit() when set.
    bool m_wgpuExternalEncoder = false;
#endif
#if defined(ORE_BACKEND_VK)
    VkFn m_vk;
    VkPhysicalDevice m_vkPhysicalDevice = VK_NULL_HANDLE;
    VkDevice m_vkDevice = VK_NULL_HANDLE;
    VkQueue m_vkQueue = VK_NULL_HANDLE;
    uint32_t m_vkQueueFamily = 0;
    VmaAllocator m_vmaAllocator = VK_NULL_HANDLE;
    VkCommandPool m_vkCommandPool = VK_NULL_HANDLE;
    VkCommandBuffer m_vkCommandBuffer = VK_NULL_HANDLE;
    // True while the current frame is recording into a host-provided CB.
    // endFrame() skips End/Submit/Wait when set; next beginFrame() drains
    // deferred destroys.
    bool m_vkExternalCmdBuf = false;
    VkDescriptorPool m_vkDescriptorPool = VK_NULL_HANDLE;
    VkFence m_vkFrameFence = VK_NULL_HANDLE;
    // Framebuffers whose destruction is deferred until the next
    // beginFrame() fence-wait (they are still referenced by the in-flight
    // command buffer until the fence signals).
    std::vector<VkFramebuffer> m_vkDeferredFramebuffers;
    // Staging buffers whose destruction is deferred until endFrame() submits
    // and waits on the fence. Texture::upload() records copy commands into
    // the frame command buffer, so the staging buffer must stay alive until
    // the command buffer finishes executing.
    struct DeferredVmaBuffer
    {
        VkBuffer buffer;
        VmaAllocation allocation;
    };
    std::vector<DeferredVmaBuffer> m_vkDeferredStagingBuffers;
    // Generic deferred-destroy closures (pipelines, samplers, etc.) for
    // external-CB mode, where Ore cannot vkDestroy* an object the moment
    // its refcount reaches zero because the host's CB still references it.
    std::vector<std::function<void()>> m_vkDeferredDestroys;
    // Persistent descriptor pool for long-lived BindGroup objects.
    // Created with VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT so
    // individual sets can be freed in BindGroup::onRefCntReachedZero()
    // without affecting other allocations.
    VkDescriptorPool m_vkPersistentDescriptorPool = VK_NULL_HANDLE;

    // Lazy-created shared empty `VkDescriptorSetLayout`. Substituted for
    // null entries in `PipelineDesc::bindGroupLayouts[]` so Vulkan's
    // `VkPipelineLayoutCreateInfo::pSetLayouts` array is fully populated
    // with valid handles (Vulkan spec VUID-VkPipelineLayoutCreateInfo-
    // graphicsPipelineLibrary-06753 forbids `VK_NULL_HANDLE` slots
    // without the graphicsPipelineLibrary feature). Destroyed at Context
    // teardown via `vkDeferDestroy` if needed.
    VkDescriptorSetLayout m_vkEmptyDSL = VK_NULL_HANDLE;
    VkDescriptorSetLayout vkGetOrCreateEmptyDSL();
    // Render pass cache — typically <10 entries, keyed on format+load/store
    // ops.
    std::vector<std::pair<VKRenderPassKey, VkRenderPass>> m_vkRenderPassCache;

    // Pending UNDEFINED → SHADER_READ_ONLY_OPTIMAL layout transitions for
    // textures bound as sampled images in a BindGroup before their VkImage
    // layout has been initialized by upload() or a render pass.  A render-
    // target-capable texture that is sampled without first being rendered
    // into would otherwise violate VUID-vkCmdDraw-None-09600 — the
    // descriptor declares SHADER_READ_ONLY_OPTIMAL but the image is still
    // VK_IMAGE_LAYOUT_UNDEFINED from creation.  Recorded in makeBindGroup()
    // and flushed onto the frame command buffer at beginFrame() / at the
    // start of beginRenderPass(), before any draw can reference the
    // descriptor.  Matches WebGPU's lazy-initialize-on-first-use semantic.
    // The rcp<Texture> pins the image alive until the barrier is recorded.
    struct VkPendingImageTransition
    {
        rcp<Texture> texture;
        VkImageAspectFlags aspectMask;
    };
    std::vector<VkPendingImageTransition> m_vkPendingInitialTransitions;
    void vkFlushPendingInitialTransitions();

    // Drain framebuffers/staging buffers and deferred destroy closures
    // from the previous frame. Caller is responsible for ensuring the prior
    // GPU work has completed (via our fence or the host's).
    void vkDrainDeferred();
#endif
};

// ============================================================================
// RenderPass inline helpers — defined here rather than in ore_render_pass.hpp
// because they depend on Pipeline::desc() / TextureView::texture()->format(),
// which are pulled in by this header's full set of Ore includes.
// ============================================================================

inline void RenderPass::populateAttachmentMetadata(const RenderPassDesc& desc)
{
    m_colorCount = desc.colorCount;
    for (uint32_t i = 0; i < desc.colorCount; ++i)
    {
        TextureView* view = desc.colorAttachments[i].view;
        if (!view || !view->texture())
            continue;
        m_colorFormats[i] = view->texture()->format();
        m_sampleCount = view->texture()->sampleCount();
    }
    if (desc.depthStencil.view && desc.depthStencil.view->texture())
    {
        m_depthFormat = desc.depthStencil.view->texture()->format();
        m_hasDepth = true;
        // If no colour attachments drove sampleCount, take it from depth.
        if (desc.colorCount == 0)
        {
            m_sampleCount = desc.depthStencil.view->texture()->sampleCount();
        }
    }
}

inline bool RenderPass::checkPipelineCompat(const Pipeline* pipeline) const
{
    if (!pipeline)
        return true;
    const PipelineDesc& pd = pipeline->desc();

    if (pd.colorCount != m_colorCount)
    {
        if (m_context)
            m_context->setLastError(
                "setPipeline: pipeline has %u color targets but render pass "
                "was begun with %u",
                pd.colorCount,
                m_colorCount);
        return false;
    }
    for (uint32_t i = 0; i < m_colorCount; ++i)
    {
        if (pd.colorTargets[i].format != m_colorFormats[i])
        {
            if (m_context)
                m_context->setLastError(
                    "setPipeline: color target %u format mismatch "
                    "(pipeline=%u, pass=%u)",
                    i,
                    static_cast<unsigned>(pd.colorTargets[i].format),
                    static_cast<unsigned>(m_colorFormats[i]));
            return false;
        }
    }
    if (pd.sampleCount != m_sampleCount)
    {
        if (m_context)
            m_context->setLastError(
                "setPipeline: sample count mismatch (pipeline=%u, pass=%u)",
                pd.sampleCount,
                m_sampleCount);
        return false;
    }
    // DepthStencilState::format == rgba8unorm is the "no depth" sentinel
    // (see ore_types.hpp:443). Treat that as "pipeline has no depth."
    const bool pipelineHasDepth =
        pd.depthStencil.format != TextureFormat::rgba8unorm;
    if (pipelineHasDepth != m_hasDepth)
    {
        if (m_context)
            m_context->setLastError(
                "setPipeline: depth attachment %s (pipeline=%d, pass=%d)",
                pipelineHasDepth ? "pipeline expects depth but pass has none"
                                 : "pipeline has no depth but pass provides it",
                (int)pipelineHasDepth,
                (int)m_hasDepth);
        return false;
    }
    if (pipelineHasDepth && pd.depthStencil.format != m_depthFormat)
    {
        if (m_context)
            m_context->setLastError(
                "setPipeline: depth format mismatch (pipeline=%u, pass=%u)",
                static_cast<unsigned>(pd.depthStencil.format),
                static_cast<unsigned>(m_depthFormat));
        return false;
    }
    return true;
}

} // namespace rive::ore
