/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/refcnt.hpp"
#include "rive/renderer/render_target.hpp"
#include "rive/renderer/rive_render_image.hpp"

namespace rive::gpu
{

// A GPU texture that can be used as both a render target (for rendering into)
// and a render image (for compositing into Rive draws). Enables off-screen
// rendering for 3D content (Ore), cached 2D content, or any render-to-texture
// use case.
class RenderCanvas : public RefCnt<RenderCanvas>
{
public:
    RenderCanvas(rcp<RiveRenderImage> image, rcp<RenderTarget> target) :
        m_renderImage(std::move(image)), m_renderTarget(std::move(target))
    {}

    uint32_t width() const { return m_renderTarget->width(); }
    uint32_t height() const { return m_renderTarget->height(); }

    // Use as a RenderImage for compositing into Rive draws.
    RiveRenderImage* renderImage() { return m_renderImage.get(); }

    // Use as a RenderTarget for rendering into this texture.
    RenderTarget* renderTarget() { return m_renderTarget.get(); }

private:
    rcp<RiveRenderImage> m_renderImage;
    rcp<RenderTarget> m_renderTarget;
};

} // namespace rive::gpu
