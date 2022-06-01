/*
 * Copyright 2022 Rive
 */

#include "skia_factory.hpp"
#include "skia_renderer.hpp"
#include "to_skia.hpp"

#include "include/core/SkCanvas.h"
#include "include/core/SkData.h"
#include "include/core/SkImage.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkVertices.h"
#include "include/effects/SkGradientShader.h"

#include "rive/math/vec2d.hpp"
#include "rive/shapes/paint/color.hpp"

using namespace rive;

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
    SkiaRenderPaint();

    const SkPaint& paint() const { return m_Paint; }

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
    SkiaRenderImage(sk_sp<SkImage> image);

    sk_sp<SkImage> skImage() const { return m_SkImage; }

    rcp<RenderShader>
    makeShader(RenderTileMode tx, RenderTileMode ty, const Mat2D* localMatrix) const override;
};

class SkiaBuffer : public RenderBuffer {
    const size_t m_ElemSize;
    void* m_Buffer;

public:
    SkiaBuffer(const void* src, size_t count, size_t elemSize) :
        RenderBuffer(count), m_ElemSize(elemSize) {
        size_t bytes = count * elemSize;
        m_Buffer = malloc(bytes);
        memcpy(m_Buffer, src, bytes);
    }

    ~SkiaBuffer() override { free(m_Buffer); }

    const float* f32s() const {
        assert(m_ElemSize == sizeof(float));
        return static_cast<const float*>(m_Buffer);
    }

    const uint16_t* u16s() const {
        assert(m_ElemSize == sizeof(uint16_t));
        return static_cast<const uint16_t*>(m_Buffer);
    }

    const SkPoint* points() const { return reinterpret_cast<const SkPoint*>(this->f32s()); }

    static const SkiaBuffer* Cast(const RenderBuffer* buffer) {
        return reinterpret_cast<const SkiaBuffer*>(buffer);
    }
};

template <typename T> rcp<RenderBuffer> make_buffer(Span<T> span) {
    return rcp<RenderBuffer>(new SkiaBuffer(span.data(), span.size(), sizeof(T)));
}

class SkiaRenderShader : public RenderShader {
public:
    SkiaRenderShader(sk_sp<SkShader> sh) : shader(std::move(sh)) {}

    sk_sp<SkShader> shader;
};

void SkiaRenderPath::fillRule(FillRule value) { m_Path.setFillType(ToSkia::convert(value)); }

void SkiaRenderPath::reset() { m_Path.reset(); }
void SkiaRenderPath::addRenderPath(RenderPath* path, const Mat2D& transform) {
    m_Path.addPath(reinterpret_cast<SkiaRenderPath*>(path)->m_Path, ToSkia::convert(transform));
}

void SkiaRenderPath::moveTo(float x, float y) { m_Path.moveTo(x, y); }
void SkiaRenderPath::lineTo(float x, float y) { m_Path.lineTo(x, y); }
void SkiaRenderPath::cubicTo(float ox, float oy, float ix, float iy, float x, float y) {
    m_Path.cubicTo(ox, oy, ix, iy, x, y);
}
void SkiaRenderPath::close() { m_Path.close(); }

SkiaRenderPaint::SkiaRenderPaint() { m_Paint.setAntiAlias(true); }

void SkiaRenderPaint::style(RenderPaintStyle style) {
    switch (style) {
        case RenderPaintStyle::fill:
            m_Paint.setStyle(SkPaint::Style::kFill_Style);
            break;
        case RenderPaintStyle::stroke:
            m_Paint.setStyle(SkPaint::Style::kStroke_Style);
            break;
    }
}
void SkiaRenderPaint::color(unsigned int value) { m_Paint.setColor(value); }
void SkiaRenderPaint::thickness(float value) { m_Paint.setStrokeWidth(value); }
void SkiaRenderPaint::join(StrokeJoin value) { m_Paint.setStrokeJoin(ToSkia::convert(value)); }
void SkiaRenderPaint::cap(StrokeCap value) { m_Paint.setStrokeCap(ToSkia::convert(value)); }

