/*
 * Copyright 2022 Rive
 */

#include "rive/rive_types.hpp"

#ifdef RIVE_BUILD_FOR_APPLE

#include "cg_factory.hpp"
#include "cg_renderer.hpp"
#include "mac_utils.hpp"

#if defined(RIVE_BUILD_FOR_OSX)
#include <ApplicationServices/ApplicationServices.h>
#elif defined(RIVE_BUILD_FOR_IOS)
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#endif

#include "utils/factory_utils.hpp"
#include "rive/math/vec2d.hpp"
#include "rive/core/type_conversions.hpp"
#include "rive/shapes/paint/color.hpp"

using namespace rive;

static CGAffineTransform convert(const Mat2D& m)
{
    return CGAffineTransformMake(m[0], m[1], m[2], m[3], m[4], m[5]);
}

static CGPathDrawingMode convert(FillRule rule)
{
    return (rule == FillRule::nonZero) ? CGPathDrawingMode::kCGPathFill
                                       : CGPathDrawingMode::kCGPathEOFill;
}

static CGLineJoin convert(StrokeJoin j)
{
    const CGLineJoin cg[] = {
        CGLineJoin::kCGLineJoinMiter,
        CGLineJoin::kCGLineJoinRound,
        CGLineJoin::kCGLineJoinBevel,
    };
    return cg[(unsigned)j];
}

static CGLineCap convert(StrokeCap c)
{
    const CGLineCap cg[] = {
        CGLineCap::kCGLineCapButt,
        CGLineCap::kCGLineCapRound,
        CGLineCap::kCGLineCapSquare,
    };
    return cg[(unsigned)c];
}

// clang-format off
static CGBlendMode convert(BlendMode mode) {
    CGBlendMode cg = kCGBlendModeNormal;
    switch (mode) {
        case BlendMode::srcOver: cg = kCGBlendModeNormal; break;
        case BlendMode::screen: cg = kCGBlendModeScreen; break;
        case BlendMode::overlay: cg = kCGBlendModeOverlay; break;
        case BlendMode::darken: cg = kCGBlendModeDarken; break;
        case BlendMode::lighten: cg = kCGBlendModeLighten; break;
        case BlendMode::colorDodge: cg = kCGBlendModeColorDodge; break;
        case BlendMode::colorBurn: cg = kCGBlendModeColorBurn; break;
        case BlendMode::hardLight: cg = kCGBlendModeHardLight; break;
        case BlendMode::softLight: cg = kCGBlendModeSoftLight; break;
        case BlendMode::difference: cg = kCGBlendModeDifference; break;
        case BlendMode::exclusion: cg = kCGBlendModeExclusion; break;
        case BlendMode::multiply: cg = kCGBlendModeMultiply; break;
        case BlendMode::hue: cg = kCGBlendModeHue; break;
        case BlendMode::saturation: cg = kCGBlendModeSaturation; break;
        case BlendMode::color: cg = kCGBlendModeColor; break;
        case BlendMode::luminosity: cg = kCGBlendModeLuminosity; break;
    }
    return cg;
}
// clang-format on

static void convertColor(ColorInt c, CGFloat rgba[])
{
    constexpr float kByteToUnit = 1.0f / 255;
    rgba[0] = colorRed(c) * kByteToUnit;
    rgba[1] = colorGreen(c) * kByteToUnit;
    rgba[2] = colorBlue(c) * kByteToUnit;
    rgba[3] = colorAlpha(c) * kByteToUnit;
}

class CGRenderPath : public RenderPath
{
private:
    AutoCF<CGMutablePathRef> m_path = CGPathCreateMutable();
    CGPathDrawingMode m_fillMode = CGPathDrawingMode::kCGPathFill;

public:
    CGRenderPath() {}

    CGRenderPath(Span<const Vec2D> pts, Span<const PathVerb> vbs, FillRule rule)
    {
        m_fillMode = convert(rule);

        auto p = pts.data();
        for (auto v : vbs)
        {
            switch ((PathVerb)v)
            {
                case PathVerb::move:
                    CGPathMoveToPoint(m_path, nullptr, p[0].x, p[0].y);
                    p += 1;
                    break;
                case PathVerb::line:
                    CGPathAddLineToPoint(m_path, nullptr, p[0].x, p[0].y);
                    p += 1;
                    break;
                case PathVerb::quad:
                    CGPathAddQuadCurveToPoint(m_path, nullptr, p[0].x, p[0].y, p[1].x, p[1].y);
                    p += 2;
                    break;
                case PathVerb::cubic:
                    CGPathAddCurveToPoint(m_path,
                                          nullptr,
                                          p[0].x,
                                          p[0].y,
                                          p[1].x,
                                          p[1].y,
                                          p[2].x,
                                          p[2].y);
                    p += 3;
                    break;
                case PathVerb::close:
                    CGPathCloseSubpath(m_path);
                    break;
            }
        }
        assert(p == pts.end());
    }

