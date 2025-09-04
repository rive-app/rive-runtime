#include "fiddle_context.hpp"

#if !defined(_WIN32) || defined(RIVE_UNREAL)

std::unique_ptr<FiddleContext> FiddleContext::MakeD3D12PLS(
    FiddleContextOptions fiddleOptions)
{
    return nullptr;
}

#else

#include "rive/renderer/rive_renderer.hpp"
#include "rive/renderer/d3d12/render_context_d3d12_impl.hpp"
#include <dxgi1_6.h>

#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_WIN32
#include "GLFW/glfw3.h"
#include <GLFW/glfw3native.h>

using namespace rive;
using namespace rive::gpu;

void GetHardwareAdapter(IDXGIFactory1* pFactory, IDXGIAdapter1** ppAdapter)
{
    using namespace Microsoft::WRL;

    *ppAdapter = nullptr;

    ComPtr<IDXGIAdapter1> adapter;

    ComPtr<IDXGIFactory6> factory6;
    if (SUCCEEDED(pFactory->QueryInterface(IID_PPV_ARGS(&factory6))))
    {
        for (UINT adapterIndex = 0;
             SUCCEEDED(factory6->EnumAdapterByGpuPreference(
                 adapterIndex,
                 DXGI_GPU_PREFERENCE_UNSPECIFIED,
                 IID_PPV_ARGS(&adapter)));
             ++adapterIndex)
        {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                // Don't select the Basic Render Driver adapter.
                continue;
            }

            // Check to see whether the adapter supports Direct3D 12, but don't
            // create the actual device yet.
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(),
                                            D3D_FEATURE_LEVEL_11_0,
                                            _uuidof(ID3D12Device),
                                            nullptr)))
            {
                break;
            }
        }
    }

    if (adapter.Get() == nullptr)
    {
        for (UINT adapterIndex = 0;
             SUCCEEDED(pFactory->EnumAdapters1(adapterIndex, &adapter));
             ++adapterIndex)
        {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);

            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
            {
                // Don't select the Basic Render Driver adapter.
                // If you want a software adapter, pass in "/warp" on the
                // command line.
                continue;
            }

            // Check to see whether the adapter supports Direct3D 12, but don't
            // create the actual device yet.
            if (SUCCEEDED(D3D12CreateDevice(adapter.Get(),
                                            D3D_FEATURE_LEVEL_11_0,
                                            _uuidof(ID3D12Device),
                                            nullptr)))
            {
                break;
            }
        }
    }

    *ppAdapter = adapter.Detach();
}

