/*
 * Copyright 2025 Rive
 */

// Combined D3D11 + D3D12 Context implementation for Windows.
// Compiled when both ORE_BACKEND_D3D11 and ORE_BACKEND_D3D12 are active.
// Provides all Context method definitions with runtime dispatch via
// Context::BackendType so D3D11 and D3D12 contexts can coexist in one binary.

#if defined(ORE_BACKEND_D3D11) && defined(ORE_BACKEND_D3D12)

// Pull in D3D12 static helpers, d3d12Alloc helpers, d3d12FlushUploads, and
// d3d12* factory/wrap helpers. The !defined(ORE_BACKEND_D3D11) guard in
// ore_context_d3d12.cpp excludes Context public method bodies.
#include "ore_context_d3d12.cpp"

// Pull in D3D11 static helpers and d3d11* helper methods.
// The !defined(ORE_BACKEND_D3D12) guard in ore_context_d3d11.cpp excludes
// Context public method bodies.
#include "../d3d11/ore_context_d3d11.cpp"

namespace rive::ore
{

// ============================================================================
// Context lifecycle (D3D11 + D3D12 dispatch)
// ============================================================================

Context::Context() {}

Context::~Context()
{
    if (m_backendType == BackendType::D3D11)
    {
        m_d3d11Context.Reset();
        m_d3d11Device.Reset();
        return;
    }
    // D3D12 cleanup.
    if (m_d3dDevice)
    {
        if (m_d3dQueue && m_d3dFence && m_d3dFenceEvent)
            d3d12WaitFence(m_d3dQueue.Get(),
                           m_d3dFence.Get(),
                           m_d3dFenceValue,
                           m_d3dFenceEvent);
        // Drain any closures queued while in external-CL mode.
        d3dDrainDeferred();
        m_d3dPendingUploads.clear();
        if (m_d3dFenceEvent)
        {
            CloseHandle(m_d3dFenceEvent);
            m_d3dFenceEvent = nullptr;
        }
    }
}

Context::Context(Context&& other) noexcept :
    m_features(other.m_features),
    m_backendType(other.m_backendType),
    // D3D11 members
    m_d3d11Device(std::move(other.m_d3d11Device)),
    m_d3d11Context(std::move(other.m_d3d11Context)),
    m_d3d11Context1(std::move(other.m_d3d11Context1)),
    // D3D12 members
    m_d3dDevice(std::move(other.m_d3dDevice)),
    m_d3dQueue(std::move(other.m_d3dQueue)),
    m_d3dAllocator(std::move(other.m_d3dAllocator)),
    m_d3dOwnedCmdList(std::move(other.m_d3dOwnedCmdList)),
    m_d3dCmdList(other.m_d3dCmdList),
    m_d3dUploadAllocator(std::move(other.m_d3dUploadAllocator)),
    m_d3dUploadCmdList(std::move(other.m_d3dUploadCmdList)),
    m_d3dUploadListOpen(other.m_d3dUploadListOpen),
    m_d3dFence(std::move(other.m_d3dFence)),
    m_d3dFenceValue(other.m_d3dFenceValue),
    m_d3dFenceEvent(other.m_d3dFenceEvent),
    m_d3dCpuSrvHeap(std::move(other.m_d3dCpuSrvHeap)),
    m_d3dCpuRtvHeap(std::move(other.m_d3dCpuRtvHeap)),
    m_d3dCpuDsvHeap(std::move(other.m_d3dCpuDsvHeap)),
    m_d3dCpuSamplerHeap(std::move(other.m_d3dCpuSamplerHeap)),
    m_d3dCpuSrvAllocated(other.m_d3dCpuSrvAllocated),
    m_d3dCpuRtvAllocated(other.m_d3dCpuRtvAllocated),
    m_d3dCpuDsvAllocated(other.m_d3dCpuDsvAllocated),
    m_d3dCpuSamplerAllocated(other.m_d3dCpuSamplerAllocated),
    m_d3dSrvDescSize(other.m_d3dSrvDescSize),
    m_d3dRtvDescSize(other.m_d3dRtvDescSize),
    m_d3dDsvDescSize(other.m_d3dDsvDescSize),
    m_d3dSamplerDescSize(other.m_d3dSamplerDescSize),
    m_d3dNullSrv(other.m_d3dNullSrv),
    m_d3dNullSampler(other.m_d3dNullSampler),
    m_d3dGpuSrvHeap(std::move(other.m_d3dGpuSrvHeap)),
    m_d3dGpuSamplerHeap(std::move(other.m_d3dGpuSamplerHeap)),
    m_d3dGpuSrvAllocated(other.m_d3dGpuSrvAllocated),
    m_d3dGpuSamplerAllocated(other.m_d3dGpuSamplerAllocated),
    m_d3dPendingUploads(std::move(other.m_d3dPendingUploads)),
    m_d3dExternalCmdList(other.m_d3dExternalCmdList),
    m_d3dDeferredDestroys(std::move(other.m_d3dDeferredDestroys))
{
    other.m_d3dFenceEvent = nullptr;
    other.m_d3dFenceValue = 0;
    other.m_d3dUploadListOpen = false;
    other.m_d3dCmdList = nullptr;
    other.m_d3dExternalCmdList = false;
}

Context& Context::operator=(Context&& other) noexcept
{
    if (this != &other)
    {
        this->~Context();
        new (this) Context(std::move(other));
    }
    return *this;
}

// ============================================================================
// createD3D11 / createD3D12
// ============================================================================

Context Context::createD3D11(ID3D11Device* device, ID3D11DeviceContext* context)
{
    Context ctx;
    ctx.m_backendType = BackendType::D3D11;
    ctx.m_d3d11Device = device;
    ctx.m_d3d11Context = context;
    // QI for offset-range CB binds (VSSetConstantBuffers1 /
    // PSSetConstantBuffers1).
    context->QueryInterface(IID_PPV_ARGS(ctx.m_d3d11Context1.GetAddressOf()));

    // Populate features for D3D11 feature level 11.0.
    Features& f = ctx.m_features;
    f.colorBufferFloat = true;
    f.perTargetBlend = true;
    f.perTargetWriteMask = true;
    f.textureViewSampling = true;
    f.drawBaseInstance = true;
    f.depthBiasClamp = true;
    f.anisotropicFiltering = true;
    f.texture3D = true;
    f.textureArrays = true;
    f.computeShaders = true;
    f.storageBuffers = true;
    f.bc = true;
    f.etc2 = false;
    f.astc = false;

    f.maxColorAttachments = 8;
    f.maxTextureSize2D = 16384;
    f.maxTextureSizeCube = 16384;
    f.maxTextureSize3D = 2048;
    f.maxUniformBufferSize = 64 * 1024;
    f.maxVertexAttributes = 16;
    f.maxSamplers = 16;

    return ctx;
}

Context Context::createD3D12(ID3D12Device* device, ID3D12CommandQueue* queue)
{
    Context ctx;
    ctx.m_backendType = BackendType::D3D12;
    ctx.m_d3dDevice = device;
    ctx.m_d3dQueue = queue;

    // --- Command allocator + list for rendering ---
    [[maybe_unused]] HRESULT hr;
    hr = device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(ctx.m_d3dAllocator.GetAddressOf()));
    assert(SUCCEEDED(hr));
    hr = device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        ctx.m_d3dAllocator.Get(),
        nullptr,
        IID_PPV_ARGS(ctx.m_d3dOwnedCmdList.GetAddressOf()));
    assert(SUCCEEDED(hr));
    ctx.m_d3dCmdList = ctx.m_d3dOwnedCmdList.Get();
    ctx.m_d3dCmdList->Close();

    // --- Upload allocator + list ---
    hr = device->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(ctx.m_d3dUploadAllocator.GetAddressOf()));
    assert(SUCCEEDED(hr));
    hr = device->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        ctx.m_d3dUploadAllocator.Get(),
        nullptr,
        IID_PPV_ARGS(ctx.m_d3dUploadCmdList.GetAddressOf()));
    assert(SUCCEEDED(hr));
    ctx.m_d3dUploadCmdList->Close();

    // --- Fence ---
    hr = device->CreateFence(0,
                             D3D12_FENCE_FLAG_NONE,
                             IID_PPV_ARGS(ctx.m_d3dFence.GetAddressOf()));
    assert(SUCCEEDED(hr));
    ctx.m_d3dFenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    assert(ctx.m_d3dFenceEvent != nullptr);

    // Root signatures are per-pipeline (RFC v5 §3.2.2). See
    // `ore_context_d3d12.cpp::createD3D12` for the full commentary.

    // --- Descriptor heap sizes ---
    ctx.m_d3dSrvDescSize = device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    ctx.m_d3dRtvDescSize = device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    ctx.m_d3dDsvDescSize = device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    ctx.m_d3dSamplerDescSize = device->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

    // --- CPU-visible heaps ---
    auto makeCpuHeap = [&](D3D12_DESCRIPTOR_HEAP_TYPE type,
                           UINT count,
                           ComPtr<ID3D12DescriptorHeap>& out) {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = type;
        desc.NumDescriptors = count;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        [[maybe_unused]] HRESULT r =
            device->CreateDescriptorHeap(&desc,
                                         IID_PPV_ARGS(out.GetAddressOf()));
        assert(SUCCEEDED(r));
    };
    makeCpuHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                1024,
                ctx.m_d3dCpuSrvHeap);
    makeCpuHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 256, ctx.m_d3dCpuRtvHeap);
    makeCpuHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 64, ctx.m_d3dCpuDsvHeap);
    makeCpuHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
                256,
                ctx.m_d3dCpuSamplerHeap);

    // --- GPU-visible heaps ---
    auto makeGpuHeap = [&](D3D12_DESCRIPTOR_HEAP_TYPE type,
                           UINT count,
                           ComPtr<ID3D12DescriptorHeap>& out) {
        D3D12_DESCRIPTOR_HEAP_DESC desc = {};
        desc.Type = type;
        desc.NumDescriptors = count;
        desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        [[maybe_unused]] HRESULT r =
            device->CreateDescriptorHeap(&desc,
                                         IID_PPV_ARGS(out.GetAddressOf()));
        assert(SUCCEEDED(r));
    };
    makeGpuHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                4096,
                ctx.m_d3dGpuSrvHeap);
    makeGpuHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
                512,
                ctx.m_d3dGpuSamplerHeap);

    // --- Null SRV ---
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle =
            ctx.m_d3dCpuSrvHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += ctx.m_d3dCpuSrvAllocated++ * ctx.m_d3dSrvDescSize;
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Shader4ComponentMapping =
            D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Texture2D.MipLevels = 1;
        device->CreateShaderResourceView(nullptr, &srvDesc, handle);
        ctx.m_d3dNullSrv = handle;
    }

    // --- Null Sampler ---
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle =
            ctx.m_d3dCpuSamplerHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += ctx.m_d3dCpuSamplerAllocated++ * ctx.m_d3dSamplerDescSize;
        D3D12_SAMPLER_DESC sampDesc = {};
        sampDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        sampDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        sampDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
        sampDesc.MaxLOD = D3D12_FLOAT32_MAX;
        device->CreateSampler(&sampDesc, handle);
        ctx.m_d3dNullSampler = handle;
    }

    // --- Features ---
    Features& f = ctx.m_features;
    f.colorBufferFloat = true;
    f.perTargetBlend = true;
    f.perTargetWriteMask = true;
    f.textureViewSampling = true;
    f.drawBaseInstance = true;
    f.depthBiasClamp = true;
    f.anisotropicFiltering = true;
    f.texture3D = true;
    f.textureArrays = true;
    f.computeShaders = true;
    f.storageBuffers = true;
    f.bc = true;
    f.etc2 = false;
    f.astc = false;
    f.maxColorAttachments = 8;
    f.maxTextureSize2D = 16384;
    f.maxTextureSizeCube = 16384;
    f.maxTextureSize3D = 2048;
    f.maxUniformBufferSize = 65536;
    f.maxVertexAttributes = 16;
    f.maxSamplers = 16;

    return ctx;
}

