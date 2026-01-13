/*
 * Copyright 2022 Rive
 */

#include "skia_factory.hpp"
#include "skia_renderer.hpp"
#include "to_skia.hpp"

#include "include/core/SkCanvas.h"
#include "include/core/SkData.h"
#include "include/core/SkImage.h"
#include "include/core/SkPixmap.h"
#include "include/core/SkPaint.h"
#include "include/core/SkPath.h"
#include "include/core/SkVertices.h"
#include "include/effects/SkGradientShader.h"
#include "include/effects/SkImageFilters.h"
#include "include/core/SkColorFilter.h"

#include "rive/math/vec2d.hpp"
#include "rive/shapes/paint/color.hpp"
#include "utils/factory_utils.hpp"

using namespace rive;

// skia's has/had bugs in trilerp, so backing down to nearest mip
const SkSamplingOptions gSampling(SkFilterMode::kLinear,
                                  SkMipmapMode::kNearest);

class SkiaRenderPath : public LITE_RTTI_OVERRIDE(RenderPath, SkiaRenderPath)
{
private:
    SkPath m_Path;

public:
    SkiaRenderPath() {}
    SkiaRenderPath(SkPath&& path) : m_Path(std::move(path)) {}

    const SkPath& path() const { return m_Path; }

    void rewind() override;
    void addRenderPath(RenderPath* path, const Mat2D& transform) override;
    void addRawPath(const RawPath& path) override;
    void fillRule(FillRule value) override;
    void moveTo(float x, float y) override;
    void lineTo(float x, float y) override;
    void cubicTo(float ox, float oy, float ix, float iy, float x, float y)
        override;
    virtual void close() override;
};

class SkiaRenderPaint : public LITE_RTTI_OVERRIDE(RenderPaint, SkiaRenderPaint)
{
private:
    SkPaint m_Paint;
    bool m_hasFeather = false;

public:
    SkiaRenderPaint();

    const SkPaint& paint() const { return m_Paint; }

    void style(RenderPaintStyle style) override;
    void color(unsigned int value) override;
    void thickness(float value) override;
    void join(StrokeJoin value) override;
    void cap(StrokeCap value) override;
    void feather(float value) override;
    void blendMode(BlendMode value) override;
    void shader(rcp<RenderShader>) override;
    void invalidateStroke() override {}

    class OverrideStrokeParamsForFeather
    {
    public:
        OverrideStrokeParamsForFeather(SkiaRenderPaint* paint) :
            m_skPaint(&paint->m_Paint), m_hasFeather(paint->m_hasFeather)
        {
            if (m_hasFeather)
            {
                m_overriddenCap = m_skPaint->getStrokeCap();
                m_overriddenJoin = m_skPaint->getStrokeJoin();
                m_skPaint->setStrokeCap(SkPaint::kRound_Cap);
                m_skPaint->setStrokeJoin(SkPaint::kRound_Join);
            }
        }

        ~OverrideStrokeParamsForFeather()
        {
            if (m_hasFeather)
            {
                m_skPaint->setStrokeCap(m_overriddenCap);
                m_skPaint->setStrokeJoin(m_overriddenJoin);
            }
        }

    private:
        SkPaint* const m_skPaint;
        const bool m_hasFeather;
        SkPaint::Cap m_overriddenCap;
        SkPaint::Join m_overriddenJoin;
    };
};

class SkiaRenderImage : public LITE_RTTI_OVERRIDE(RenderImage, SkiaRenderImage)
{
private:
    sk_sp<SkImage> m_SkImage;

public:
    SkiaRenderImage(sk_sp<SkImage> image);

    sk_sp<SkImage> skImage() const { return m_SkImage; }
};

class SkiaRenderShader
    : public LITE_RTTI_OVERRIDE(RenderShader, SkiaRenderShader)
{
public:
    SkiaRenderShader(sk_sp<SkShader> sh) : shader(std::move(sh)) {}

    sk_sp<SkShader> shader;
};

void SkiaRenderPath::fillRule(FillRule value)
{
    m_Path.setFillType(ToSkia::convert(value));
}

