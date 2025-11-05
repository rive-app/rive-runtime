#include "fiddle_context.hpp"

#if !defined(_WIN32) || defined(RIVE_UNREAL)

std::unique_ptr<FiddleContext> FiddleContext::MakeD3DPLS(FiddleContextOptions)
{
    return nullptr;
}

#else

#include "rive/renderer/rive_renderer.hpp"
#include "rive/renderer/d3d/d3d_utils.hpp"
#include "rive/renderer/d3d11/render_context_d3d_impl.hpp"
#include "rive/profiler/profiler_macros.h"
#include <array>
#include <dxgi1_2.h>
#include <iostream>
#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_WIN32
#include "GLFW/glfw3.h"
#include <GLFW/glfw3native.h>

using namespace rive;
using namespace rive::gpu;

class FiddleContextD3DPLS : public FiddleContext
{
public:
    FiddleContextD3DPLS(ComPtr<IDXGIFactory2> d3dFactory,
                        ComPtr<ID3D11Device> gpu,
                        ComPtr<ID3D11DeviceContext> gpuContext,
                        bool isHeadless,
                        const D3DContextOptions& contextOptions) :
        m_isHeadless(isHeadless),
        m_d3dFactory(std::move(d3dFactory)),
        m_gpu(std::move(gpu)),
        m_gpuContext(std::move(gpuContext)),
        m_renderContext(RenderContextD3DImpl::MakeContext(m_gpu,
                                                          m_gpuContext,
                                                          contextOptions))
    {}

    float dpiScale(GLFWwindow*) const override { return 1; }

    rive::Factory* factory() override { return m_renderContext.get(); }

    rive::gpu::RenderContext* renderContextOrNull() override
    {
        return m_renderContext.get();
    }

    rive::gpu::RenderTarget* renderTargetOrNull() override
    {
        return m_renderTarget.get();
    }