// ============================================================================
// beginFrame / endFrame / waitForGPU
// ============================================================================

void Context::beginFrame()
{
    m_deferredBindGroups.clear();

    if (m_backendType == BackendType::D3D11)
        return; // D3D11 immediate context has no frame-level state.

    // D3D12 path.
    d3dDrainDeferred();
    m_d3dExternalCmdList = false;
    m_d3dCmdList = m_d3dOwnedCmdList.Get();

    d3d12FlushUploads();
    m_d3dAllocator->Reset();
    m_d3dCmdList->Reset(m_d3dAllocator.Get(), nullptr);

    m_d3dGpuSrvAllocated = 0;
    m_d3dGpuSamplerAllocated = 0;

    ID3D12DescriptorHeap* heaps[] = {m_d3dGpuSrvHeap.Get(),
                                     m_d3dGpuSamplerHeap.Get()};
    m_d3dCmdList->SetDescriptorHeaps(2, heaps);
    // Root sig is bound by the first setPipeline — every Ore pipeline
    // ships with its own per-pipeline root sig (RFC v5 §3.2.2).
}

void Context::beginFrame(ID3D12GraphicsCommandList* externalCl)
{
    assert(externalCl != nullptr);
    assert(m_backendType == BackendType::D3D12 &&
           "beginFrame(externalCl) is only valid on D3D12 contexts");

    m_deferredBindGroups.clear();
    d3dDrainDeferred();

    d3d12FlushUploads();

    m_d3dGpuSrvAllocated = 0;
    m_d3dGpuSamplerAllocated = 0;

    m_d3dCmdList = externalCl;
    m_d3dExternalCmdList = true;

    ID3D12DescriptorHeap* heaps[] = {m_d3dGpuSrvHeap.Get(),
                                     m_d3dGpuSamplerHeap.Get()};
    m_d3dCmdList->SetDescriptorHeaps(2, heaps);
    // Root sig is bound by the first setPipeline — every Ore pipeline
    // ships with its own per-pipeline root sig (RFC v5 §3.2.2).
}

