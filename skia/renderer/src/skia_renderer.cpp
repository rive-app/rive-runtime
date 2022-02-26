#include "skia_renderer.hpp"
#include "SkGradientShader.h"
#include "SkPath.h"
#include "rive/math/vec2d.hpp"
#include "rive/shapes/paint/color.hpp"
#include "to_skia.hpp"

using namespace rive;

class SkiaBuffer : public RenderBuffer {
    const size_t m_ElemSize;
    void* m_Buffer;
public:
    SkiaBuffer(const void* src, size_t count, size_t elemSize)
        : RenderBuffer(count)
        , m_ElemSize(elemSize)
    {
        size_t bytes = count * elemSize;
        m_Buffer = malloc(bytes);
        memcpy(m_Buffer, src, bytes);
    }
    
    ~SkiaBuffer() {
        free(m_Buffer);
    }

    const float* f32s() {
        assert(m_ElemSize == sizeof(float));
        return static_cast<const float*>(m_Buffer);
    }

    const uint16_t* u16s() const {
        assert(m_ElemSize == sizeof(uint16_t));
        return static_cast<const uint16_t*>(m_Buffer);
    }
};

template <typename T> rcp<RenderBuffer> make_buffer(const T src[], size_t count) {
    return rcp<RenderBuffer>(new SkiaBuffer(src, count, sizeof(T)));
}

class SkiaRenderShader : public RenderShader {
public:
    SkiaRenderShader(sk_sp<SkShader> sh) : shader(std::move(sh)) {}

    sk_sp<SkShader> shader;
};

void SkiaRenderPath::fillRule(FillRule value) {
    switch (value) {
        case FillRule::evenOdd:
            m_Path.setFillType(SkPathFillType::kEvenOdd);
            break;
        case FillRule::nonZero:
            m_Path.setFillType(SkPathFillType::kWinding);
            break;
    }
}

void SkiaRenderPath::reset() { m_Path.reset(); }
void SkiaRenderPath::addRenderPath(RenderPath* path, const Mat2D& transform) {
    m_Path.addPath(reinterpret_cast<SkiaRenderPath*>(path)->m_Path,
                   ToSkia::convert(transform));
}

void SkiaRenderPath::moveTo(float x, float y) { m_Path.moveTo(x, y); }
void SkiaRenderPath::lineTo(float x, float y) { m_Path.lineTo(x, y); }
void SkiaRenderPath::cubicTo(
    float ox, float oy, float ix, float iy, float x, float y) {
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
void SkiaRenderPaint::join(StrokeJoin value) {
    m_Paint.setStrokeJoin(ToSkia::convert(value));
}
void SkiaRenderPaint::cap(StrokeCap value) {
    m_Paint.setStrokeCap(ToSkia::convert(value));
}

void SkiaRenderPaint::blendMode(BlendMode value) {
    m_Paint.setBlendMode(ToSkia::convert(value));
}

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

void SkiaRenderer::drawImage(RenderImage* image,
                             BlendMode blendMode,
                             float opacity) {
    SkPaint paint;
    paint.setAlphaf(opacity);
    paint.setBlendMode(ToSkia::convert(blendMode));
    auto skiaImage = reinterpret_cast<SkiaRenderImage*>(image);
    SkSamplingOptions samplingOptions(SkFilterMode::kLinear,
                                      SkMipmapMode::kNone);
    m_Canvas->drawImage(
        skiaImage->skImage(), 0.0f, 0.0f, samplingOptions, &paint);
}

bool SkiaRenderImage::decode(const uint8_t* bytes, std::size_t size) {

    sk_sp<SkData> data = SkData::MakeWithoutCopy(bytes, size);
    m_SkImage = SkImage::MakeFromEncoded(data);
    m_Width = m_SkImage->width();
    m_Height = m_SkImage->height();
    return true;
}

rcp<RenderShader> SkiaRenderImage::makeShader(RenderTileMode tx, RenderTileMode ty,
                                              const Mat2D* localMatrix) const {
    const SkMatrix lm = localMatrix ? ToSkia::convert(*localMatrix) : SkMatrix();
    const SkSamplingOptions options(SkFilterMode::kLinear);
    auto sh = m_SkImage->makeShader(ToSkia::convert(tx), ToSkia::convert(ty), options, &lm);
    return rcp<RenderShader>(new SkiaRenderShader(std::move(sh)));
}

namespace rive {
    rcp<RenderBuffer> makeBufferU16(const uint16_t src[], size_t count) {
        return make_buffer<uint16_t>(src, count);
    }
    rcp<RenderBuffer> makeBufferU32(const uint32_t src[], size_t count) {
        return make_buffer<uint32_t>(src, count);
    }
    rcp<RenderBuffer> makeBufferF32(const float src[], size_t count) {
        return make_buffer<float>(src, count);
    }

    RenderPath* makeRenderPath() { return new SkiaRenderPath(); }
    RenderPaint* makeRenderPaint() { return new SkiaRenderPaint(); }
    RenderImage* makeRenderImage() { return new SkiaRenderImage(); }

    rcp<RenderShader> makeLinearGradient(float sx, float sy, float ex, float ey,
                                         const ColorInt colors[], const float stops[],
                                         int count, RenderTileMode mode,
                                         const Mat2D* localm)
    {
        const SkPoint pts[] = { {sx, sy}, {ex, ey} };
        const SkMatrix lm = localm ? ToSkia::convert(*localm) : SkMatrix();
        auto sh = SkGradientShader::MakeLinear(pts, (const SkColor*)colors, stops, count,
                                               ToSkia::convert(mode), 0, &lm);
        return rcp<RenderShader>(new SkiaRenderShader(std::move(sh)));
    }

    rcp<RenderShader> makeRadialGradient(float cx, float cy, float radius,
                                         const ColorInt colors[], const float stops[],
                                         int count, RenderTileMode mode,
                                         const Mat2D* localm)
    {
        const SkMatrix lm = localm ? ToSkia::convert(*localm) : SkMatrix();
        auto sh = SkGradientShader::MakeRadial({cx, cy}, radius,
                                               (const SkColor*)colors, stops, count,
                                               ToSkia::convert(mode), 0, &lm);
        return rcp<RenderShader>(new SkiaRenderShader(std::move(sh)));
    }

    rcp<RenderShader> makeSweepGradient(float cx, float cy,
                                        const ColorInt colors[], const float stops[],
                                        int count, RenderTileMode mode,
                                        const Mat2D* localm)
   {
       const SkMatrix lm = localm ? ToSkia::convert(*localm) : SkMatrix();
       auto sh = SkGradientShader::MakeSweep(cx, cy, (const SkColor*)colors, stops, count,
                                             0, &lm);
       return rcp<RenderShader>(new SkiaRenderShader(std::move(sh)));
   }
} // namespace rive
