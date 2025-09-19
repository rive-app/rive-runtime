#include "fiddle_context.hpp"

#ifndef RIVE_DAWN

std::unique_ptr<FiddleContext> FiddleContext::MakeDawnPLS(
    FiddleContextOptions options)
{
    return nullptr;
}

#else

#include "rive/renderer/rive_renderer.hpp"
#include "rive/renderer/webgpu/render_context_webgpu_impl.hpp"

using namespace rive;
using namespace rive::gpu;

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#define _CRT_SECURE_NO_DEPRECATE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Dawn integration based on:
// https://gist.github.com/mmozeiko/4c68b91faff8b7026e8c5e44ff810b62
static void on_device_error(WGPUDevice const* device,
                            WGPUErrorType type,
                            WGPUStringView message,
                            void* userdata1,
                            void* userdata2)
{
    fprintf(stderr, "WebGPU Error: %s\n", message.data);
    abort();
}

static void on_adapter_request_ended(WGPURequestAdapterStatus status,
                                     WGPUAdapter adapter,
                                     struct WGPUStringView message,
                                     void* userdata1,
                                     void* userdata2)
{
    if (status != WGPURequestAdapterStatus_Success)
    {
        // cannot find adapter?
        fprintf(stderr, "Failed to find an adapter: %s\n", message.data);
        abort();
    }
    else
    {
        // use first adapter provided
        WGPUAdapter* result = (WGPUAdapter*)userdata1;
        if (*result == NULL)
        {
            *result = adapter;
        }
    }
}

const WGPUTextureFormat SWAPCHAIN_FORMAT = WGPUTextureFormat_RGBA8Unorm;

class FiddleContextDawnPLS : public FiddleContext
{
public:
    FiddleContextDawnPLS(FiddleContextOptions options) : m_options(options)
    {
        // optionally use WGPUInstanceDescriptor::nextInChain for
        // WGPUDawnTogglesDescriptor with various toggles enabled or
        // disabled:
        // https://dawn.googlesource.com/dawn/+/refs/heads/main/src/dawn/native/Toggles.cpp
        WGPUInstanceDescriptor instanceDescriptor = {
            .capabilities = {.timedWaitAnyEnable = true},
        };
        m_instance =
            wgpu::Instance::Acquire(wgpuCreateInstance(&instanceDescriptor));
        assert(m_instance && "Failed to create WebGPU instance");
    }

    ~FiddleContextDawnPLS()
    {
        // Destroy in reverse order so objects go before their owners.
        if (m_currentSurfaceTextureView != nullptr)
        {
            wgpuTextureViewRelease(m_currentSurfaceTextureView);
            m_currentSurfaceTextureView = nullptr;
        }
        m_queue = nullptr;
        m_device = nullptr;
        m_adapter = nullptr;
        if (m_surfaceIsConfigured)
        {
            m_surface.Unconfigure();
        }
        m_surface = nullptr;
    }

    float dpiScale(GLFWwindow* window) const override { return 1; }