void Context::waitForGPU()
{
    if (m_backendType == BackendType::D3D11)
        return; // D3D11 immediate context is synchronous.

    d3d12WaitFence(m_d3dQueue.Get(),
                   m_d3dFence.Get(),
                   m_d3dFenceValue,
                   m_d3dFenceEvent);
}

void Context::endFrame()
{
    if (m_backendType == BackendType::D3D11)
        return; // D3D11 immediate context — nothing to do.

    // D3D12 path.
    if (m_d3dExternalCmdList)
    {
        // Host owns Close/Execute/fence. Next beginFrame() drains deferred
        // destroys once the host has waited on its own frame fence.
        return;
    }

    m_d3dOwnedCmdList->Close();
    ID3D12CommandList* lists[] = {m_d3dOwnedCmdList.Get()};
    m_d3dQueue->ExecuteCommandLists(1, lists);

    d3d12WaitFence(m_d3dQueue.Get(),
                   m_d3dFence.Get(),
                   m_d3dFenceValue,
                   m_d3dFenceEvent);

    m_d3dPendingUploads.clear();
}

// ============================================================================
// Factory methods — thin dispatch to d3d11* / d3d12* helpers
// ============================================================================

rcp<Buffer> Context::makeBuffer(const BufferDesc& desc)
{
    if (m_backendType == BackendType::D3D11)
        return d3d11MakeBuffer(desc);
    return d3d12MakeBuffer(desc);
}

