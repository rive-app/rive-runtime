#include "fiddle_context.hpp"

#ifndef _WIN32

std::unique_ptr<FiddleContext> FiddleContext::MakeD3DPLS(FiddleContextOptions) { return nullptr; }

#else

#include "rive/renderer/rive_renderer.hpp"
#include "rive/renderer/d3d/render_context_d3d_impl.hpp"
#include "rive/renderer/d3d/d3d11.hpp"
#include <array>
#include <dxgi1_2.h>

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
                        const PLSRenderContextD3DImpl::ContextOptions& contextOptions) :
        m_d3dFactory(std::move(d3dFactory)),
        m_gpu(std::move(gpu)),
        m_gpuContext(std::move(gpuContext)),
        m_plsContext(PLSRenderContextD3DImpl::MakeContext(m_gpu, m_gpuContext, contextOptions))
    {}

    float dpiScale(GLFWwindow*) const override { return 1; }

    rive::Factory* factory() override { return m_plsContext.get(); }

    rive::gpu::PLSRenderContext* plsContextOrNull() override { return m_plsContext.get(); }

    rive::gpu::PLSRenderTarget* plsRenderTargetOrNull() override { return m_renderTarget.get(); }

    void onSizeChanged(GLFWwindow* window, int width, int height, uint32_t sampleCount) override
    {
        m_swapchain.Reset();

        DXGI_SWAP_CHAIN_DESC1 scd{};
        scd.Width = width;
        scd.Height = height;
        scd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        scd.SampleDesc.Count = 1;
        scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_UNORDERED_ACCESS;
        scd.BufferCount = 2;
        scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        VERIFY_OK(m_d3dFactory->CreateSwapChainForHwnd(m_gpu.Get(),
                                                       glfwGetWin32Window(window),
                                                       &scd,
                                                       NULL,
                                                       NULL,
                                                       m_swapchain.ReleaseAndGetAddressOf()));

        auto plsContextImpl = m_plsContext->static_impl_cast<PLSRenderContextD3DImpl>();
        m_renderTarget = plsContextImpl->makeRenderTarget(width, height);
        m_readbackTexture = nullptr;
    }

    void toggleZoomWindow() override {}

    std::unique_ptr<Renderer> makeRenderer(int width, int height) override
    {
        return std::make_unique<RiveRenderer>(m_plsContext.get());
    }

    void begin(const rive::gpu::PLSRenderContext::FrameDescriptor& frameDescriptor) override
    {
        m_plsContext->beginFrame(frameDescriptor);
    }

    void flushPLSContext() final
    {
        if (m_renderTarget->targetTexture() == nullptr)
        {
            ComPtr<ID3D11Texture2D> backbuffer;
            VERIFY_OK(m_swapchain->GetBuffer(
                0,
                __uuidof(ID3D11Texture2D),
                reinterpret_cast<void**>(backbuffer.ReleaseAndGetAddressOf())));
            m_renderTarget->setTargetTexture(backbuffer);
        }
        m_plsContext->flush({.renderTarget = m_renderTarget.get()});
    }

    void end(GLFWwindow*, std::vector<uint8_t>* pixelData = nullptr) override
    {
        flushPLSContext();
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
                VERIFY_OK(m_gpu->CreateTexture2D(&readbackTexDesc,
                                                 nullptr,
                                                 m_readbackTexture.ReleaseAndGetAddressOf()));
            }

            D3D11_MAPPED_SUBRESOURCE map;
            m_gpuContext->CopyResource(m_readbackTexture.Get(), m_renderTarget->targetTexture());
            m_gpuContext->Map(m_readbackTexture.Get(), 0, D3D11_MAP_READ, 0, &map);
            pixelData->resize(h * w * 4);
            for (uint32_t y = 0; y < h; ++y)
            {
                auto row = reinterpret_cast<const char*>(map.pData) + map.RowPitch * y;
                memcpy(pixelData->data() + (h - y - 1) * w * 4, row, w * 4);
            }
            m_gpuContext->Unmap(m_readbackTexture.Get(), 0);
        }
        m_swapchain->Present(0, 0);

        m_renderTarget->setTargetTexture(nullptr);
    }

private:
    ComPtr<IDXGIFactory2> m_d3dFactory;
    ComPtr<ID3D11Device> m_gpu;
    ComPtr<ID3D11DeviceContext> m_gpuContext;
    ComPtr<IDXGISwapChain1> m_swapchain;
    ComPtr<ID3D11Texture2D> m_readbackTexture;
    std::unique_ptr<PLSRenderContext> m_plsContext;
    rcp<PLSRenderTargetD3D> m_renderTarget;
};

std::unique_ptr<FiddleContext> FiddleContext::MakeD3DPLS(FiddleContextOptions fiddleOptions)
{
    // Create a DXGIFactory object.
    ComPtr<IDXGIFactory2> factory;
    VERIFY_OK(CreateDXGIFactory(__uuidof(IDXGIFactory2),
                                reinterpret_cast<void**>(factory.ReleaseAndGetAddressOf())));

    ComPtr<IDXGIAdapter> adapter;
    DXGI_ADAPTER_DESC adapterDesc{};
    PLSRenderContextD3DImpl::ContextOptions contextOptions;
    if (fiddleOptions.disableRasterOrdering)
    {
        contextOptions.disableRasterizerOrderedViews = true;
        // Also disable typed UAVs in atomic mode, to get more complete test coverage.
        contextOptions.disableTypedUAVLoadStore = true;
    }
    for (UINT i = 0; factory->EnumAdapters(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i)
    {
        adapter->GetDesc(&adapterDesc);
        contextOptions.isIntel = adapterDesc.VendorId == 0x163C || adapterDesc.VendorId == 0x8086 ||
                                 adapterDesc.VendorId == 0x8087;
        break;
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

    printf("D3D device: %S\n", adapterDesc.Description);

    return std::make_unique<FiddleContextD3DPLS>(std::move(factory),
                                                 std::move(gpu),
                                                 std::move(gpuContext),
                                                 contextOptions);
}

#endif
