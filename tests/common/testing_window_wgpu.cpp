/*
 * Copyright 2025 Rive
 */

#include "testing_window.hpp"

#if !defined(RIVE_WEBGPU) || (RIVE_WEBGPU < 2)

TestingWindow* TestingWindow::MakeWGPU(const BackendParams&) { return nullptr; }

#else

#include "common/offscreen_render_target.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "rive/renderer/rive_render_image.hpp"
#include "rive/renderer/webgpu/render_context_webgpu_impl.hpp"

#ifdef RIVE_WAGYU
#include <webgpu/webgpu_wagyu.h>
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#endif

namespace rive::gpu
{
static const char* wgpu_backend_name(wgpu::BackendType backendType)
{
    switch (backendType)
    {
        case wgpu::BackendType::Undefined:
            return "<unknown backend>";
        case wgpu::BackendType::Null:
            return "<null backend>";
        case wgpu::BackendType::WebGPU:
            return "WebGPU";
        case wgpu::BackendType::D3D11:
            return "D3D11";
        case wgpu::BackendType::D3D12:
            return "D3D12";
        case wgpu::BackendType::Metal:
            return "Metal";
        case wgpu::BackendType::Vulkan:
            return "Vulkan";
        case wgpu::BackendType::OpenGL:
            return "OpenGL";
        case wgpu::BackendType::OpenGLES:
            return "OpenGLES";
    }
    RIVE_UNREACHABLE();
}

static const char* pls_impl_name(
    const RenderContextWebGPUImpl::Capabilities& capabilities)
{
#ifdef RIVE_WAGYU
    switch (capabilities.plsType)
    {
        case RenderContextWebGPUImpl::PixelLocalStorageType::
            GL_EXT_shader_pixel_local_storage:
            return "GL_EXT_shader_pixel_local_storage";
        case RenderContextWebGPUImpl::PixelLocalStorageType::
            VK_EXT_rasterization_order_attachment_access:
            return "VK_EXT_rasterization_order_attachment_access";
        case RenderContextWebGPUImpl::PixelLocalStorageType::none:
            break;
    }
#endif
    return "<no pixel local storage>";
}

class OffscreenRenderTargetWGPU : public rive_tests::OffscreenRenderTarget
{
public:
    OffscreenRenderTargetWGPU(
        rive::gpu::RenderContextWebGPUImpl* renderContextImpl,
        wgpu::TextureFormat format,
        uint32_t width,
        uint32_t height) :
        m_textureTarget(rive::make_rcp<TextureTarget>(renderContextImpl,
                                                      format,
                                                      width,
                                                      height))
    {}

    rive::RenderImage* asRenderImage() override
    {
        return m_textureTarget->renderImage();
    }

    rive::gpu::RenderTarget* asRenderTarget() override
    {
        return m_textureTarget.get();
    }

private:
    class TextureTarget : public RenderTargetWebGPU
    {
    public:
        TextureTarget(rive::gpu::RenderContextWebGPUImpl* renderContextImpl,
                      wgpu::TextureFormat format,
                      uint32_t width,
                      uint32_t height) :
            RenderTargetWebGPU(renderContextImpl->device(),
                               renderContextImpl->capabilities(),
                               format,
                               width,
                               height)
        {
            wgpu::TextureDescriptor textureDesc = {
                .usage = wgpu::TextureUsage::TextureBinding |
                         wgpu::TextureUsage::RenderAttachment,
                .dimension = wgpu::TextureDimension::e2D,
                .size = {width, height},
                .format = format,
            };
#ifdef RIVE_WAGYU
            if (renderContextImpl->capabilities().plsType ==
                RenderContextWebGPUImpl::PixelLocalStorageType::
                    VK_EXT_rasterization_order_attachment_access)
            {
                textureDesc.usage |= static_cast<wgpu::TextureUsage>(
                    WGPUTextureUsage_WagyuInputAttachment);
            }
#endif

            auto texture = make_rcp<TextureWebGPUImpl>(
                width,
                height,
                renderContextImpl->device().CreateTexture(&textureDesc));
            setTargetTextureView(texture->textureView(), texture->texture());
            m_renderImage =
                rive::make_rcp<rive::RiveRenderImage>(std::move(texture));
        }

        rive::RiveRenderImage* renderImage() const
        {
            return m_renderImage.get();
        }

    private:
        rive::rcp<rive::RiveRenderImage> m_renderImage;
    };