    Factory* factory() override { return m_renderContext.get(); }

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
        if (m_renderContext == nullptr)
        {
            WGPUSurfaceSourceWindowsHWND surfaceDescWin = {
                .chain = {.sType = WGPUSType_SurfaceSourceWindowsHWND},
                .hinstance = GetModuleHandle(nullptr),
                .hwnd = glfwGetWin32Window(window),
            };
            WGPUSurfaceDescriptor surfaceDesc = {
                .nextInChain = &surfaceDescWin.chain,
            };
            m_surface = wgpu::Surface::Acquire(
                wgpuInstanceCreateSurface(m_instance.Get(), &surfaceDesc));
            assert(m_surface && "Failed to create WebGPU surface");

            WGPURequestAdapterOptions options = {
                .compatibleSurface = m_surface.Get(),
                .powerPreference = WGPUPowerPreference_HighPerformance,
            };

            WGPUAdapter adapter = nullptr;
            await(wgpuInstanceRequestAdapter(
                m_instance.Get(),
                &options,
                {
                    .mode = WGPUCallbackMode_WaitAnyOnly,
                    .callback = on_adapter_request_ended,
                    .userdata1 = &adapter,
                }));
            m_adapter = wgpu::Adapter::Acquire(adapter);
            assert(m_adapter && "Failed to get WebGPU adapter");

            // can query extra details on what adapter supports:
            // wgpuAdapterEnumerateFeatures
            // wgpuAdapterGetLimits
            // wgpuAdapterGetProperties
            // wgpuAdapterHasFeature

            WGPUAdapterInfo info = {0};
            wgpuAdapterGetInfo(m_adapter.Get(), &info);
            printf("WebGPU GPU: %s\n", info.description.data);
#if 0
            const char* adapter_types[] = {
                [WGPUAdapterType_DiscreteGPU] = "Discrete GPU",
                [WGPUAdapterType_IntegratedGPU] = "Integrated GPU",
                [WGPUAdapterType_CPU] = "CPU",
                [WGPUAdapterType_Unknown] = "unknown",
            };

            printf("Device        = %.*s\n"
                   "Description   = %.*s\n"
                   "Vendor        = %.*s\n"
                   "Architecture  = %.*s\n"
                   "Adapter Type  = %s\n",
                   (int)info.device.length,
                   info.device.data,
                   (int)info.description.length,
                   info.description.data,
                   (int)info.vendor.length,
                   info.vendor.data,
                   (int)info.architecture.length,
                   info.architecture.data,
                   adapter_types[info.adapterType]);
#endif

            // if you want to be sure device will support things you'll use,
            // you can specify requirements here:

            // WGPUSupportedLimits supported = { 0 };
            // wgpuAdapterGetLimits(adapter, &supported);

            // supported.limits.maxTextureDimension2D = kTextureWidth;
            // supported.limits.maxBindGroups = 1;
            // supported.limits.maxBindingsPerBindGroup = 3; // uniform
            // buffer for vertex shader, and texture + sampler for fragment
            // supported.limits.maxSampledTexturesPerShaderStage = 1;
            // supported.limits.maxSamplersPerShaderStage = 1;
            // supported.limits.maxUniformBuffersPerShaderStage = 1;
            // supported.limits.maxUniformBufferBindingSize = 4 * 4 *
            // sizeof(float);
            // // 4x4 matrix supported.limits.maxVertexBuffers = 1;
            // supported.limits.maxBufferSize = sizeof(kVertexData);
            // supported.limits.maxVertexAttributes = 3; // pos, texcoord,
            // color supported.limits.maxVertexBufferArrayStride =
            // kVertexStride; supported.limits.maxColorAttachments = 1;

            WGPUDeviceDescriptor deviceDesc = {
                // notify on errors
                .uncapturedErrorCallbackInfo = {.callback = &on_device_error},

                // extra features:
                // https://dawn.googlesource.com/dawn/+/refs/heads/main/src/dawn/native/Features.cpp
                //.requiredFeaturesCount = n
                //.requiredFeatures = (WGPUFeatureName[]) { ... }
                //.requiredLimits = &(WGPURequiredLimits) { .limits =
                // supported.limits },
            };

            m_device = wgpu::Device::Acquire(
                wgpuAdapterCreateDevice(m_adapter.Get(), &deviceDesc));
            assert(m_device && "Failed to create WebGPU device");

            // default device queue
            m_queue = m_device.GetQueue();

            m_renderContext = RenderContextWebGPUImpl::MakeContext(
                m_adapter,
                m_device,
                m_queue,
                RenderContextWebGPUImpl::ContextOptions());
        }

        if (m_surfaceIsConfigured)
        {
            // release old swap chain
            m_surface.Unconfigure();
            m_surfaceIsConfigured = false;
        }

        WGPUSurfaceConfiguration surfaceConfig = {
            .device = m_device.Get(),
            .format = SWAPCHAIN_FORMAT,
            .usage =
                WGPUTextureUsage_CopySrc | WGPUTextureUsage_RenderAttachment,
            // .alphaMode = WGPUCompositeAlphaMode_Premultiplied,
            .width = static_cast<uint32_t>(width),
            .height = static_cast<uint32_t>(height),
            .presentMode = WGPUPresentMode_Immediate,
        };

        wgpuSurfaceConfigure(m_surface.Get(), &surfaceConfig);
        m_surfaceIsConfigured = true;