rcp<Texture> Context::makeTexture(const TextureDesc& desc)
{
    if (m_backendType == BackendType::D3D11)
        return d3d11MakeTexture(desc);
    return d3d12MakeTexture(desc);
}

rcp<TextureView> Context::makeTextureView(const TextureViewDesc& desc)
{
    if (m_backendType == BackendType::D3D11)
        return d3d11MakeTextureView(desc);
    return d3d12MakeTextureView(desc);
}

rcp<Sampler> Context::makeSampler(const SamplerDesc& desc)
{
    if (m_backendType == BackendType::D3D11)
        return d3d11MakeSampler(desc);
    return d3d12MakeSampler(desc);
}

rcp<ShaderModule> Context::makeShaderModule(const ShaderModuleDesc& desc)
{
    if (m_backendType == BackendType::D3D11)
        return d3d11MakeShaderModule(desc);
    return d3d12MakeShaderModule(desc);
}

rcp<Pipeline> Context::makePipeline(const PipelineDesc& desc,
                                    std::string* outError)
{
    if (m_backendType == BackendType::D3D11)
        return d3d11MakePipeline(desc, outError);
    return d3d12MakePipeline(desc, outError);
}

rcp<BindGroup> Context::makeBindGroup(const BindGroupDesc& desc)
{
    if (m_backendType == BackendType::D3D11)
        return d3d11MakeBindGroup(desc);
    return d3d12MakeBindGroup(desc);
}

rcp<BindGroupLayout> Context::makeBindGroupLayout(
    const BindGroupLayoutDesc& desc)
{
    if (m_backendType == BackendType::D3D11)
        return d3d11MakeBindGroupLayout(desc);
    return d3d12MakeBindGroupLayout(desc);
}

RenderPass Context::beginRenderPass(const RenderPassDesc& desc,
                                    std::string* outError)
{
    finishActiveRenderPass();
    if (m_backendType == BackendType::D3D11)
        return d3d11BeginRenderPass(desc, outError);
    return d3d12BeginRenderPass(desc, outError);
}

rcp<TextureView> Context::wrapCanvasTexture(gpu::RenderCanvas* canvas)
{
    if (m_backendType == BackendType::D3D11)
        return d3d11WrapCanvasTexture(canvas);
    return d3d12WrapCanvasTexture(canvas);
}

rcp<TextureView> Context::wrapRiveTexture(gpu::Texture* gpuTex,
                                          uint32_t w,
                                          uint32_t h)
{
    if (m_backendType == BackendType::D3D11)
        return d3d11WrapRiveTexture(gpuTex, w, h);
    return d3d12WrapRiveTexture(gpuTex, w, h);
}

} // namespace rive::ore

#endif // ORE_BACKEND_D3D11 && ORE_BACKEND_D3D12
