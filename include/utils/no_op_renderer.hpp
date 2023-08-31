/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_NOOP_RENDERER_HPP_
#define _RIVE_NOOP_RENDERER_HPP_

#include "rive/renderer.hpp"

namespace rive
{

class NoOpRenderer : public Renderer
{
public:
    void save() override {}
    void restore() override {}
    void transform(const Mat2D&) override {}
    void drawPath(RenderPath* path, RenderPaint* paint) override {}
    void clipPath(RenderPath* path) override {}
    void drawImage(const RenderImage*, BlendMode, float) override {}
    void drawImageMesh(const RenderImage*,
                       rcp<RenderBuffer>,
                       rcp<RenderBuffer>,
                       rcp<RenderBuffer>,
                       uint32_t vertexCount,
                       uint32_t indexCount,
                       BlendMode,
                       float) override
    {}
};

} // namespace rive

#endif