    void onSizeChanged(GLFWwindow* window,
                       int width,
                       int height,
                       uint32_t sampleCount) override
    {
        if (!m_isHeadless)
        {
            m_swapchain.Reset();

            DXGI_SWAP_CHAIN_DESC1 scd{};
            scd.Width = width;
            scd.Height = height;
            scd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            scd.SampleDesc.Count = 1;
            scd.BufferUsage =
                DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_UNORDERED_ACCESS;
            scd.BufferCount = 2;
            scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
            VERIFY_OK(m_d3dFactory->CreateSwapChainForHwnd(
                m_gpu.Get(),
                glfwGetWin32Window(window),
                &scd,
                NULL,
                NULL,
                m_swapchain.ReleaseAndGetAddressOf()));
        }
        else
        {
            D3D11_TEXTURE2D_DESC desc{};
            desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            desc.MipLevels = 1;
            desc.Width = width;
            desc.Height = height;
            desc.SampleDesc.Count = 1;
            desc.ArraySize = 1;
            desc.Usage = D3D11_USAGE_DEFAULT;
            desc.BindFlags = D3D11_BIND_RENDER_TARGET;
            desc.CPUAccessFlags = 0;
            desc.MiscFlags = 0;
            VERIFY_OK(
                m_gpu->CreateTexture2D(&desc, NULL, &m_headlessDrawTexture));
        }

        auto renderContextImpl =
            m_renderContext->static_impl_cast<RenderContextD3DImpl>();
        m_renderTarget = renderContextImpl->makeRenderTarget(width, height);
        m_readbackTexture = nullptr;
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

    void flushPLSContext(RenderTarget* offscreenRenderTarget) final
    {
        RIVE_PROF_SCOPE()

        if (m_renderTarget->targetTexture() == nullptr)
        {
            if (m_isHeadless)
            {
                m_renderTarget->setTargetTexture(m_headlessDrawTexture);
            }
            else
            {
                ComPtr<ID3D11Texture2D> backbuffer;
                VERIFY_OK(m_swapchain->GetBuffer(
                    0,
                    __uuidof(ID3D11Texture2D),
                    reinterpret_cast<void**>(
                        backbuffer.ReleaseAndGetAddressOf())));
                m_renderTarget->setTargetTexture(backbuffer);
            }
        }
        m_renderContext->flush({
            .renderTarget = offscreenRenderTarget != nullptr
                                ? offscreenRenderTarget
                                : m_renderTarget.get(),
        });
    }

    void end(GLFWwindow*, std::vector<uint8_t>* pixelData = nullptr) override
    {
        RIVE_PROF_SCOPE()
        flushPLSContext(nullptr);
        if (pixelData != nullptr)
        {
            uint32_t w = m_renderTarget->width();
            uint32_t h = m_renderTarget->height();
            if (m_readbackTexture == nullptr)
            {
                D3D11_TEXTURE2D_DESC readbackTexDesc{};
                readbackTexDesc.Width = w;
                readbackTexDesc.Height = h;
                readbackTexDesc.MipLevels = 1;
                readbackTexDesc.ArraySize = 1;
                readbackTexDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                readbackTexDesc.SampleDesc.Count = 1;
                readbackTexDesc.Usage = D3D11_USAGE_STAGING;
                readbackTexDesc.BindFlags = 0;
                readbackTexDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
                readbackTexDesc.MiscFlags = 0;
                VERIFY_OK(m_gpu->CreateTexture2D(
                    &readbackTexDesc,
                    nullptr,
                    m_readbackTexture.ReleaseAndGetAddressOf()));
            }

            D3D11_MAPPED_SUBRESOURCE map;
            m_gpuContext->CopyResource(m_readbackTexture.Get(),
                                       m_renderTarget->targetTexture());
            m_gpuContext->Map(m_readbackTexture.Get(),
                              0,
                              D3D11_MAP_READ,
                              0,
                              &map);
            pixelData->resize(h * w * 4);
            for (uint32_t y = 0; y < h; ++y)
            {
                auto row =
                    reinterpret_cast<const char*>(map.pData) + map.RowPitch * y;
                memcpy(pixelData->data() + (h - y - 1) * w * 4, row, w * 4);
            }
            m_gpuContext->Unmap(m_readbackTexture.Get(), 0);
        }

        if (!m_isHeadless)
            m_swapchain->Present(0, 0);

        m_renderTarget->setTargetTexture(nullptr);
    }

private:
    const bool m_isHeadless;
    ComPtr<IDXGIFactory2> m_d3dFactory;
    ComPtr<ID3D11Device> m_gpu;
    ComPtr<ID3D11DeviceContext> m_gpuContext;
    ComPtr<IDXGISwapChain1> m_swapchain;
    ComPtr<ID3D11Texture2D> m_readbackTexture;
    ComPtr<ID3D11Texture2D> m_headlessDrawTexture;
    std::unique_ptr<RenderContext> m_renderContext;
    rcp<RenderTargetD3D> m_renderTarget;
};

std::unique_ptr<FiddleContext> FiddleContext::MakeD3DPLS(
    FiddleContextOptions fiddleOptions)
{
    // Create a DXGIFactory object.
    ComPtr<IDXGIFactory2> factory;
    VERIFY_OK(CreateDXGIFactory(
        __uuidof(IDXGIFactory2),
        reinterpret_cast<void**>(factory.ReleaseAndGetAddressOf())));

    ComPtr<IDXGIAdapter> adapter;
    DXGI_ADAPTER_DESC adapterDesc{};
    D3DContextOptions contextOptions;
    contextOptions.shaderCompilationMode = fiddleOptions.shaderCompilationMode;
    if (fiddleOptions.disableRasterOrdering)
    {
        contextOptions.disableRasterizerOrderedViews = true;
        // Also disable typed UAVs in atomic mode, to get more complete test
        // coverage.
        contextOptions.disableTypedUAVLoadStore = true;
    }
    bool shouldUseNameFilter = false;
    std::wstring gpuNameFilterW;
    if (fiddleOptions.gpuNameFilter)
    {
        shouldUseNameFilter =
            d3d_utils::GetWStringFromString(fiddleOptions.gpuNameFilter,
                                            gpuNameFilterW);
    }

    for (UINT i = 0; factory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND;
         ++i)
    {
        adapter->GetDesc(&adapterDesc);
        if (shouldUseNameFilter)
        {
            std::wstring gpuName(adapterDesc.Description,
                                 wcslen(adapterDesc.Description));
            if (gpuName.find(gpuNameFilterW) == std::wstring::npos)
            {
                adapterDesc = {};
                continue;
            }
        }

        contextOptions.isIntel = adapterDesc.VendorId == 0x163C ||
                                 adapterDesc.VendorId == 0x8086 ||
                                 adapterDesc.VendorId == 0x8087;

        break;
    }

    // if we used a filter and we foud nothing, use the first adaptor.
    if (shouldUseNameFilter && adapterDesc.DeviceId == 0)
    {
        std::cout << "Failed to find adaptor named \""
                  << fiddleOptions.gpuNameFilter
                  << "\" using first adaptor instead.\n";
        if (factory->EnumAdapters(0, &adapter) != DXGI_ERROR_NOT_FOUND)
        {
            adapter->GetDesc(&adapterDesc);
            contextOptions.isIntel = adapterDesc.VendorId == 0x163C ||
                                     adapterDesc.VendorId == 0x8086 ||
                                     adapterDesc.VendorId == 0x8087;
        }
    }

    ComPtr<ID3D11Device> gpu;
    ComPtr<ID3D11DeviceContext> gpuContext;
    D3D_FEATURE_LEVEL featureLevels[] = {D3D_FEATURE_LEVEL_11_1};
    UINT creationFlags = 0;
#ifdef DEBUG
    creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    VERIFY_OK(D3D11CreateDevice(adapter.Get(),
                                D3D_DRIVER_TYPE_UNKNOWN,
                                NULL,
                                creationFlags,
                                featureLevels,
                                std::size(featureLevels),
                                D3D11_SDK_VERSION,
                                gpu.ReleaseAndGetAddressOf(),
                                NULL,
                                gpuContext.ReleaseAndGetAddressOf()));
    if (!gpu || !gpuContext)
    {
        return nullptr;
    }

    printf("D3D11 device: %S\n", adapterDesc.Description);

    return std::make_unique<FiddleContextD3DPLS>(
        std::move(factory),
        std::move(gpu),
        std::move(gpuContext),
        fiddleOptions.allowHeadlessRendering,
        contextOptions);
}

#endif