class FiddleContextD3D12PLS : public FiddleContext
{
public:
    FiddleContextD3D12PLS(ComPtr<IDXGIFactory4> factory,
                          ComPtr<ID3D12Device> device,
                          bool isHeadless,
                          D3DContextOptions& contextOptions) :
        m_isHeadless(isHeadless), m_factory(factory), m_device(device)
    {

        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

        VERIFY_OK(m_device->CreateCommandQueue(&queueDesc,
                                               IID_PPV_ARGS(&m_commandQueue)));

        NAME_RAW_D3D12_OBJECT(m_commandQueue);

        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
        VERIFY_OK(
            m_device->CreateCommandQueue(&queueDesc,
                                         IID_PPV_ARGS(&m_copyCommandQueue)));
        NAME_RAW_D3D12_OBJECT(m_copyCommandQueue);

        for (auto i = 0; i < FrameCount; ++i)
        {
            VERIFY_OK(m_device->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT,
                IID_PPV_ARGS(&m_allocators[i])));

            NAME_RAW_D3D12_OBJECT(m_allocators[i]);

            VERIFY_OK(m_device->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_COPY,
                IID_PPV_ARGS(&m_copyAllocators[i])));
            NAME_RAW_D3D12_OBJECT(m_copyAllocators[i]);
        }

        VERIFY_OK(m_device->CreateCommandList(0,
                                              D3D12_COMMAND_LIST_TYPE_DIRECT,
                                              m_allocators[0].Get(),
                                              NULL,
                                              IID_PPV_ARGS(&m_commandList)));
        NAME_RAW_D3D12_OBJECT(m_commandList);

        VERIFY_OK(
            m_device->CreateCommandList(0,
                                        D3D12_COMMAND_LIST_TYPE_COPY,
                                        m_copyAllocators[0].Get(),
                                        NULL,
                                        IID_PPV_ARGS(&m_copyCommandList)));
        NAME_RAW_D3D12_OBJECT(m_copyCommandList);

        m_renderContext =
            RenderContextD3D12Impl::MakeContext(m_device,
                                                m_copyCommandList.Get(),
                                                contextOptions);
        VERIFY_OK(m_copyCommandList->Close());
        VERIFY_OK(m_commandList->Close());

        ID3D12CommandList* ppCommandLists[] = {m_copyCommandList.Get()};
        m_copyCommandQueue->ExecuteCommandLists(1, ppCommandLists);

        VERIFY_OK(m_device->CreateFence(m_currentFrame,
                                        D3D12_FENCE_FLAG_NONE,
                                        IID_PPV_ARGS(&m_fence)));

        // NOTE: Originally the code was setting this to -1 and then waiting on
        // a signal of 0, but this does not work in practice because some D3D12
        // implementations only allow the signaled value to increase - so
        // starting the fence at unsigned -1 meant that it can never be changed
        // again.
        VERIFY_OK(m_device->CreateFence(m_currentFrame,
                                        D3D12_FENCE_FLAG_NONE,
                                        IID_PPV_ARGS(&m_copyFence)));

        // Increment m_currentFrame since the value we initialized the fences to
        // cannot be waited on (it'll return immediately).
        m_currentFrame++;

        m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        VERIFY_OK(HRESULT_FROM_WIN32(GetLastError()));

        assert(m_fenceEvent);

        // Signal the fence to ensure that we wait for creation to be complete.
        // Start at m_currentFrame which should currently be set to 1.
        VERIFY_OK(
            m_copyCommandQueue->Signal(m_copyFence.Get(), m_currentFrame));

        if (m_copyFence->GetCompletedValue() != m_currentFrame)
        {
            VERIFY_OK(m_copyFence->SetEventOnCompletion(m_currentFrame,
                                                        m_fenceEvent));
            WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);

            assert(m_copyFence->GetCompletedValue() == m_currentFrame);
        }

        // Increment the current frame one more to get past this wait.
        m_currentFrame++;
    }

    float dpiScale(GLFWwindow*) const override { return 1; }

    rive::Factory* factory() override { return m_renderContext.get(); }

    rive::gpu::RenderContext* renderContextOrNull() override
    {
        return m_renderContext.get();
    }

    rive::gpu::RenderTarget* renderTargetOrNull() override
    {
        return m_renderTargets[m_frameIndex].get();
    }

    void onSizeChanged(GLFWwindow* window,
                       int width,
                       int height,
                       uint32_t sampleCount) override
    {
        // wait for all frames to finish
        waitForLastFrame();

        if (!m_isHeadless)
        {
            if (!m_swapChain)
            {
                DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
                swapChainDesc.BufferCount = FrameCount;
                swapChainDesc.Width = width;
                swapChainDesc.Height = height;
                swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
                swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
                swapChainDesc.SampleDesc.Count = 1;
                swapChainDesc.Flags = 0;

                ComPtr<IDXGISwapChain1> swapChain;
                VERIFY_OK(m_factory->CreateSwapChainForHwnd(
                    m_commandQueue.Get(), // Swap chain needs the queue so that
                                          // it can force a flush on it.
                    glfwGetWin32Window(window),
                    &swapChainDesc,
                    nullptr,
                    nullptr,
                    &swapChain));

                VERIFY_OK(swapChain.As(&m_swapChain));
            }
            else
            {
                // we must release all references to resize swapchain
                for (auto i = 0; i < FrameCount; ++i)
                {
                    m_renderTargets[i]->releaseTexturesImmediately();
                }
                // invalidates all previous buffers
                m_swapChain->ResizeBuffers(FrameCount,
                                           width,
                                           height,
                                           DXGI_FORMAT_R8G8B8A8_UNORM,
                                           0);
            }

            for (auto i = 0; i < FrameCount; ++i)
            {
                ComPtr<ID3D12Resource> backbuffer;
                VERIFY_OK(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&backbuffer)));
                auto renderContextImpl =
                    m_renderContext->static_impl_cast<RenderContextD3D12Impl>();
                m_renderTargets[i] =
                    renderContextImpl->makeRenderTarget(width, height);
                m_renderTargets[i]->setTargetTexture(backbuffer);
            }
        }

        if (m_isHeadless)
        {
            ComPtr<ID3D12Resource> backbuffer;
            auto desc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM,
                                                     width,
                                                     height);
            desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
            D3D12_CLEAR_VALUE clearValue{DXGI_FORMAT_R8G8B8A8_UNORM, {}};
            auto heapProperties =
                CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
            m_device->CreateCommittedResource(&heapProperties,
                                              D3D12_HEAP_FLAG_NONE,
                                              &desc,
                                              D3D12_RESOURCE_STATE_PRESENT,
                                              &clearValue,
                                              IID_PPV_ARGS(&backbuffer));
            auto renderContextImpl =
                m_renderContext->static_impl_cast<RenderContextD3D12Impl>();
            m_renderTargets[0] =
                renderContextImpl->makeRenderTarget(width, height);
            m_renderTargets[0]->setTargetTexture(backbuffer);
        }
    }

    void toggleZoomWindow() override {}

    std::unique_ptr<Renderer> makeRenderer(int width, int height) override
    {
        return std::make_unique<RiveRenderer>(m_renderContext.get());
    }

    void begin(const rive::gpu::RenderContext::FrameDescriptor& frameDescriptor)
        override
    {
        m_renderContext->beginFrame(frameDescriptor);
    }

    UINT64 getFrameIndex() const
    {
        // when headless we only ever use one frames worth of textures for
        // rendering
        if (m_isHeadless)
        {
            return 0;
        }
        else
        {
            return m_swapChain->GetCurrentBackBufferIndex();
        }
    }

    UINT64 waitForNextSafeFrame()
    {
        // wait in case this swapchain still hasnt finished
        auto safeFrame = m_fence->GetCompletedValue();
        if (safeFrame < m_previousFrames[m_frameIndex])
        {
            VERIFY_OK(
                m_fence->SetEventOnCompletion(m_previousFrames[m_frameIndex],
                                              m_fenceEvent));
            WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
            safeFrame = m_previousFrames[m_frameIndex];
        }

        auto copySafeFrame = m_copyFence->GetCompletedValue();
        if (copySafeFrame < m_previousFrames[m_frameIndex])
        {
            VERIFY_OK(m_copyFence->SetEventOnCompletion(
                m_previousFrames[m_frameIndex],
                m_fenceEvent));
            WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
            copySafeFrame = m_previousFrames[m_frameIndex];
        }

        return std::min(copySafeFrame, safeFrame);
    }

    void waitForLastFrame()
    {
        auto frame = m_currentFrame++;
        m_previousFrames[m_frameIndex] = frame;
        VERIFY_OK(m_copyCommandQueue->Signal(m_copyFence.Get(), frame));
        VERIFY_OK(m_commandQueue->Signal(m_fence.Get(), frame));

        VERIFY_OK(m_fence->SetEventOnCompletion(frame, m_fenceEvent));
        WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);

        VERIFY_OK(m_copyFence->SetEventOnCompletion(frame, m_fenceEvent));
        WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
    }

    void moveToNextFrame()
    {
        VERIFY_OK(m_commandList->Close());
        VERIFY_OK(m_copyCommandList->Close());

        ID3D12CommandList* ppCopyCommandLists[] = {m_copyCommandList.Get()};

        ID3D12CommandList* ppCommandLists[] = {m_commandList.Get()};

        m_previousFrames[m_frameIndex] = m_currentFrame++;

        m_copyCommandQueue->ExecuteCommandLists(_countof(ppCopyCommandLists),
                                                ppCopyCommandLists);

        VERIFY_OK(m_copyCommandQueue->Signal(m_copyFence.Get(),
                                             m_previousFrames[m_frameIndex]));

        // tell the direct command que to wait for the copy command que to
        // finish
        m_commandQueue->Wait(m_copyFence.Get(), m_previousFrames[m_frameIndex]);

        m_commandQueue->ExecuteCommandLists(_countof(ppCommandLists),
                                            ppCommandLists);

        VERIFY_OK(m_commandQueue->Signal(m_fence.Get(),
                                         m_previousFrames[m_frameIndex]));
    }

    void flushPLSContext(RenderTarget* offscreenRenderTarget) final
    {
        m_frameIndex = getFrameIndex();

        auto safeFrame = waitForNextSafeFrame();

        VERIFY_OK(m_allocators[m_frameIndex]->Reset());

        VERIFY_OK(m_copyAllocators[m_frameIndex]->Reset());

        VERIFY_OK(m_commandList->Reset(m_allocators[m_frameIndex].Get(), NULL));

        VERIFY_OK(m_copyCommandList->Reset(m_copyAllocators[m_frameIndex].Get(),
                                           NULL));

        RenderContextD3D12Impl::CommandLists cmdLists = {
            m_copyCommandList.Get(),
            m_commandList.Get()};

        m_renderContext->flush({
            .renderTarget = offscreenRenderTarget != nullptr
                                ? offscreenRenderTarget
                                : m_renderTargets[m_frameIndex].get(),
            .externalCommandBuffer = &cmdLists,
            .currentFrameNumber = m_currentFrame,
            .safeFrameNumber = safeFrame,
        });

        moveToNextFrame();
    }

    void readBackPixels(std::vector<uint8_t>* pixelData)
    {
        waitForLastFrame();

        VERIFY_OK(m_allocators[m_frameIndex]->Reset());
        VERIFY_OK(m_copyAllocators[m_frameIndex]->Reset());
        VERIFY_OK(m_commandList->Reset(m_allocators[m_frameIndex].Get(), NULL));
        VERIFY_OK(m_copyCommandList->Reset(m_copyAllocators[m_frameIndex].Get(),
                                           NULL));

        ComPtr<ID3D12Resource> readbackBuffer;
        auto w = m_renderTargets[m_frameIndex]->width();
        auto h = m_renderTargets[m_frameIndex]->height();
        size_t outputBufferSize = w * h * 4;
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;

        auto targetTexture =
            m_renderTargets[m_frameIndex]->targetTexture()->resource();

        D3D12_HEAP_PROPERTIES readbackHeapProperties{
            CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK)};

        D3D12_RESOURCE_DESC readbackBufferDesc{
            CD3DX12_RESOURCE_DESC::Buffer(outputBufferSize)};

        VERIFY_OK(
            m_device->CreateCommittedResource(&readbackHeapProperties,
                                              D3D12_HEAP_FLAG_NONE,
                                              &readbackBufferDesc,
                                              D3D12_RESOURCE_STATE_COPY_DEST,
                                              nullptr,
                                              IID_PPV_ARGS(&readbackBuffer)));

        {
            D3D12_RESOURCE_BARRIER outputBufferResourceBarrier{
                CD3DX12_RESOURCE_BARRIER::Transition(
                    targetTexture,
                    D3D12_RESOURCE_STATE_PRESENT,
                    D3D12_RESOURCE_STATE_COPY_SOURCE)};
            m_commandList->ResourceBarrier(1, &outputBufferResourceBarrier);
        }

        UINT numRows;
        UINT64 rowSizeInBtes;
        UINT64 totalBytes;
        auto desc = targetTexture->GetDesc();
        m_device->GetCopyableFootprints(&desc,
                                        0,
                                        1,
                                        0,
                                        &footprint,
                                        &numRows,
                                        &rowSizeInBtes,
                                        &totalBytes);
        footprint.Footprint.RowPitch = w * 4;

        const CD3DX12_TEXTURE_COPY_LOCATION dst(readbackBuffer.Get(),
                                                footprint);
        const CD3DX12_TEXTURE_COPY_LOCATION src(targetTexture, 0);

        m_commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

        footprint.Footprint.RowPitch = static_cast<UINT>(rowSizeInBtes);

        {
            D3D12_RESOURCE_BARRIER outputBufferResourceBarrier{
                CD3DX12_RESOURCE_BARRIER::Transition(
                    targetTexture,
                    D3D12_RESOURCE_STATE_COPY_SOURCE,
                    D3D12_RESOURCE_STATE_PRESENT)};
            m_commandList->ResourceBarrier(1, &outputBufferResourceBarrier);
        }

        moveToNextFrame();

        waitForLastFrame();

        assert(pixelData);
        pixelData->resize(outputBufferSize);

        uint8_t* mappedData;
        D3D12_RANGE range{0, outputBufferSize};
        VERIFY_OK(readbackBuffer->Map(0,
                                      &range,
                                      reinterpret_cast<void**>(&mappedData)));

        for (uint32_t y = 0; y < h; ++y)
        {
            auto row = reinterpret_cast<const char*>(mappedData) +
                       footprint.Footprint.RowPitch * y;
            memcpy(pixelData->data() + (h - y - 1) * w * 4, row, w * 4);
        }

        D3D12_RANGE emptyRange{0, 0};
        readbackBuffer->Unmap(0, &emptyRange);
    }

    void end(GLFWwindow*, std::vector<uint8_t>* pixelData = nullptr) override
    {
        flushPLSContext(nullptr);

        if (pixelData != nullptr)
        {
            auto w = m_renderTargets[m_frameIndex]->width();
            auto h = m_renderTargets[m_frameIndex]->height();
            pixelData->resize(w * h * 4);
            readBackPixels(pixelData);
        }

        if (!m_isHeadless)
            m_swapChain->Present(0, 0);
    }

