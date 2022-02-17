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
        const SkPath& path() const { return m_Path; }
        void reset() override;
        void addRenderPath(RenderPath* path, const Mat2D& transform) override;
        void fillRule(FillRule value) override;
        void moveTo(float x, float y) override;
        void lineTo(float x, float y) override;
        void cubicTo(
            float ox, float oy, float ix, float iy, float x, float y) override;
        virtual void close() override;
    };

    struct GradientStop {
        unsigned int color;
        float stop;
        GradientStop(unsigned int color, float stop) :
            color(color), stop(stop) {}
    };

    class SkiaGradientBuilder {
    public:
        std::vector<GradientStop> stops;
        float sx, sy, ex, ey;
        virtual ~SkiaGradientBuilder() {}
        SkiaGradientBuilder(float sx, float sy, float ex, float ey) :
            sx(sx), sy(sy), ex(ex), ey(ey) {}

        virtual void make(SkPaint& paint) = 0;
    };

    class SkiaRadialGradientBuilder : public SkiaGradientBuilder {
    public:
        SkiaRadialGradientBuilder(float sx, float sy, float ex, float ey) :
            SkiaGradientBuilder(sx, sy, ex, ey) {}
        void make(SkPaint& paint) override;
    };

    class SkiaLinearGradientBuilder : public SkiaGradientBuilder {
    public:
        SkiaLinearGradientBuilder(float sx, float sy, float ex, float ey) :
            SkiaGradientBuilder(sx, sy, ex, ey) {}
        void make(SkPaint& paint) override;
    };

    class SkiaRenderPaint : public RenderPaint {
    private:
        SkPaint m_Paint;
        SkiaGradientBuilder* m_GradientBuilder;

    public:
        const SkPaint& paint() const { return m_Paint; }
        SkiaRenderPaint();
        void style(RenderPaintStyle style) override;
        void color(unsigned int value) override;
        void thickness(float value) override;
        void join(StrokeJoin value) override;
        void cap(StrokeCap value) override;
        void blendMode(BlendMode value) override;

        void linearGradient(float sx, float sy, float ex, float ey) override;
        void radialGradient(float sx, float sy, float ex, float ey) override;
        void addStop(unsigned int color, float stop) override;
        void completeGradient() override;
    };

    class SkiaRenderImage : public RenderImage {
    private:
        sk_sp<SkImage> m_SkImage;

    public:
        sk_sp<SkImage> skImage() const { return m_SkImage; };
        bool decode(const uint8_t* bytes, std::size_t size) override;
    };

    class SkiaRenderer : public Renderer {
    protected:
        SkCanvas* m_Canvas;

    public:
        SkiaRenderer(SkCanvas* canvas) : m_Canvas(canvas) {}
        void save() override;
        void restore() override;
        void transform(const Mat2D& transform) override;
        void drawPath(RenderPath* path, RenderPaint* paint) override;
        void
        drawImage(RenderImage* image, BlendMode value, float opacity) override;
        void clipPath(RenderPath* path) override;
    };
} // namespace rive
#endif