/*
 * Copyright 2025 Rive
 */

#include "offscreen_render_target.hpp"

#if !defined(RIVE_WEBGPU) && !defined(RIVE_DAWN)

namespace rive_tests
{
rive::rcp<OffscreenRenderTarget> OffscreenRenderTarget::MakeWebGPU(
    rive::gpu::RenderContextWebGPUImpl*,
    uint32_t width,
    uint32_t height)
{
    return nullptr;
}
} // namespace rive_tests

#else

#include "rive/renderer/rive_render_image.hpp"
#include "rive/renderer/webgpu/render_context_webgpu_impl.hpp"

#ifdef RIVE_WAGYU
#include <webgpu/webgpu_wagyu.h>
#endif

namespace rive_tests
{
class OffscreenRenderTargetWebGPU : public rive_tests::OffscreenRenderTarget
{
public:
    OffscreenRenderTargetWebGPU(rive::gpu::RenderContextWebGPUImpl* impl,
                                wgpu::TextureFormat format,
                                uint32_t width,
                                uint32_t height) :
        m_textureTarget(
            rive::make_rcp<TextureTarget>(impl, format, width, height))
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
    class TextureTarget : public rive::gpu::RenderTargetWebGPU
    {
    public:
        TextureTarget(rive::gpu::RenderContextWebGPUImpl* impl,
                      wgpu::TextureFormat format,
                      uint32_t width,
                      uint32_t height) :
            RenderTargetWebGPU(impl->device(),
                               impl->capabilities(),
                               format,
                               width,
                               height)
        {
            wgpu::TextureDescriptor textureDesc = {
                .usage = wgpu::TextureUsage::TextureBinding |
                         wgpu::TextureUsage::RenderAttachment |
                         wgpu::TextureUsage::CopySrc,
                .dimension = wgpu::TextureDimension::e2D,
                .size = {width, height},
                .format = format,
            };
#ifdef RIVE_WAGYU
            if (impl->capabilities().plsType ==
                rive::gpu::RenderContextWebGPUImpl::PixelLocalStorageType::
                    VK_EXT_rasterization_order_attachment_access)
            {
                textureDesc.usage |= static_cast<wgpu::TextureUsage>(
                    WGPUTextureUsage_WagyuInputAttachment);
            }
#endif

            auto texture = rive::make_rcp<rive::gpu::TextureWebGPUImpl>(
                width,
                height,
                impl->device().CreateTexture(&textureDesc));
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

rive::rcp<OffscreenRenderTarget> OffscreenRenderTarget::MakeWebGPU(
    rive::gpu::RenderContextWebGPUImpl* impl,
    uint32_t width,
    uint32_t height)
{
    return rive::make_rcp<OffscreenRenderTargetWebGPU>(
        impl,
        wgpu::TextureFormat::RGBA8Unorm,
        width,
        height);
}
}; // namespace rive_tests

#endif