    rive::rcp<TextureTarget> m_textureTarget;
};

class TestingWindowWGPU : public TestingWindow
{
public:
    TestingWindowWGPU(const BackendParams& backendParams) :
        m_backendParams(backendParams)
    {
        m_instance = wgpu::CreateInstance(nullptr);
        assert(m_instance);

        m_instance.RequestAdapter(
            {},
            wgpu::CallbackMode::AllowSpontaneous,
            [](wgpu::RequestAdapterStatus status,
               wgpu::Adapter adapter,
               wgpu::StringView message,
               TestingWindowWGPU* this_) {
                assert(status == wgpu::RequestAdapterStatus::Success);
                this_->m_adapter = adapter;
            },
            this);
        while (!m_adapter)
        {
            emscripten_sleep(1);
        }

        m_adapter.RequestDevice(
            {},
            wgpu::CallbackMode::AllowSpontaneous,
            [](wgpu::RequestDeviceStatus status,
               wgpu::Device device,
               wgpu::StringView message,
               TestingWindowWGPU* this_) {
                assert(status == wgpu::RequestDeviceStatus::Success);
                this_->m_device = device;
            },
            this);
        while (!m_device)
        {
            emscripten_sleep(1);
        }

        m_queue = m_device.GetQueue();
        assert(m_queue);

        {
            wgpu::EmscriptenSurfaceSourceCanvasHTMLSelector htmlSelector;
            htmlSelector.selector = "#canvas";

            wgpu::SurfaceDescriptor surfaceDesc = {
                .nextInChain = &htmlSelector,
            };

            m_surface = m_instance.CreateSurface(&surfaceDesc);
            assert(m_surface);

            int w, h;
            emscripten_get_canvas_element_size("#canvas", &w, &h);
            m_width = w;
            m_height = h;
        }

        {
            wgpu::SurfaceCapabilities capabilities;
            m_surface.GetCapabilities(m_adapter, &capabilities);
            assert(capabilities.formatCount > 0);
            m_format = capabilities.formats[0];
            assert(m_format != wgpu::TextureFormat::Undefined);
            if (m_format != wgpu::TextureFormat::RGBA8Unorm &&
                m_format != wgpu::TextureFormat::BGRA8Unorm)
            {
                m_format = wgpu::TextureFormat::RGBA8Unorm;
            }
        }

        {
            wgpu::SurfaceConfiguration conf = {
                .device = m_device,
                .format = m_format,
            };
            m_surface.Configure(&conf);
        }

        RenderContextWebGPUImpl::ContextOptions contextOptions;
        m_renderContext = RenderContextWebGPUImpl::MakeContext(m_adapter,
                                                               m_device,
                                                               m_queue,
                                                               contextOptions);

        wgpu::AdapterInfo adapterInfo;
        m_adapter.GetInfo(&adapterInfo);
        printf("==== WGPU device: %s %s %s (%s, %s) ====\n",
               adapterInfo.vendor.data,
               adapterInfo.device.data,
               adapterInfo.description.data,
               wgpu_backend_name(impl()->capabilities().backendType),
               pls_impl_name(impl()->capabilities()));
    }

    rive::Factory* factory() override { return m_renderContext.get(); }

    rive::gpu::RenderContext* renderContext() const override
    {
        return m_renderContext.get();
    }

    rcp<rive_tests::OffscreenRenderTarget> makeOffscreenRenderTarget(
        uint32_t width,
        uint32_t height,
        bool riveRenderable) const override
    {
        return make_rcp<OffscreenRenderTargetWGPU>(
            impl(),
            // The format has no impact on whether Rive can render directly to
            // the texture, but switch on that flag to test both formats.
            //
            // NOTE: The WebGPU backend currently has no code to handle
            // non-renderable textures. GL_EXT_shader_pixel_local_storage has no
            // such restrictions and
            // VK_EXT_rasterization_order_attachment_access mode requires the
            // texture to support input attachments.
            riveRenderable ? wgpu::TextureFormat::RGBA8Unorm
                           : wgpu::TextureFormat::BGRA8Unorm,
            width,
            height);
    }

    void resize(int width, int height) override
    {
        if (m_width != width || m_height != height)
        {
            m_overflowTexture = {};
            m_overflowTextureView = {};
            m_pixelReadBuff = {};
            TestingWindow::resize(width, height);
        }
    }

