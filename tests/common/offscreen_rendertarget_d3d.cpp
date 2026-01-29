
#include "offscreen_render_target.hpp"

#ifndef _WIN32

namespace rive_tests
{
rive::rcp<OffscreenRenderTarget> OffscreenRenderTarget::MakeD3D(
    rive::gpu::RenderContextD3DImpl*,
    uint32_t width,
    uint32_t height,
    bool riveRenderable)
{
    return nullptr;
}
} // namespace rive_tests

#else
#include <rive/renderer/d3d11/render_context_d3d_impl.hpp>
#include <rive/renderer/rive_render_image.hpp>

namespace rive_tests
{
class OffscreenRenderTargetD3D : public OffscreenRenderTarget
{
public:
    OffscreenRenderTargetD3D(rive::gpu::RenderContextD3DImpl* renderContextImpl,
                             uint32_t width,
                             uint32_t height,
                             bool riveRenderable) :
        m_renderTarget(
            rive::make_rcp<rive::gpu::RenderTargetD3D>(renderContextImpl,
                                                       width,
                                                       height))
    {
        auto texture = renderContextImpl->makeSimple2DTexture(
            DXGI_FORMAT_R8G8B8A8_UNORM,
            width,
            height,
            1,
            riveRenderable
                ? (D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_RENDER_TARGET |
                   D3D11_BIND_SHADER_RESOURCE)
                : D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE);
        m_renderTarget->setTargetTexture(texture);

        m_renderImage = rive::make_rcp<rive::RiveRenderImage>(
            renderContextImpl->adoptImageTexture(texture, width, height));
    };

    rive::RenderImage* asRenderImage() override { return m_renderImage.get(); }

    rive::gpu::RenderTarget* asRenderTarget() override
    {
        return m_renderTarget.get();
    }

private:
    rive::rcp<rive::gpu::RenderTargetD3D> m_renderTarget;
    rive::rcp<rive::RiveRenderImage> m_renderImage;
};

rive::rcp<OffscreenRenderTarget> OffscreenRenderTarget::MakeD3D(
    rive::gpu::RenderContextD3DImpl* impl,
    uint32_t width,
    uint32_t height,
    bool riveRenderable)
{
    return rive::make_rcp<OffscreenRenderTargetD3D>(impl,
                                                    width,
                                                    height,
                                                    riveRenderable);
}

} // namespace rive_tests

#endif