void SkiaRenderPath::rewind()
{
    auto fillType = m_Path.getFillType();
    m_Path.rewind();
    m_Path.setFillType(fillType);
}
void SkiaRenderPath::addRenderPath(RenderPath* path, const Mat2D& transform)
{
    LITE_RTTI_CAST_OR_RETURN(skPath, SkiaRenderPath*, path);
    m_Path.addPath(skPath->m_Path, ToSkia::convert(transform));
}

void SkiaRenderPath::addRawPath(const RawPath& path)
{
    const bool isVolatile = false;
    const SkScalar* conicWeights = nullptr;
    const int conicWeightCount = 0;
    auto skPath =
        SkPath::Make(reinterpret_cast<const SkPoint*>(path.points().data()),
                     path.points().size(),
                     (uint8_t*)path.verbs().data(),
                     path.verbs().size(),
                     conicWeights,
                     conicWeightCount,
                     m_Path.getFillType(),
                     isVolatile);
    m_Path.addPath(skPath);
}

void SkiaRenderPath::moveTo(float x, float y) { m_Path.moveTo(x, y); }
void SkiaRenderPath::lineTo(float x, float y) { m_Path.lineTo(x, y); }
void SkiaRenderPath::cubicTo(float ox,
                             float oy,
                             float ix,
                             float iy,
                             float x,
                             float y)
{
    m_Path.cubicTo(ox, oy, ix, iy, x, y);
}
void SkiaRenderPath::close() { m_Path.close(); }

SkiaRenderPaint::SkiaRenderPaint() { m_Paint.setAntiAlias(true); }

