#ifndef _RIVE_RENDERER_HPP_
#define _RIVE_RENDERER_HPP_

#include "rive/shapes/paint/color.hpp"
#include "rive/command_path.hpp"
#include "rive/layout.hpp"
#include "rive/refcnt.hpp"
#include "rive/span.hpp"
#include "rive/math/aabb.hpp"
#include "rive/math/mat2d.hpp"
#include "rive/math/raw_path.hpp"
#include "rive/shapes/paint/blend_mode.hpp"
#include "rive/shapes/paint/stroke_cap.hpp"
#include "rive/shapes/paint/stroke_join.hpp"
#include <cmath>
#include <stdio.h>
#include <cstdint>

namespace rive {
    class Vec2D;

    // A render buffer holds an immutable array of values
    class RenderBuffer : public RefCnt {
        const size_t m_Count;

    public:
        RenderBuffer(size_t count) : m_Count(count) {}

        size_t count() const { return m_Count; }
    };

    extern rcp<RenderBuffer> makeBufferU16(Span<const uint16_t>);
    extern rcp<RenderBuffer> makeBufferU32(Span<const uint32_t>);
    extern rcp<RenderBuffer> makeBufferF32(Span<const float>);

    enum class RenderPaintStyle { stroke, fill };

    enum class RenderTileMode {
        clamp,
        repeat,
        mirror,
        decal, // fill outside the domain with transparent
    };

    /*
     *  Base class for Render objects that specify the src colors.
     *
     *  Shaders are immutable, and sharable between multiple paints, etc.
     *
     *  It is common that a shader may be created with a 'localMatrix'. If this is
     *  not null, then it is applied to the shader's domain before the Renderer's CTM.
     */
    class RenderShader : public RefCnt {};

    extern rcp<RenderShader> makeLinearGradient(float sx,
                                                float sy,
                                                float ex,
                                                float ey,
                                                const ColorInt colors[], // [count]
                                                const float stops[],     // [count]
                                                int count,
                                                RenderTileMode,
                                                const Mat2D* localMatrix = nullptr);

    extern rcp<RenderShader> makeRadialGradient(float cx,
                                                float cy,
                                                float radius,
                                                const ColorInt colors[], // [count]
                                                const float stops[],     // [count]
                                                int count,
                                                RenderTileMode,
                                                const Mat2D* localMatrix = nullptr);

    extern rcp<RenderShader> makeSweepGradient(float cx,
                                               float cy,
                                               const ColorInt colors[], // [count]
                                               const float stops[],     // [count]
                                               int count,
                                               const Mat2D* localMatrix = nullptr);

    class RenderPaint {
    public:
        virtual void style(RenderPaintStyle style) = 0;
        virtual void color(ColorInt value) = 0;
        virtual void thickness(float value) = 0;
        virtual void join(StrokeJoin value) = 0;
        virtual void cap(StrokeCap value) = 0;
        virtual void blendMode(BlendMode value) = 0;
        virtual void shader(rcp<RenderShader>) = 0;

        virtual ~RenderPaint() {}
    };

    class RenderImage {
    protected:
        int m_Width = 0;
        int m_Height = 0;

    public:
        virtual ~RenderImage() {}
        virtual bool decode(Span<const uint8_t>) = 0;
        int width() const { return m_Width; }
        int height() const { return m_Height; }

        virtual rcp<RenderShader> makeShader(RenderTileMode tx,
                                             RenderTileMode ty,
                                             const Mat2D* localMatrix = nullptr) const = 0;
    };

    class RenderPath : public CommandPath {
    public:
        RenderPath* renderPath() override { return this; }
        void addPath(CommandPath* path, const Mat2D& transform) override {
            addRenderPath(path->renderPath(), transform);
        }

        virtual void addRenderPath(RenderPath* path, const Mat2D& transform) = 0;
    };

    class Renderer {
    public:
        virtual ~Renderer() {}
        virtual void save() = 0;
        virtual void restore() = 0;
        virtual void transform(const Mat2D& transform) = 0;
        virtual void drawPath(RenderPath* path, RenderPaint* paint) = 0;
        virtual void clipPath(RenderPath* path) = 0;
        virtual void drawImage(const RenderImage*, BlendMode, float opacity) = 0;
        virtual void drawImageMesh(const RenderImage*,
                                   rcp<RenderBuffer> vertices_f32,
                                   rcp<RenderBuffer> uvCoords_f32,
                                   rcp<RenderBuffer> indices_u16,
                                   BlendMode,
                                   float opacity) = 0;

        // helpers

        void translate(float x, float y);
        void scale(float sx, float sy);
        void rotate(float radians);

        void computeAlignment(Mat2D& result,
                              Fit fit,
                              const Alignment& alignment,
                              const AABB& frame,
                              const AABB& content);

        void align(Fit fit, const Alignment& alignment, const AABB& frame, const AABB& content);
    };

    // Returns a full-formed RenderPath -- can be treated as immutable
    extern RenderPath*
    makeRenderPath(Span<const Vec2D> points, Span<const uint8_t> verbs, FillRule);

    extern RenderPath* makeRenderPath();
    extern RenderPaint* makeRenderPaint();
    extern RenderImage* makeRenderImage();
} // namespace rive
#endif