void SkiaRenderPaint::blendMode(BlendMode value) { m_Paint.setBlendMode(ToSkia::convert(value)); }

void SkiaRenderPaint::shader(rcp<RenderShader> rsh) {
    SkiaRenderShader* sksh = (SkiaRenderShader*)rsh.get();
    m_Paint.setShader(sksh ? sksh->shader : nullptr);
}

void SkiaRenderer::save() { m_Canvas->save(); }
void SkiaRenderer::restore() { m_Canvas->restore(); }
void SkiaRenderer::transform(const Mat2D& transform) {
    m_Canvas->concat(ToSkia::convert(transform));
}
void SkiaRenderer::drawPath(RenderPath* path, RenderPaint* paint) {
    m_Canvas->drawPath(reinterpret_cast<SkiaRenderPath*>(path)->path(),
                       reinterpret_cast<SkiaRenderPaint*>(paint)->paint());
}

void SkiaRenderer::clipPath(RenderPath* path) {
    m_Canvas->clipPath(reinterpret_cast<SkiaRenderPath*>(path)->path(), true);
}

void SkiaRenderer::drawImage(const RenderImage* image, BlendMode blendMode, float opacity) {
    SkPaint paint;
    paint.setAlphaf(opacity);
    paint.setBlendMode(ToSkia::convert(blendMode));
    auto skiaImage = reinterpret_cast<const SkiaRenderImage*>(image);
    SkSamplingOptions sampling(SkFilterMode::kLinear);
    m_Canvas->drawImage(skiaImage->skImage(), 0.0f, 0.0f, sampling, &paint);
}

#define SKIA_BUG_13047

void SkiaRenderer::drawImageMesh(const RenderImage* image,
                                 rcp<RenderBuffer> vertices,
                                 rcp<RenderBuffer> uvCoords,
                                 rcp<RenderBuffer> indices,
                                 BlendMode blendMode,
                                 float opacity) {
    // need our vertices and uvs to agree
    assert(vertices->count() == uvCoords->count());
    // vertices and uvs are arrays of floats, so we need their counts to be
    // even, since we treat them as arrays of points
    assert((vertices->count() & 1) == 0);

    const int vertexCount = vertices->count() >> 1;

    SkMatrix scaleM;

    const SkPoint* uvs = SkiaBuffer::Cast(uvCoords.get())->points();

#ifdef SKIA_BUG_13047
    // The local matrix is ignored for drawVertices, so we have to manually scale
    // the UVs to match Skia's convention...
    std::vector<SkPoint> scaledUVs(vertexCount);
    for (int i = 0; i < vertexCount; ++i) {
        scaledUVs[i] = {uvs[i].fX * image->width(), uvs[i].fY * image->height()};
    }
    uvs = scaledUVs.data();
#else
    // We do this because our UVs are normalized, but Skia expects them to be
    // sized to the shader (i.e. 0..width, 0..height).
    // To accomdate this, we effectively scaling the image down to 0..1 to
    // match the scale of the UVs.
    scaleM = SkMatrix::Scale(2.0f / image->width(), 2.0f / image->height());
#endif

    auto skiaImage = reinterpret_cast<const SkiaRenderImage*>(image)->skImage();
    const SkSamplingOptions sampling(SkFilterMode::kLinear);
    auto shader = skiaImage->makeShader(SkTileMode::kClamp, SkTileMode::kClamp, sampling, &scaleM);

    SkPaint paint;
    paint.setAlphaf(opacity);
    paint.setBlendMode(ToSkia::convert(blendMode));
    paint.setShader(shader);

    const SkColor* no_colors = nullptr;
    auto vertexMode = SkVertices::kTriangles_VertexMode;
    // clang-format off
    auto vt = SkVertices::MakeCopy(vertexMode,
                                   vertexCount,
                                   SkiaBuffer::Cast(vertices.get())->points(),
                                   uvs,
                                   no_colors,
                                   indices->count(),
                                   SkiaBuffer::Cast(indices.get())->u16s());
    // clang-format on

    // The blend mode is ignored if we don't have colors && uvs
    m_Canvas->drawVertices(vt, SkBlendMode::kModulate, paint);
}