    CGPathRef path() const { return m_path.get(); }
    CGPathDrawingMode drawingMode(bool isStroke) const
    {
        return isStroke ? CGPathDrawingMode::kCGPathStroke : m_fillMode;
    }

    void reset() override { m_path.reset(CGPathCreateMutable()); }
    void addRenderPath(RenderPath* path, const Mat2D& mx) override
    {
        auto transform = convert(mx);
        CGPathAddPath(m_path, &transform, ((CGRenderPath*)path)->path());
    }
    void fillRule(FillRule value) override
    {
        m_fillMode = (value == FillRule::nonZero) ? CGPathDrawingMode::kCGPathFill
                                                  : CGPathDrawingMode::kCGPathEOFill;
    }
    void moveTo(float x, float y) override { CGPathMoveToPoint(m_path, nullptr, x, y); }
    void lineTo(float x, float y) override { CGPathAddLineToPoint(m_path, nullptr, x, y); }
    void cubicTo(float ox, float oy, float ix, float iy, float x, float y) override
    {
        CGPathAddCurveToPoint(m_path, nullptr, ox, oy, ix, iy, x, y);
    }
    void close() override { CGPathCloseSubpath(m_path); }
};

class CGRenderShader : public RenderShader
{
public:
    CGRenderShader() {}

    static constexpr int clampOptions =
        kCGGradientDrawsBeforeStartLocation | kCGGradientDrawsAfterEndLocation;

    virtual void draw(CGContextRef) {}
};

class CGRenderPaint : public RenderPaint
{
private:
    bool m_isStroke = false;
    CGFloat m_rgba[4] = {0, 0, 0, 1};
    float m_width = 1;
    CGLineJoin m_join = kCGLineJoinMiter;
    CGLineCap m_cap = kCGLineCapButt;
    CGBlendMode m_blend = kCGBlendModeNormal;
    rcp<RenderShader> m_shader;

public:
    CGRenderPaint() {}

    bool isStroke() const { return m_isStroke; }
    float opacity() const { return m_rgba[3]; }

    CGRenderShader* shader() const { return static_cast<CGRenderShader*>(m_shader.get()); }

    void apply(CGContextRef ctx)
    {
        if (m_isStroke)
        {
            CGContextSetRGBStrokeColor(ctx, m_rgba[0], m_rgba[1], m_rgba[2], m_rgba[3]);
            CGContextSetLineWidth(ctx, m_width);
            CGContextSetLineJoin(ctx, m_join);
            CGContextSetLineCap(ctx, m_cap);
        }
        else
        {
            CGContextSetRGBFillColor(ctx, m_rgba[0], m_rgba[1], m_rgba[2], m_rgba[3]);
        }
        CGContextSetBlendMode(ctx, m_blend);
    }

    void style(RenderPaintStyle style) override
    {
        m_isStroke = (style == RenderPaintStyle::stroke);
    }
    void color(ColorInt value) override { convertColor(value, m_rgba); }
    void thickness(float value) override { m_width = value; }
    void join(StrokeJoin value) override { m_join = convert(value); }
    void cap(StrokeCap value) override { m_cap = convert(value); }
    void blendMode(BlendMode value) override { m_blend = convert(value); }
    void shader(rcp<RenderShader> sh) override { m_shader = std::move(sh); }
    void invalidateStroke() override {}
};

static CGGradientRef convert(const ColorInt colors[], const float stops[], size_t count)
{
    AutoCF space = CGColorSpaceCreateDeviceRGB();
    std::vector<CGFloat> floats(count * 5); // colors[4] + stops[1]
    auto c = &floats[0];
    auto s = &floats[count * 4];

    for (size_t i = 0; i < count; ++i)
    {
        convertColor(colors[i], &c[i * 4]);

        // Rive wants the colors to be premultiplied *after* interpolation
        // Unfortunately, CG doesn't know about this option, it just does
        // a straight interpolation and uses the result (thinking it is
        // in premul form already). This can lead to artifacts in the drawing
        // (e.g. sparkles) so as a hack, we premul our color stops up front.
        // Not exactly correct, but does remove the sparkles.
        // A better fix might be to write a custom Shading proc... but that
        // is likely to be be slower (but need to try/time it to know for sure).
        CGFloat* p = &c[i * 4];
        p[0] *= p[3];
        p[1] *= p[3];
        p[2] *= p[3];
    }
    if (stops)
    {
        for (size_t i = 0; i < count; ++i)
        {
            s[i] = stops[i];
        }
    }
    return CGGradientCreateWithColorComponents(space, c, s, count);
}