    std::unique_ptr<rive::Renderer> beginFrame(
        const FrameOptions& options) override
    {
        assert(m_currentCanvasTexture == nullptr);

        wgpu::SurfaceTexture surfaceTexture;
        m_surface.GetCurrentTexture(&surfaceTexture);

        m_currentCanvasTexture = surfaceTexture.texture;
        uint32_t surfaceWidth = m_currentCanvasTexture.GetWidth();
        uint32_t surfaceHeight = m_currentCanvasTexture.GetHeight();

        wgpu::TextureViewDescriptor textureViewDesc = {
            .format = m_format,
            .dimension = wgpu::TextureViewDimension::e2D,
        };

        m_currentCanvasTextureView =
            m_currentCanvasTexture.CreateView(&textureViewDesc);

        if (surfaceWidth < m_width || surfaceHeight < m_height)
        {
            if (!m_overflowTexture)
            {
                wgpu::TextureDescriptor overflowTextureDesc = {
                    .usage = wgpu::TextureUsage::RenderAttachment |
                             wgpu::TextureUsage::CopySrc,
                    .dimension = wgpu::TextureDimension::e2D,
                    .size = {m_width, m_height},
                    .format = m_format,
                };

                m_overflowTexture =
                    m_device.CreateTexture(&overflowTextureDesc);
                m_overflowTextureView = m_overflowTexture.CreateView();
            }
            assert(m_overflowTexture.GetWidth() == m_width);
            assert(m_overflowTexture.GetHeight() == m_height);

            m_renderTarget =
                m_renderContext->static_impl_cast<RenderContextWebGPUImpl>()
                    ->makeRenderTarget(m_format, m_width, m_height);
            m_renderTarget->setTargetTextureView(m_overflowTextureView,
                                                 m_overflowTexture);
        }
        else
        {
            m_renderTarget =
                m_renderContext->static_impl_cast<RenderContextWebGPUImpl>()
                    ->makeRenderTarget(m_format, surfaceWidth, surfaceHeight);
            m_renderTarget->setTargetTextureView(m_currentCanvasTextureView,
                                                 m_currentCanvasTexture);
        }

        rive::gpu::RenderContext::FrameDescriptor frameDescriptor = {
            .renderTargetWidth = surfaceWidth,
            .renderTargetHeight = surfaceHeight,
            .loadAction = options.doClear
                              ? rive::gpu::LoadAction::clear
                              : rive::gpu::LoadAction::preserveRenderTarget,
            .clearColor = options.clearColor,
            .msaaSampleCount = m_backendParams.msaa ? 4u : 0u,
            .disableRasterOrdering = options.disableRasterOrdering,
            .wireframe = options.wireframe,
            .clockwiseFillOverride =
                m_backendParams.clockwise || options.clockwiseFillOverride,
            .synthesizedFailureType = options.synthesizedFailureType,
        };
        m_renderContext->beginFrame(frameDescriptor);
        return std::make_unique<RiveRenderer>(m_renderContext.get());
    }

    void flushPLSContext(RenderTarget* offscreenRenderTarget) final
    {
        wgpu::CommandEncoder encoder = m_device.CreateCommandEncoder();
        m_renderContext->flush({
            .renderTarget = m_renderTarget.get(),
            .externalCommandBuffer = encoder.Get(),
        });
        wgpu::CommandBuffer commands = encoder.Finish();
        m_queue.Submit(1, &commands);
    }

