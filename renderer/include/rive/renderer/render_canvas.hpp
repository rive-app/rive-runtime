/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/refcnt.hpp"
#include "rive/renderer/render_target.hpp"
#include "rive/renderer/rive_render_image.hpp"

namespace rive::gpu
{

// The image half of a RenderCanvas. A recording refers to a canvas by this
// object, so the identity is fixed at creation while the texture waits for the
// device that ends up replaying.
class RenderCanvasImage : public RiveRenderImage
{
public:
    RenderCanvasImage(uint32_t width, uint32_t height) :
        RiveRenderImage(width, height)
    {}

    using RiveRenderImage::resetTexture;
};

// A GPU texture that can be used as both a render target (for rendering into)
// and a render image (for compositing into Rive draws). Enables off-screen
// rendering for 3D content (Ore), cached 2D content, or any render-to-texture
// use case.
//
// Construction allocates nothing. RenderContextImpl::ensureCanvasBacking
// installs the pixels, which is what lets a recording hand out a canvas with
// no device in reach and leave the allocation to whoever replays it.
class RenderCanvas : public RefCnt<RenderCanvas>
{
public:
    RenderCanvas(uint32_t width, uint32_t height) :
        m_renderImage(make_rcp<RenderCanvasImage>(width, height)),
        m_width(width),
        m_height(height)
    {}

    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }

    bool isBacked() const { return m_renderTarget != nullptr; }

    void setBacking(rcp<Texture> texture, rcp<RenderTarget> renderTarget)
    {
        assert(!isBacked());
        m_renderImage->resetTexture(std::move(texture));
        m_renderTarget = std::move(renderTarget);
    }

    // Use as a RenderImage for compositing into Rive draws.
    RiveRenderImage* renderImage() { return m_renderImage.get(); }

    // Use as a RenderTarget for rendering into this texture.
    RenderTarget* renderTarget() { return m_renderTarget.get(); }

private:
    rcp<RenderCanvasImage> m_renderImage;
    rcp<RenderTarget> m_renderTarget;
    uint32_t m_width;
    uint32_t m_height;
};

} // namespace rive::gpu
