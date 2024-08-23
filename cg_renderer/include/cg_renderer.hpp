/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_CG_RENDERER_HPP_
#define _RIVE_CG_RENDERER_HPP_

#include "rive/renderer.hpp"
#include "utils/auto_cf.hpp"

#if defined(RIVE_BUILD_FOR_OSX)
#include <ApplicationServices/ApplicationServices.h>
#elif defined(RIVE_BUILD_FOR_IOS)
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#endif

namespace rive
{
class CGRenderer : public Renderer
{
protected:
    CGContextRef m_ctx;

public:
    CGRenderer(CGContextRef ctx, int width, int height);
    ~CGRenderer() override;

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

    static AutoCF<CGImageRef> DecodeToCGImage(Span<const uint8_t>);
    static AutoCF<CGImageRef> FlipCGImageInY(AutoCF<CGImageRef>);
};
} // namespace rive
#endif
