/*
 * Copyright 2026 Rive
 */

#ifndef _RIVE_SVG_FACTORY_HPP_
#define _RIVE_SVG_FACTORY_HPP_

#include "rive/factory.hpp"
#include "rive/math/raw_path.hpp"
#include "rive/renderer.hpp"
#include "rive/shapes/paint/color.hpp"
#include <sstream>
#include <string>
#include <vector>

namespace rive
{

// -- Render objects stored by SVGFactory, read by SVGRenderer --

class SVGRenderPath : public RenderPath
{
public:
    SVGRenderPath() = default;
    SVGRenderPath(RawPath& rawPath, FillRule fillRule);

    void rewind() override;
    void fillRule(FillRule value) override;
    void addRenderPath(const RenderPath* path, const Mat2D& transform) override;
    void moveTo(float x, float y) override;
    void lineTo(float x, float y) override;
    void cubicTo(float ox, float oy, float ix, float iy, float x, float y)
        override;
    void close() override;
    void addRawPath(const RawPath& path) override;

    std::string toSvgD(int floatPrecision) const;
    FillRule getFillRule() const { return m_fillRule; }

private:
    RawPath m_rawPath;
    FillRule m_fillRule = FillRule::nonZero;
};

class SVGRenderShader : public RenderShader
{
public:
    virtual ~SVGRenderShader() = default;
    virtual void emitDefs(std::ostream& out, const std::string& id) const = 0;
};

class SVGLinearGradientShader : public SVGRenderShader
{
public:
    SVGLinearGradientShader(float sx,
                            float sy,
                            float ex,
                            float ey,
                            const ColorInt colors[],
                            const float stops[],
                            size_t count);
    void emitDefs(std::ostream& out, const std::string& id) const override;

private:
    float m_sx, m_sy, m_ex, m_ey;
    std::vector<ColorInt> m_colors;
    std::vector<float> m_stops;
};

class SVGRadialGradientShader : public SVGRenderShader
{
public:
    SVGRadialGradientShader(float cx,
                            float cy,
                            float radius,
                            const ColorInt colors[],
                            const float stops[],
                            size_t count);
    void emitDefs(std::ostream& out, const std::string& id) const override;

private:
    float m_cx, m_cy, m_radius;
    std::vector<ColorInt> m_colors;
    std::vector<float> m_stops;
};

class SVGRenderPaint : public RenderPaint
{
public:
    void style(RenderPaintStyle value) override;
    void color(ColorInt value) override;
    void thickness(float value) override;
    void join(StrokeJoin value) override;
    void cap(StrokeCap value) override;
    void blendMode(BlendMode value) override;
    void shader(rcp<RenderShader> sh) override;
    void invalidateStroke() override {}
    void feather(float value) override {}

    bool isStroke() const { return m_isStroke; }
    ColorInt getColor() const { return m_color; }
    float getThickness() const { return m_thickness; }
    StrokeJoin getJoin() const { return m_join; }
    StrokeCap getCap() const { return m_cap; }
    BlendMode getBlendMode() const { return m_blendMode; }
    SVGRenderShader* getSvgShader() const;

private:
    bool m_isStroke = false;
    ColorInt m_color = 0xFF000000;
    float m_thickness = 1.0f;
    StrokeJoin m_join = StrokeJoin::miter;
    StrokeCap m_cap = StrokeCap::butt;
    BlendMode m_blendMode = BlendMode::srcOver;
    rcp<RenderShader> m_shader;
};

class SVGRenderImage : public RenderImage
{
public:
    SVGRenderImage(Span<const uint8_t> encodedBytes,
                   uint32_t width,
                   uint32_t height);
    std::string toDataURI() const;

private:
    std::vector<uint8_t> m_encodedBytes;
};

class SVGRenderBuffer : public RenderBuffer
{
public:
    SVGRenderBuffer(RenderBufferType type,
                    RenderBufferFlags flags,
                    size_t sizeInBytes);

protected:
    void* onMap() override;
    void onUnmap() override;

private:
    std::vector<uint8_t> m_bytes;
};

// -- Factory --

class SVGFactory : public Factory
{
public:
    // Reads the pixel dimensions of an encoded image. SVG embeds the encoded
    // bytes verbatim as a data URI, so the pixels themselves are never needed.
    // Returns false when the format isn't recognized.
    using DecodeImageSize = bool (*)(Span<const uint8_t> encodedBytes,
                                     uint32_t* width,
                                     uint32_t* height);

    // Injected so this factory depends on no particular image decoder.
    // Without one, decodeImage() fails and images drop out of the output.
    explicit SVGFactory(DecodeImageSize decodeImageSize = nullptr) :
        m_decodeImageSize(decodeImageSize)
    {}

    rcp<RenderBuffer> makeRenderBuffer(RenderBufferType type,
                                       RenderBufferFlags flags,
                                       size_t sizeInBytes) override;

    rcp<RenderShader> makeLinearGradient(float sx,
                                         float sy,
                                         float ex,
                                         float ey,
                                         const ColorInt colors[],
                                         const float stops[],
                                         size_t count) override;

    rcp<RenderShader> makeRadialGradient(float cx,
                                         float cy,
                                         float radius,
                                         const ColorInt colors[],
                                         const float stops[],
                                         size_t count) override;

    rcp<RenderPath> makeRenderPath(RawPath& rawPath,
                                   FillRule fillRule) override;
    rcp<RenderPath> makeEmptyRenderPath() override;
    rcp<RenderPaint> makeRenderPaint() override;
    rcp<RenderImage> decodeImage(Span<const uint8_t> data) override;

private:
    DecodeImageSize m_decodeImageSize;
};

} // namespace rive

#endif
