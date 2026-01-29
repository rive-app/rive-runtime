
#include "offscreen_render_target.hpp"

#ifndef _WIN32

namespace rive_tests
{
rive::rcp<OffscreenRenderTarget> OffscreenRenderTarget::MakeD3D12(
    rive::gpu::RenderContextD3D12Impl*,
    uint32_t width,
    uint32_t height,
    bool riveRenderable)
{
    return nullptr;
}
} // namespace rive_tests

#else
#include <rive/renderer/d3d12/render_context_d3d12_impl.hpp>
#include <rive/renderer/rive_render_image.hpp>

namespace rive_tests
{
class OffscreenRenderTargetD3D12 : public OffscreenRenderTarget
{
public:
    OffscreenRenderTargetD3D12(
        rive::gpu::RenderContextD3D12Impl* renderContextImpl,
        uint32_t width,
        uint32_t height,
        bool riveRenderable) :
        m_renderTarget(
            rive::make_rcp<rive::gpu::RenderTargetD3D12>(renderContextImpl,
                                                         width,
                                                         height))
    {
        auto texture = renderContextImpl->manager()->make2DTexture(
            width,
            height,
            1,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            riveRenderable ? (D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS |
                              D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
                           : D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
            D3D12_RESOURCE_STATE_COMMON);
        m_renderTarget->setTargetTexture(texture);

        m_renderImage = rive::make_rcp<rive::RiveRenderImage>(
            renderContextImpl->adoptImageTexture(texture));
    };

    rive::RenderImage* asRenderImage() override { return m_renderImage.get(); }

    rive::gpu::RenderTarget* asRenderTarget() override
    {
        return m_renderTarget.get();
    }

private:
    rive::rcp<rive::gpu::RenderTargetD3D12> m_renderTarget;
    rive::rcp<rive::RenderImage> m_renderImage;
};

rive::rcp<OffscreenRenderTarget> OffscreenRenderTarget::MakeD3D12(
    rive::gpu::RenderContextD3D12Impl* impl,
    uint32_t width,
    uint32_t height,
    bool riveRenderable)
{
    return rive::make_rcp<OffscreenRenderTargetD3D12>(impl,
                                                      width,
                                                      height,
                                                      riveRenderable);
}

} // namespace rive_tests

#endif