        m_renderTarget =
            m_renderContext->static_impl_cast<RenderContextWebGPUImpl>()
                ->makeRenderTarget(wgpu::TextureFormat::RGBA8Unorm,
                                   width,
                                   height);
        m_pixelReadBuff = {};
    }

    void toggleZoomWindow() override {}

    std::unique_ptr<Renderer> makeRenderer(int width, int height) override
    {
        return std::make_unique<RiveRenderer>(m_renderContext.get());
    }

    void begin(const RenderContext::FrameDescriptor& frameDescriptor) override
    {
        wgpuSurfaceGetCurrentTexture(m_surface.Get(), &m_currentSurfaceTexture);
        assert(wgpuTextureGetWidth(m_currentSurfaceTexture.texture) ==
               m_renderTarget->width());
        assert(wgpuTextureGetHeight(m_currentSurfaceTexture.texture) ==
               m_renderTarget->height());
        WGPUTextureViewDescriptor textureViewDesc = {
            .format = SWAPCHAIN_FORMAT,
            .dimension = WGPUTextureViewDimension_2D,
            .baseMipLevel = 0,
            .mipLevelCount = 1,
            .baseArrayLayer = 0,
            .arrayLayerCount = 1,
            .aspect = WGPUTextureAspect_All,
            .usage =
                WGPUTextureUsage_CopySrc | WGPUTextureUsage_RenderAttachment,
        };
        m_currentSurfaceTextureView =
            wgpuTextureCreateView(m_currentSurfaceTexture.texture,
                                  &textureViewDesc);
        m_renderTarget->setTargetTextureView(m_currentSurfaceTextureView,
                                             m_currentSurfaceTexture.texture);
        m_renderContext->beginFrame(std::move(frameDescriptor));
    }

    void flushPLSContext(RenderTarget* offscreenRenderTarget) final
    {
        wgpu::CommandEncoder encoder = m_device.CreateCommandEncoder();
        m_renderContext->flush({
            .renderTarget = offscreenRenderTarget != nullptr
                                ? offscreenRenderTarget
                                : m_renderTarget.get(),
            .externalCommandBuffer = encoder.Get(),
        });
        wgpu::CommandBuffer commands = encoder.Finish();
        m_queue.Submit(1, &commands);
    }

    void end(GLFWwindow* window, std::vector<uint8_t>* pixelData) final
    {
        flushPLSContext(nullptr);

        if (pixelData != nullptr)
        {
            // Read back pixels from the framebuffer!
            uint32_t w = m_renderTarget->width();
            uint32_t h = m_renderTarget->height();
            uint32_t rowBytesInReadBuff =
                math::round_up_to_multiple_of<256>(w * 4);

            // Create a buffer to receive the pixels.
            if (!m_pixelReadBuff)
            {
                wgpu::BufferDescriptor buffDesc{
                    .usage =
                        wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst,
                    .size = h * rowBytesInReadBuff,
                };
                m_pixelReadBuff = m_device.CreateBuffer(&buffDesc);
            }
            assert(m_pixelReadBuff.GetSize() == h * rowBytesInReadBuff);

            // Blit the framebuffer into m_pixelReadBuff.
            wgpu::CommandEncoder readEncoder =
                m_device.CreateCommandEncoder(NULL);
            wgpu::TexelCopyTextureInfo srcTexture = {
                .texture = m_currentSurfaceTexture.texture,
                .origin = {0, 0, 0},
            };
            wgpu::TexelCopyBufferInfo dstBuffer = {
                .layout =
                    {
                        .offset = 0,
                        .bytesPerRow = rowBytesInReadBuff,
                    },
                .buffer = m_pixelReadBuff,
            };
            wgpu::Extent3D copySize = {
                .width = w,
                .height = h,
            };
            readEncoder.CopyTextureToBuffer(&srcTexture, &dstBuffer, &copySize);

            wgpu::CommandBuffer commands = readEncoder.Finish(NULL);
            m_queue.Submit(1, &commands);

            // Request a mapping of m_pixelReadBuff and wait for it to complete.
            await(m_pixelReadBuff.MapAsync(
                wgpu::MapMode::Read,
                0,
                h * rowBytesInReadBuff,
                wgpu::CallbackMode::WaitAnyOnly,
                [](wgpu::MapAsyncStatus status, wgpu::StringView message) {
                    if (status != wgpu::MapAsyncStatus::Success)
                    {
                        fprintf(stderr,
                                "failed to map m_pixelReadBuff: %s\n",
                                message.data);
                        abort();
                    }
                }));

            // Copy the image data from m_pixelReadBuff to pixelData.
            pixelData->resize(h * w * 4);
            const uint8_t* pixelReadBuffData = reinterpret_cast<const uint8_t*>(
                m_pixelReadBuff.GetConstMappedRange());
            for (size_t y = 0; y < h; ++y)
            {
                // Flip Y.
                const uint8_t* src =
                    &pixelReadBuffData[(h - y - 1) * rowBytesInReadBuff];
                size_t row = y * w * 4;
                memcpy(pixelData->data() + row, src, w * 4);
            }
            m_pixelReadBuff.Unmap();
        }

        wgpuTextureViewRelease(m_currentSurfaceTextureView);
        m_currentSurfaceTextureView = nullptr;

        m_surface.Present();
    }

    void tick() override { wgpuInstanceProcessEvents(m_instance.Get()); }

private:
    void await(WGPUFuture future)
    {
        WGPUFutureWaitInfo futureWait = {future};
        if (wgpuInstanceWaitAny(m_instance.Get(), 1, &futureWait, -1) !=
            WGPUWaitStatus_Success)
        {
            fprintf(stderr, "wgpuInstanceWaitAny failed.");
            abort();
        }
    }

    const FiddleContextOptions m_options;

    wgpu::Instance m_instance = nullptr;
    wgpu::Surface m_surface = nullptr;
    wgpu::Adapter m_adapter = nullptr;
    wgpu::Device m_device = nullptr;
    wgpu::Queue m_queue = nullptr;
    bool m_surfaceIsConfigured = false;

    WGPUSurfaceTexture m_currentSurfaceTexture = {};
    WGPUTextureView m_currentSurfaceTextureView = {};
    std::unique_ptr<RenderContext> m_renderContext;
    rcp<RenderTargetWebGPU> m_renderTarget;
    wgpu::Buffer m_pixelReadBuff;
};

std::unique_ptr<FiddleContext> FiddleContext::MakeDawnPLS(
    FiddleContextOptions options)
{
    return std::make_unique<FiddleContextDawnPLS>(options);
}

#endif