class CGRadialGradientRenderShader : public CGRenderShader
{
    AutoCF<CGGradientRef> m_grad;
    CGPoint m_center;
    CGFloat m_radius;

public:
    CGRadialGradientRenderShader(float cx,
                                 float cy,
                                 float radius,
                                 const ColorInt colors[],
                                 const float stops[],
                                 size_t count) :
        m_grad(convert(colors, stops, count))
    {
        m_center = CGPointMake(cx, cy);
        m_radius = radius;
    }

    void draw(CGContextRef ctx) override
    {
        CGContextDrawRadialGradient(ctx, m_grad, m_center, 0, m_center, m_radius, clampOptions);
    }
};

class CGLinearGradientRenderShader : public CGRenderShader
{
    AutoCF<CGGradientRef> m_grad;
    CGPoint m_start, m_end;

public:
    CGLinearGradientRenderShader(float sx,
                                 float sy,
                                 float ex,
                                 float ey,
                                 const ColorInt colors[], // [count]
                                 const float stops[],     // [count]
                                 size_t count) :
        m_grad(convert(colors, stops, count))
    {
        m_start = CGPointMake(sx, sy);
        m_end = CGPointMake(ex, ey);
    }

    void draw(CGContextRef ctx) override
    {
        CGContextDrawLinearGradient(ctx, m_grad, m_start, m_end, clampOptions);
    }
};

class CGRenderImage : public RenderImage
{
public:
    AutoCF<CGImageRef> m_image;

    CGRenderImage(const Span<const uint8_t> span) : m_image(DecodeToCGImage(span))
    {
        if (m_image)
        {
            m_Width = rive::castTo<uint32_t>(CGImageGetWidth(m_image.get()));
            m_Height = rive::castTo<uint32_t>(CGImageGetHeight(m_image.get()));
        }
    }

    Mat2D localM2D() const { return Mat2D(1, 0, 0, -1, 0, (float)m_Height); }

    void applyLocalMatrix(CGContextRef ctx) const
    {
        CGContextConcatCTM(ctx, CGAffineTransformMake(1, 0, 0, -1, 0, (float)m_Height));
    }

    static const CGRenderImage* Cast(const RenderImage* image)
    {
        return reinterpret_cast<const CGRenderImage*>(image);
    }
};

//////////////////////////////////////////////////////////////////////////

CGRenderer::CGRenderer(CGContextRef ctx, int width, int height) : m_ctx(ctx)
{
    CGContextSaveGState(ctx);

    Mat2D m(1, 0, 0, -1, 0, height);
    CGContextConcatCTM(ctx, convert(m));

    CGContextSetInterpolationQuality(ctx, kCGInterpolationMedium);
}

CGRenderer::~CGRenderer() { CGContextRestoreGState(m_ctx); }

void CGRenderer::save() { CGContextSaveGState(m_ctx); }

void CGRenderer::restore() { CGContextRestoreGState(m_ctx); }

void CGRenderer::transform(const Mat2D& m) { CGContextConcatCTM(m_ctx, convert(m)); }

void CGRenderer::drawPath(RenderPath* path, RenderPaint* paint)
{
    auto cgpaint = reinterpret_cast<CGRenderPaint*>(paint);
    auto cgpath = reinterpret_cast<CGRenderPath*>(path);

    cgpaint->apply(m_ctx);

    CGContextBeginPath(m_ctx);
    CGContextAddPath(m_ctx, cgpath->path());
    if (auto sh = cgpaint->shader())
    {
        if (cgpaint->isStroke())
        {
            // so we can clip against the "stroke" of the path
            CGContextReplacePathWithStrokedPath(m_ctx);
        }
        CGContextSaveGState(m_ctx);
        CGContextClip(m_ctx);

        // so the gradient modulates with the color's alpha
        CGContextSetAlpha(m_ctx, cgpaint->opacity());

        sh->draw(m_ctx);
        CGContextRestoreGState(m_ctx);
    }
    else
    {
        CGContextDrawPath(m_ctx, cgpath->drawingMode(cgpaint->isStroke()));
    }

    assert(CGContextIsPathEmpty(m_ctx));
}

void CGRenderer::clipPath(RenderPath* path)
{
    auto cgpath = reinterpret_cast<CGRenderPath*>(path);

    CGContextBeginPath(m_ctx);
    CGContextAddPath(m_ctx, cgpath->path());
    CGContextClip(m_ctx);
}

void CGRenderer::drawImage(const RenderImage* image, BlendMode blendMode, float opacity)
{
    auto bounds = CGRectMake(0, 0, image->width(), image->height());

    CGContextSaveGState(m_ctx);
    CGContextSetAlpha(m_ctx, opacity);
    CGContextSetBlendMode(m_ctx, convert(blendMode));
    auto cgimg = CGRenderImage::Cast(image);
    cgimg->applyLocalMatrix(m_ctx);
    CGContextDrawImage(m_ctx, bounds, cgimg->m_image);
    CGContextRestoreGState(m_ctx);
}