SkiaRenderImage::SkiaRenderImage(sk_sp<SkImage> image) : m_SkImage(std::move(image)) {
    m_Width = m_SkImage->width();
    m_Height = m_SkImage->height();
}

rcp<RenderShader>
SkiaRenderImage::makeShader(RenderTileMode tx, RenderTileMode ty, const Mat2D* localMatrix) const {
    const SkMatrix lm = localMatrix ? ToSkia::convert(*localMatrix) : SkMatrix();
    const SkSamplingOptions options(SkFilterMode::kLinear);
    auto sh = m_SkImage->makeShader(ToSkia::convert(tx), ToSkia::convert(ty), options, &lm);
    return rcp<RenderShader>(new SkiaRenderShader(std::move(sh)));
}

// Factory

rcp<RenderBuffer> SkiaFactory::makeBufferU16(Span<const uint16_t> data) {
    return make_buffer(data);
}

rcp<RenderBuffer> SkiaFactory::makeBufferU32(Span<const uint32_t> data) {
    return make_buffer(data);
}

rcp<RenderBuffer> SkiaFactory::makeBufferF32(Span<const float> data) {
    return make_buffer(data);
}

rcp<RenderShader> SkiaFactory::makeLinearGradient(float sx, float sy,
                                                float ex, float ey,
                                                const ColorInt colors[],    // [count]
                                                const float stops[],        // [count]
                                                size_t count,
                                                RenderTileMode mode,
                                                const Mat2D* localMatrix) {
    const SkPoint pts[] = {{sx, sy}, {ex, ey}};
    const SkMatrix lm = localMatrix ? ToSkia::convert(*localMatrix) : SkMatrix();
    auto sh = SkGradientShader::MakeLinear(
        pts, (const SkColor*)colors, stops, count, ToSkia::convert(mode), 0, &lm);
    return rcp<RenderShader>(new SkiaRenderShader(std::move(sh)));
}

rcp<RenderShader> SkiaFactory::makeRadialGradient(float cx, float cy, float radius,
                                                const ColorInt colors[],    // [count]
                                                const float stops[],        // [count]
                                                size_t count,
                                                RenderTileMode mode,
                                                const Mat2D* localMatrix) {
    const SkMatrix lm = localMatrix ? ToSkia::convert(*localMatrix) : SkMatrix();
    auto sh = SkGradientShader::MakeRadial(
        {cx, cy}, radius, (const SkColor*)colors, stops, count, ToSkia::convert(mode), 0, &lm);
    return rcp<RenderShader>(new SkiaRenderShader(std::move(sh)));
}

std::unique_ptr<RenderPath> SkiaFactory::makeRenderPath(Span<const Vec2D> points,
                                                        Span<const uint8_t> verbs,
                                                        FillRule fillRule) {
    const bool isVolatile = false;  // ???
    const SkScalar* conicWeights = nullptr;
    const int conicWeightCount = 0;
    return std::make_unique<SkiaRenderPath>(SkPath::Make(reinterpret_cast<const SkPoint*>(points.data()),
                                                         points.count(),
                                                         verbs.data(),
                                                         verbs.count(),
                                                         conicWeights,
                                                         conicWeightCount,
                                                         ToSkia::convert(fillRule),
                                                         isVolatile));
}

std::unique_ptr<RenderPath> SkiaFactory::makeEmptyRenderPath() {
    return std::make_unique<SkiaRenderPath>();
}

std::unique_ptr<RenderPaint> SkiaFactory::makeRenderPaint() {
    return std::make_unique<SkiaRenderPaint>();
}

std::unique_ptr<RenderImage> SkiaFactory::decodeImage(Span<const uint8_t> encoded) {
    sk_sp<SkData> data = SkData::MakeWithoutCopy(encoded.data(), encoded.size());
    auto image = SkImage::MakeFromEncoded(data);

    // Our optimized skia buld seems to have broken lazy-image decode.
    // As a work-around for now, force the image to be decoded.
    if (image) {
        image = image->makeRasterImage();
    }

    return image ? std::make_unique<SkiaRenderImage>(std::move(image)) : nullptr;
}
