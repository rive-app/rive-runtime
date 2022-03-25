#ifndef _RIVE_SKIA_RENDERER_HPP_
#define _RIVE_SKIA_RENDERER_HPP_

#include "SkCanvas.h"
#include "SkPaint.h"
#include "SkPath.h"
#include "SkImage.h"
#include "rive/renderer.hpp"
#include <vector>

namespace rive {
    class SkiaRenderPath : public RenderPath {
    private:
        SkPath m_Path;

    public:
        SkiaRenderPath() {}
        SkiaRenderPath(SkPath&& path) : m_Path(std::move(path)) {}

        const SkPath& path() const { return m_Path; }
        void reset() override;
        void addRenderPath(RenderPath* path, const Mat2D& transform) override;
        void fillRule(FillRule value) override;
        void moveTo(float x, float y) override;
        void lineTo(float x, float y) override;
        void cubicTo(float ox, float oy, float ix, float iy, float x, float y) override;
        virtual void close() override;
    };

    class SkiaRenderPaint : public RenderPaint {
    private:
        SkPaint m_Paint;

    public:
        const SkPaint& paint() const { return m_Paint; }
        SkiaRenderPaint();
        void style(RenderPaintStyle style) override;
        void color(unsigned int value) override;
        void thickness(float value) override;
        void join(StrokeJoin value) override;
        void cap(StrokeCap value) override;
        void blendMode(BlendMode value) override;
        void shader(rcp<RenderShader>) override;
    };

    class SkiaRenderImage : public RenderImage {
    private:
        sk_sp<SkImage> m_SkImage;

    public:
        sk_sp<SkImage> skImage() const { return m_SkImage; };
        bool decode(Span<const uint8_t>) override;
        rcp<RenderShader>
        makeShader(RenderTileMode tx, RenderTileMode ty, const Mat2D* localMatrix) const override;
    };

    class SkiaRenderer : public Renderer {
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
                           BlendMode,
                           float opacity) override;
    };
} // namespace rive
#endif