private:
    static constexpr SIZE_T FrameCount = 2;

    ComPtr<ID3D12CommandAllocator> m_allocators[FrameCount];
    ComPtr<ID3D12CommandAllocator> m_copyAllocators[FrameCount];
    rcp<RenderTargetD3D12> m_renderTargets[FrameCount];
    ComPtr<ID3D12Fence> m_fence;
    ComPtr<ID3D12Fence> m_copyFence;
    HANDLE m_fenceEvent = NULL;
    UINT64 m_previousFrames[FrameCount] = {0};
    UINT64 m_currentFrame = 0;
    UINT64 m_frameIndex = 0;
    const bool m_isHeadless;

    ComPtr<IDXGIFactory4> m_factory;
    ComPtr<IDXGISwapChain3> m_swapChain;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<ID3D12CommandQueue> m_copyCommandQueue;
    ComPtr<ID3D12GraphicsCommandList> m_commandList;
    ComPtr<ID3D12GraphicsCommandList> m_copyCommandList;
    ComPtr<ID3D12Device> m_device;
    std::unique_ptr<RenderContext> m_renderContext;
};

std::unique_ptr<FiddleContext> FiddleContext::MakeD3D12PLS(
    FiddleContextOptions fiddleOptions)
{
    UINT dxgiFactoryFlags = 0;

#ifdef DEBUG
    // Enable the debug layer (requires the Graphics Tools "optional feature").
    // NOTE: Enabling the debug layer after device creation will invalidate the
    // active device.
    {
        ComPtr<ID3D12Debug> debugController;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
        {
            debugController->EnableDebugLayer();

            // Enable additional debug layers.
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    }
#endif

    ComPtr<IDXGIFactory4> factory;
    VERIFY_OK(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

    ComPtr<ID3D12Device> device;

    D3DContextOptions contextOptions;

    contextOptions.shaderCompilationMode = fiddleOptions.shaderCompilationMode;

    if (fiddleOptions.d3d12UseWarpDevice)
    {
        ComPtr<IDXGIAdapter> warpAdapter;
        DXGI_ADAPTER_DESC adapterDesc{};
        VERIFY_OK(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));
        warpAdapter->GetDesc(&adapterDesc);
        contextOptions.isIntel = adapterDesc.VendorId == 0x163C ||
                                 adapterDesc.VendorId == 0x8086 ||
                                 adapterDesc.VendorId == 0x8087;
        VERIFY_OK(D3D12CreateDevice(warpAdapter.Get(),
                                    D3D_FEATURE_LEVEL_11_0,
                                    IID_PPV_ARGS(&device)));
        printf("D3D12 device: %S\n", adapterDesc.Description);
    }
    else
    {
        ComPtr<IDXGIAdapter1> hardwareAdapter;
        DXGI_ADAPTER_DESC adapterDesc{};
        GetHardwareAdapter(factory.Get(), &hardwareAdapter);
        hardwareAdapter->GetDesc(&adapterDesc);
        contextOptions.isIntel = adapterDesc.VendorId == 0x163C ||
                                 adapterDesc.VendorId == 0x8086 ||
                                 adapterDesc.VendorId == 0x8087;
        VERIFY_OK(D3D12CreateDevice(hardwareAdapter.Get(),
                                    D3D_FEATURE_LEVEL_11_0,
                                    IID_PPV_ARGS(&device)));
        printf("D3D12 device: %S\n", adapterDesc.Description);
    }

    if (!device)
    {
        return nullptr;
    }

    if (fiddleOptions.disableRasterOrdering)
    {
        contextOptions.disableRasterizerOrderedViews = true;
        // Also disable typed UAVs in atomic mode, to get more complete test
        // coverage.
        contextOptions.disableTypedUAVLoadStore = true;
    }

    return std::make_unique<FiddleContextD3D12PLS>(
        std::move(factory),
        std::move(device),
        fiddleOptions.allowHeadlessRendering,
        contextOptions);
}

#endif