void SkiaRenderPaint::style(RenderPaintStyle style)
{
    switch (style)
    {
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
void SkiaRenderPaint::join(StrokeJoin value)
{
    m_Paint.setStrokeJoin(ToSkia::convert(value));
}
void SkiaRenderPaint::cap(StrokeCap value)
{
    m_Paint.setStrokeCap(ToSkia::convert(value));
}

void SkiaRenderPaint::feather(float value)
{
    m_hasFeather = value != 0;
    if (m_hasFeather)
    {
        m_Paint.setImageFilter(
            SkImageFilters::Blur(value * .5f, value * .5f, nullptr));
    }
    else
    {
        m_Paint.setImageFilter(nullptr);
    }
}

void SkiaRenderPaint::blendMode(BlendMode value)
{
    m_Paint.setBlendMode(ToSkia::convert(value));
}

void SkiaRenderPaint::shader(rcp<RenderShader> rsh)
{
    SkiaRenderShader* sksh = lite_rtti_cast<SkiaRenderShader*>(rsh.get());
    m_Paint.setShader(sksh ? sksh->shader : nullptr);
}

void SkiaRenderer::save()
{
    m_Canvas->save();
    m_opacityStack.push_back(m_opacityStack.back());
}
void SkiaRenderer::restore()
{
    m_Canvas->restore();
    if (m_opacityStack.size() > 1)
    {
        m_opacityStack.pop_back();
    }
}
void SkiaRenderer::transform(const Mat2D& transform)
{
    m_Canvas->concat(ToSkia::convert(transform));
}
void SkiaRenderer::modulateOpacity(float opacity)
{
    m_opacityStack.back() = std::max(0.0f, m_opacityStack.back() * opacity);
}
void SkiaRenderer::drawPath(RenderPath* path, RenderPaint* paint)
{
    LITE_RTTI_CAST_OR_RETURN(skiaRenderPath, SkiaRenderPath*, path);
    LITE_RTTI_CAST_OR_RETURN(skiaRenderPaint, SkiaRenderPaint*, paint);

    SkiaRenderPaint::OverrideStrokeParamsForFeather ospff(skiaRenderPaint);

    float modulatedOpacity = m_opacityStack.back();
    if (modulatedOpacity != 1.0f)
    {
        // Apply modulated opacity using a color filter on the paint.
        // This is more efficient than saveLayer as it doesn't allocate
        // an offscreen buffer.
        // We scale all color components (RGBA) by opacity since Skia uses
        // pre-multiplied alpha.
        SkPaint modulatedPaint(skiaRenderPaint->paint());

        float matrix[20] = {
            // clang-format off
            modulatedOpacity, 0, 0, 0, 0,  // R
            0, modulatedOpacity, 0, 0, 0,  // G
            0, 0, modulatedOpacity, 0, 0,  // B
            0, 0, 0, modulatedOpacity, 0,  // A
            // clang-format on
        };

        auto opacityFilter = SkColorFilters::Matrix(matrix);
        auto existingFilter = modulatedPaint.refColorFilter();
        if (existingFilter)
        {
            modulatedPaint.setColorFilter(
                opacityFilter->makeComposed(existingFilter));
        }
        else
        {
            modulatedPaint.setColorFilter(opacityFilter);
        }
        m_Canvas->drawPath(skiaRenderPath->path(), modulatedPaint);
    }
    else
    {
        m_Canvas->drawPath(skiaRenderPath->path(), skiaRenderPaint->paint());
    }
}

void SkiaRenderer::clipPath(RenderPath* path)
{
    LITE_RTTI_CAST_OR_RETURN(skPath, SkiaRenderPath*, path);
    m_Canvas->clipPath(skPath->path(), true);
}

void SkiaRenderer::drawImage(const RenderImage* image,
                             const rive::ImageSampler,
                             BlendMode blendMode,
                             float opacity)
{
    LITE_RTTI_CAST_OR_RETURN(skiaImage, const SkiaRenderImage*, image);
    float finalOpacity = std::max(0.0f, opacity * m_opacityStack.back());
    SkPaint paint;
    paint.setAlphaf(finalOpacity);
    paint.setBlendMode(ToSkia::convert(blendMode));
    m_Canvas->drawImage(skiaImage->skImage(), 0.0f, 0.0f, gSampling, &paint);
}

#define SKIA_BUG_13047

void SkiaRenderer::drawImageMesh(const RenderImage* image,
                                 const rive::ImageSampler,
                                 rcp<RenderBuffer> vertices,
                                 rcp<RenderBuffer> uvCoords,
                                 rcp<RenderBuffer> indices,
                                 uint32_t vertexCount,
                                 uint32_t indexCount,
                                 BlendMode blendMode,
                                 float opacity)
{
    LITE_RTTI_CAST_OR_RETURN(skImage, const SkiaRenderImage*, image);
    LITE_RTTI_CAST_OR_RETURN(skVertices, DataRenderBuffer*, vertices.get());
    LITE_RTTI_CAST_OR_RETURN(skUVCoords, DataRenderBuffer*, uvCoords.get());
    LITE_RTTI_CAST_OR_RETURN(skIndices, DataRenderBuffer*, indices.get());

    // need our buffers and counts to agree
    assert(vertices->sizeInBytes() == vertexCount * sizeof(Vec2D));
    assert(uvCoords->sizeInBytes() == vertexCount * sizeof(Vec2D));
    assert(indices->sizeInBytes() == indexCount * sizeof(uint16_t));

    SkMatrix scaleM;

    auto uvs = (const SkPoint*)skUVCoords->vecs();

#ifdef SKIA_BUG_13047
    // The local matrix is ignored for drawVertices, so we have to manually
    // scale the UVs to match Skia's convention...
    std::vector<SkPoint> scaledUVs(vertexCount);
    for (uint32_t i = 0; i < vertexCount; ++i)
    {
        scaledUVs[i] = {uvs[i].fX * image->width(),
                        uvs[i].fY * image->height()};
    }
    uvs = scaledUVs.data();
#else
    // We do this because our UVs are normalized, but Skia expects them to be
    // sized to the shader (i.e. 0..width, 0..height).
    // To accomdate this, we effectively scaling the image down to 0..1 to
    // match the scale of the UVs.
    scaleM = SkMatrix::Scale(2.0f / image->width(), 2.0f / image->height());
#endif

    auto skiaImage = skImage->skImage();
    auto shader = skiaImage->makeShader(SkTileMode::kClamp,
                                        SkTileMode::kClamp,
                                        gSampling,
                                        &scaleM);

    float finalOpacity = std::max(0.0f, opacity * m_opacityStack.back());
    SkPaint paint;
    paint.setAlphaf(finalOpacity);
    paint.setBlendMode(ToSkia::convert(blendMode));
    paint.setShader(shader);

    const SkColor* no_colors = nullptr;
    auto vertexMode = SkVertices::kTriangles_VertexMode;
    auto vt = SkVertices::MakeCopy(vertexMode,
                                   vertexCount,
                                   (const SkPoint*)skVertices->vecs(),
                                   uvs,
                                   no_colors,
                                   indexCount,
                                   skIndices->u16s());

    // The blend mode is ignored if we don't have colors && uvs
    m_Canvas->drawVertices(vt, SkBlendMode::kModulate, paint);
}

SkiaRenderImage::SkiaRenderImage(sk_sp<SkImage> image) :
    m_SkImage(std::move(image))
{
    m_Width = m_SkImage->width();
    m_Height = m_SkImage->height();
}

// Factory

rcp<RenderBuffer> SkiaFactory::makeRenderBuffer(RenderBufferType type,
                                                RenderBufferFlags flags,
                                                size_t sizeInBytes)
{
    return make_rcp<DataRenderBuffer>(type, flags, sizeInBytes);
}

rcp<RenderShader> SkiaFactory::makeLinearGradient(
    float sx,
    float sy,
    float ex,
    float ey,
    const ColorInt colors[], // [count]
    const float stops[],     // [count]
    size_t count)
{
    const SkPoint pts[] = {{sx, sy}, {ex, ey}};
    auto sh = SkGradientShader::MakeLinear(pts,
                                           (const SkColor*)colors,
                                           stops,
                                           count,
                                           SkTileMode::kClamp);
    return rcp<RenderShader>(new SkiaRenderShader(std::move(sh)));
}

rcp<RenderShader> SkiaFactory::makeRadialGradient(
    float cx,
    float cy,
    float radius,
    const ColorInt colors[], // [count]
    const float stops[],     // [count]
    size_t count)
{
    auto sh = SkGradientShader::MakeRadial({cx, cy},
                                           radius,
                                           (const SkColor*)colors,
                                           stops,
                                           count,
                                           SkTileMode::kClamp);
    return rcp<RenderShader>(new SkiaRenderShader(std::move(sh)));
}

rcp<RenderPath> SkiaFactory::makeRenderPath(RawPath& rawPath, FillRule fillRule)
{
    const bool isVolatile = false; // ???
    const SkScalar* conicWeights = nullptr;
    const int conicWeightCount = 0;
    return make_rcp<SkiaRenderPath>(
        SkPath::Make(reinterpret_cast<const SkPoint*>(rawPath.points().data()),
                     rawPath.points().size(),
                     (uint8_t*)rawPath.verbs().data(),
                     rawPath.verbs().size(),
                     conicWeights,
                     conicWeightCount,
                     ToSkia::convert(fillRule),
                     isVolatile));
}

rcp<RenderPath> SkiaFactory::makeEmptyRenderPath()
{
    return make_rcp<SkiaRenderPath>();
}

rcp<RenderPaint> SkiaFactory::makeRenderPaint()
{
    return make_rcp<SkiaRenderPaint>();
}

rcp<RenderImage> SkiaFactory::decodeImage(Span<const uint8_t> encoded)
{
    sk_sp<SkData> data = SkData::MakeWithCopy(encoded.data(), encoded.size());
    auto image = SkImage::MakeFromEncoded(data);

    if (image)
    {
        // Our optimized skia build seems to have broken lazy-image decode.
        // As a work-around for now, force the image to be decoded.
        image = image->makeRasterImage();
    }
    else
    {
        // Skia failed, so let's try the platform
        ImageInfo info;
        auto pixels = this->platformDecode(encoded, &info);
        if (pixels.size() > 0)
        {
            auto ct = info.colorType == ColorType::rgba
                          ? kRGBA_8888_SkColorType
                          : kBGRA_8888_SkColorType;
            auto at = info.alphaType == AlphaType::premul ? kPremul_SkAlphaType
                                                          : kOpaque_SkAlphaType;
            auto skinfo = SkImageInfo::Make(info.width, info.height, ct, at);
            image =
                SkImage::MakeRasterCopy({skinfo, pixels.data(), info.rowBytes});
        }
    }

    return image ? make_rcp<SkiaRenderImage>(std::move(image)) : nullptr;
}