    void endFrame(std::vector<uint8_t>* pixelData) override
    {
        flushPLSContext(nullptr);
        assert(m_currentCanvasTexture != nullptr);

        if (m_overflowTexture)
        {
            // Blit the overflow texture back to the canvas.
            wgpu::CommandEncoder encoder = m_device.CreateCommandEncoder();

            wgpu::TexelCopyTextureInfo src = {
                .texture = m_overflowTexture,
                .origin = {0, 0, 0},
            };

            wgpu::TexelCopyTextureInfo dst = {
                .texture = m_currentCanvasTexture,
                .origin = {0, 0, 0},
            };

            wgpu::Extent3D copySize{
                std::min(m_width, m_currentCanvasTexture.GetWidth()),
                std::min(m_height, m_currentCanvasTexture.GetHeight()),
            };

            encoder.CopyTextureToTexture(&src, &dst, &copySize);

            wgpu::CommandBuffer commands = encoder.Finish();
            m_queue.Submit(1, &commands);
        }

        if (pixelData != nullptr)
        {
            assert(m_format == wgpu::TextureFormat::RGBA8Unorm ||
                   m_format == wgpu::TextureFormat::BGRA8Unorm);
            bool invertY = false;
#ifdef RIVE_WAGYU
            invertY =
                impl()->capabilities().backendType ==
                    wgpu::BackendType::OpenGLES &&
                wgpuWagyuTextureIsSwapchain(m_currentCanvasTexture.Get()) &&
                m_overflowTexture == nullptr;
#endif
            const uint32_t rowBytesInReadBuff =
                math::round_up_to_multiple_of<256>(m_width * 4);

            // Create a buffer to receive the pixels.
            if (!m_pixelReadBuff)
            {
                wgpu::BufferDescriptor buffDesc{
                    .usage =
                        wgpu::BufferUsage::MapRead | wgpu::BufferUsage::CopyDst,
                    .size = m_height * rowBytesInReadBuff,
                };
                m_pixelReadBuff = m_device.CreateBuffer(&buffDesc);
            }
            assert(m_pixelReadBuff.GetSize() == m_height * rowBytesInReadBuff);

            // Blit the framebuffer into m_pixelReadBuff.
            wgpu::CommandEncoder readEncoder = m_device.CreateCommandEncoder();
            wgpu::TexelCopyTextureInfo srcTexture = {
                .texture = m_overflowTexture != nullptr
                               ? m_overflowTexture
                               : m_currentCanvasTexture,
                .origin = {0,
                           invertY ? m_renderTarget->height() - m_height : 0,
                           0},
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
                .width = m_width,
                .height = m_height,
            };
            readEncoder.CopyTextureToBuffer(&srcTexture, &dstBuffer, &copySize);

            wgpu::CommandBuffer commands = readEncoder.Finish(NULL);
            m_queue.Submit(1, &commands);

            {
                // Map m_pixelReadBuff.
                bool mappingFinished = false;
                m_pixelReadBuff.MapAsync(
                    wgpu::MapMode::Read,
                    0,
                    m_height * rowBytesInReadBuff,
                    wgpu::CallbackMode::AllowSpontaneous,
                    [](wgpu::MapAsyncStatus status,
                       wgpu::StringView message,
                       bool* mappingFinished) {
                        if (status != wgpu::MapAsyncStatus::Success)
                        {
                            fprintf(stderr,
                                    "failed to map m_pixelReadBuff: %s\n",
                                    message.data);
                            abort();
                        }
                        *mappingFinished = true;
                    },
                    &mappingFinished);
                while (!mappingFinished)
                {
                    emscripten_sleep(1);
                }
            }

            // Copy the image data from m_pixelReadBuff to pixelData.
            const size_t rowBytesInDst = m_width * 4;
            pixelData->resize(m_height * rowBytesInDst);
            const uint8_t* pixelReadBuffData = reinterpret_cast<const uint8_t*>(
                m_pixelReadBuff.GetConstMappedRange());
            for (size_t y = 0; y < m_height; ++y)
            {
                const uint8_t* src;
                if (invertY)
                {
                    src = &pixelReadBuffData[y * rowBytesInReadBuff];
                }
                else
                {
                    src = &pixelReadBuffData[(m_height - y - 1) *
                                             rowBytesInReadBuff];
                }
                uint8_t* dst = &(*pixelData)[y * rowBytesInDst];
                if (m_format == wgpu::TextureFormat::RGBA8Unorm)
                {
                    memcpy(dst, src, rowBytesInDst);
                }
                else
                {
                    assert(m_format == wgpu::TextureFormat::BGRA8Unorm);
                    for (size_t x = 0; x < rowBytesInDst; x += 4)
                    {
                        // BGBRA -> RGBA.
                        dst[x + 0] = src[x + 2];
                        dst[x + 1] = src[x + 1];
                        dst[x + 2] = src[x + 0];
                        dst[x + 3] = src[x + 3];
                    }
                }
            }
            m_pixelReadBuff.Unmap();
        }

        m_currentCanvasTextureView = {};
        m_currentCanvasTexture = {};
    }

private:
    RenderContextWebGPUImpl* impl() const
    {
        return m_renderContext->static_impl_cast<RenderContextWebGPUImpl>();
    }

    const BackendParams m_backendParams;

    wgpu::Instance m_instance = nullptr;
    wgpu::Adapter m_adapter;
    wgpu::Device m_device;
    wgpu::Surface m_surface;
    wgpu::TextureFormat m_format = wgpu::TextureFormat::Undefined;
    wgpu::Queue m_queue;
    wgpu::Texture m_currentCanvasTexture;
    wgpu::TextureView m_currentCanvasTextureView;
    wgpu::Texture m_overflowTexture;
    wgpu::TextureView m_overflowTextureView;
    wgpu::Buffer m_pixelReadBuff;

    std::unique_ptr<RenderContext> m_renderContext;
    rcp<RenderTargetWebGPU> m_renderTarget;
};
}; // namespace rive::gpu

TestingWindow* TestingWindow::MakeWGPU(const BackendParams& backendParams)
{
    return new rive::gpu::TestingWindowWGPU(backendParams);
}

#endif
