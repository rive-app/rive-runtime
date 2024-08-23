/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_SKIA_RENDERER_HPP_
#define _RIVE_SKIA_RENDERER_HPP_

#include "rive/renderer.hpp"

class SkCanvas;

namespace rive
{
class SkiaRenderer : public Renderer
{
protected:
    SkCanvas* m_Canvas;

public:
    SkiaRenderer(SkCanvas* canvas) : m_Canvas(canvas) {}
    void save() override;
    void restore() override;
    void transform(const Mat2D& transform) override;
    void clipPath(RenderPath* path) override;
    void drawPath(RenderPath* path, RenderPaint* paint) override;
    void drawImage(const RenderImage*, BlendMode, float opacity) override;
    void drawImageMesh(const RenderImage*,
                       rcp<RenderBuffer> vertices_f32,
                       rcp<RenderBuffer> uvCoords_f32,
                       rcp<RenderBuffer> indices_u16,
                       uint32_t vertexCount,
                       uint32_t indexCount,
                       BlendMode,
                       float opacity) override;
};
} // namespace rive
#endif