static Mat2D basis_matrix(Vec2D p0, Vec2D p1, Vec2D p2)
{
    auto e0 = p1 - p0;
    auto e1 = p2 - p0;
    return Mat2D(e0.x, e0.y, e1.x, e1.y, p0.x, p0.y);
}

void CGRenderer::drawImageMesh(const RenderImage* image,
                               rcp<RenderBuffer> vertices,
                               rcp<RenderBuffer> uvCoords,
                               rcp<RenderBuffer> indices,
                               BlendMode blendMode,
                               float opacity)
{
    auto cgimage = CGRenderImage::Cast(image);
    auto const localMatrix = cgimage->localM2D();

    const float sx = image->width();
    const float sy = image->height();
    auto const bounds = CGRectMake(0, 0, sx, sy);

    auto scale = [sx, sy](Vec2D v) { return Vec2D{v.x * sx, v.y * sy}; };

    auto triangles = indices->count() / 3;
    auto ndx = DataRenderBuffer::Cast(indices.get())->u16s();
    auto pts = DataRenderBuffer::Cast(vertices.get())->vecs();
    auto uvs = DataRenderBuffer::Cast(uvCoords.get())->vecs();

    // We use the path to set the clip for each triangle. Since calling
    // CGContextClip() resets the path, we only need to this once at
    // the beginning.
    CGContextBeginPath(m_ctx);

    CGContextSaveGState(m_ctx);
    CGContextSetAlpha(m_ctx, opacity);
    CGContextSetBlendMode(m_ctx, convert(blendMode));
    CGContextSetShouldAntialias(m_ctx, false);

    for (size_t i = 0; i < triangles; ++i)
    {
        const auto index0 = *ndx++;
        const auto index1 = *ndx++;
        const auto index2 = *ndx++;

        CGContextSaveGState(m_ctx);

        const auto p0 = pts[index0];
        const auto p1 = pts[index1];
        const auto p2 = pts[index2];
        CGContextMoveToPoint(m_ctx, p0.x, p0.y);
        CGContextAddLineToPoint(m_ctx, p1.x, p1.y);
        CGContextAddLineToPoint(m_ctx, p2.x, p2.y);
        CGContextClip(m_ctx);

        const auto v0 = scale(uvs[index0]);
        const auto v1 = scale(uvs[index1]);
        const auto v2 = scale(uvs[index2]);
        auto mx =
            basis_matrix(p0, p1, p2) * basis_matrix(v0, v1, v2).invertOrIdentity() * localMatrix;
        CGContextConcatCTM(m_ctx, convert(mx));
        CGContextDrawImage(m_ctx, bounds, cgimage->m_image);

        CGContextRestoreGState(m_ctx);
    }

    CGContextRestoreGState(m_ctx); // restore opacity, antialias, etc.
}

// Factory

rcp<RenderBuffer> CGFactory::makeBufferU16(Span<const uint16_t> data)
{
    return DataRenderBuffer::Make(data);
}

rcp<RenderBuffer> CGFactory::makeBufferU32(Span<const uint32_t> data)
{
    return DataRenderBuffer::Make(data);
}

rcp<RenderBuffer> CGFactory::makeBufferF32(Span<const float> data)
{
    return DataRenderBuffer::Make(data);
}

rcp<RenderShader> CGFactory::makeLinearGradient(float sx,
                                                float sy,
                                                float ex,
                                                float ey,
                                                const ColorInt colors[], // [count]
                                                const float stops[],     // [count]
                                                size_t count)
{
    return rcp<RenderShader>(
        new CGLinearGradientRenderShader(sx, sy, ex, ey, colors, stops, count));
}

rcp<RenderShader> CGFactory::makeRadialGradient(float cx,
                                                float cy,
                                                float radius,
                                                const ColorInt colors[], // [count]
                                                const float stops[],     // [count]
                                                size_t count)
{
    return rcp<RenderShader>(
        new CGRadialGradientRenderShader(cx, cy, radius, colors, stops, count));
}

std::unique_ptr<RenderPath> CGFactory::makeRenderPath(RawPath& rawPath, FillRule fillRule)
{
    return std::make_unique<CGRenderPath>(rawPath.points(), rawPath.verbs(), fillRule);
}

std::unique_ptr<RenderPath> CGFactory::makeEmptyRenderPath()
{
    return std::make_unique<CGRenderPath>();
}

std::unique_ptr<RenderPaint> CGFactory::makeRenderPaint()
{
    return std::make_unique<CGRenderPaint>();
}

std::unique_ptr<RenderImage> CGFactory::decodeImage(Span<const uint8_t> encoded)
{
    return std::make_unique<CGRenderImage>(encoded);
}

#endif // APPLE
