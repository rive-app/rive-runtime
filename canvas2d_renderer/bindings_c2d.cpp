#include "bindings_c2d.hpp"

#include "rive/factory.hpp"
#include "rive/renderer.hpp"
#include "rive/math/path_types.hpp"
#include "utils/factory_utils.hpp"

#include "js_alignment.hpp"

#include <emscripten.h>
#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <vector>

using namespace emscripten;

class RendererWrapper : public wrapper<rive::Renderer>
{
public:
    EMSCRIPTEN_WRAPPER(RendererWrapper);

    void save() override { call<void>("save"); }

    void restore() override { call<void>("restore"); }

    void clear() { call<void>("clear"); }

    void transform(const rive::Mat2D& transform) override
    {
        call<void>("transform",
                   transform.xx(),
                   transform.xy(),
                   transform.yx(),
                   transform.yy(),
                   transform.tx(),
                   transform.ty());
    }

    void align(rive::Fit fit, JsAlignment alignment, const rive::AABB& foo, const rive::AABB& bar)
    {
        transform(computeAlignment(fit, convertAlignment(alignment), foo, bar));
    }

    void drawPath(rive::RenderPath* path, rive::RenderPaint* paint) override
    {
        call<void>("_drawPath", path, paint);
    }

    void clipPath(rive::RenderPath* path) override { call<void>("_clipPath", path); }

    void drawImage(const rive::RenderImage* image, rive::BlendMode value, float opacity) override
    {
        call<void>("_drawImage", image, value, opacity);
    }

    void drawImageMesh(const rive::RenderImage* image,
                       rive::rcp<rive::RenderBuffer> vertices_f32,
                       rive::rcp<rive::RenderBuffer> uvCoords_f32,
                       rive::rcp<rive::RenderBuffer> indices_u16,
                       uint32_t vertexCount,
                       uint32_t indexCount,
                       rive::BlendMode value,
                       float opacity) override
    {}
};

class RenderPathWrapper : public wrapper<rive::RenderPath>
{
public:
    EMSCRIPTEN_WRAPPER(RenderPathWrapper);

    void rewind() override { call<void>("rewind"); }

    void addRenderPath(rive::RenderPath* path, const rive::Mat2D& transform) override
    {
        float xx = transform.xx();
        float xy = transform.xy();
        float yx = transform.yx();
        float yy = transform.yy();
        float tx = transform.tx();
        float ty = transform.ty();
        call<void>("addPath", path, xx, xy, yx, yy, tx, ty);
    }
    void fillRule(rive::FillRule value) override { call<void>("fillRule", value); }

    void moveTo(float x, float y) override { call<void>("moveTo", x, y); }
    void lineTo(float x, float y) override { call<void>("lineTo", x, y); }
    void cubicTo(float ox, float oy, float ix, float iy, float x, float y) override
    {
        call<void>("cubicTo", ox, oy, ix, iy, x, y);
    }
    void close() override { call<void>("close"); }
};

class RenderPaintWrapper;
class GradientShader : public rive::RenderShader
{
private:
    std::vector<float> m_Stops;
    std::vector<rive::ColorInt> m_Colors;

public:
    GradientShader(const rive::ColorInt colors[], const float stops[], int count) :
        m_Stops(stops, stops + count), m_Colors(colors, colors + count)
    {}

    void passStopsToJS(const RenderPaintWrapper& wrapper);

    virtual void passToJS(const RenderPaintWrapper& wrapper) = 0;
};

class LinearGradientShader : public GradientShader
{
private:
    float m_StartX;
    float m_StartY;
    float m_EndX;
    float m_EndY;

public:
    LinearGradientShader(const rive::ColorInt colors[],
                         const float stops[],
                         int count,
                         float sx,
                         float sy,
                         float ex,
                         float ey) :
        GradientShader(colors, stops, count), m_StartX(sx), m_StartY(sy), m_EndX(ex), m_EndY(ey)
    {}

    void passToJS(const RenderPaintWrapper& wrapper) override;
};

class RadialGradientShader : public GradientShader
{
private:
    float m_CenterX;
    float m_CenterY;
    float m_Radius;

public:
    RadialGradientShader(const rive::ColorInt colors[],
                         const float stops[],
                         int count,
                         float cx,
                         float cy,
                         float r) :
        GradientShader(colors, stops, count), m_CenterX(cx), m_CenterY(cy), m_Radius(r)
    {}

    void passToJS(const RenderPaintWrapper& wrapper) override;
};

class RenderPaintWrapper : public wrapper<rive::RenderPaint>
{
public:
    EMSCRIPTEN_WRAPPER(RenderPaintWrapper);

    void color(unsigned int value) override { call<void>("color", value); }
    void thickness(float value) override { call<void>("thickness", value); }
    void join(rive::StrokeJoin value) override { call<void>("join", value); }
    void cap(rive::StrokeCap value) override { call<void>("cap", value); }
    void blendMode(rive::BlendMode value) override { call<void>("blendMode", value); }

    void style(rive::RenderPaintStyle value) override { call<void>("style", value); }

    void shader(rive::rcp<rive::RenderShader> shader) override
    {
        if (shader == nullptr)
        {
            call<void>("clearGradient");
            return;
        }
        static_cast<GradientShader*>(shader.get())->passToJS(*this);
    }
    void invalidateStroke() override {}
};

void GradientShader::passStopsToJS(const RenderPaintWrapper& wrapper)
{
    // Consider passing in a bulk op encoding into a single array.
    for (std::size_t i = 0; i < m_Stops.size(); i++)
    {
        wrapper.call<void>("addStop", m_Colors[i], m_Stops[i]);
    }
}

void LinearGradientShader::passToJS(const RenderPaintWrapper& wrapper)
{
    wrapper.call<void>("linearGradient", m_StartX, m_StartY, m_EndX, m_EndY);
    passStopsToJS(wrapper);
}

void RadialGradientShader::passToJS(const RenderPaintWrapper& wrapper)
{
    wrapper.call<void>("radialGradient", m_CenterX, m_CenterY, m_CenterX + m_Radius, m_CenterY);
    passStopsToJS(wrapper);
}

class RenderImageWrapper : public wrapper<rive::RenderImage>
{
public:
    EMSCRIPTEN_WRAPPER(RenderImageWrapper);

    void size(int width, int height)
    {
        m_Width = width;
        m_Height = height;
    }
};

namespace rive
{
class C2DFactory : public Factory
{
    rcp<RenderBuffer> makeRenderBuffer(RenderBufferType type,
                                       RenderBufferFlags,
                                       size_t sizeInBytes) override
    {
        return make_rcp<DataRenderBuffer>(type, flags, sizeInBytes);
    }

    rcp<RenderShader> makeLinearGradient(float sx,
                                         float sy,
                                         float ex,
                                         float ey,
                                         const ColorInt colors[], // [count]
                                         const float stops[],     // [count]
                                         size_t count) override
    {
        return rcp<RenderShader>(new LinearGradientShader(colors, stops, count, sx, sy, ex, ey));
    }
    rcp<RenderShader> makeRadialGradient(float cx,
                                         float cy,
                                         float radius,
                                         const ColorInt colors[], // [count]
                                         const float stops[],     // [count]
                                         size_t count) override
    {
        return rcp<RenderShader>(new RadialGradientShader(colors, stops, count, cx, cy, radius));
    }

    std::unique_ptr<RenderPath> makeRenderPath(RawPath& path, FillRule fr) override
    {
        val renderPath = val::module_property("renderFactory").call<val>("makeRenderPath");
        auto ptr = renderPath.as<RenderPath*>(allow_raw_pointers());

        // It might be faster to do this on the JS side, and just pass up the arrays...
        // for now, we do it one segment at a time (each turns into an up-call to JS)
        ptr->fillRule(fr);
        const rive::Vec2D* pts = path.points().data();
        for (auto v : path.verbs())
        {
            switch ((PathVerb)v)
            {
                case PathVerb::move:
                    ptr->move(*pts++);
                    break;
                case PathVerb::line:
                    ptr->line(*pts++);
                    break;
                case PathVerb::cubic:
                    ptr->cubic(pts[0], pts[1], pts[2]);
                    pts += 3;
                    break;
                case PathVerb::close:
                    ptr->close();
                    break;
                default:
                    assert(false); // unexpected verb
            }
        }
        assert(pts - path.points().data() == path.points().size());

        return std::unique_ptr<RenderPath>(ptr);
    }

    std::unique_ptr<RenderPath> makeEmptyRenderPath() override
    {
        val renderPath = val::module_property("renderFactory").call<val>("makeRenderPath");
        auto ptr = renderPath.as<RenderPath*>(allow_raw_pointers());
        return std::unique_ptr<RenderPath>(ptr);
    }

    std::unique_ptr<RenderPaint> makeRenderPaint() override
    {
        val renderPaint = val::module_property("renderFactory").call<val>("makeRenderPaint");
        auto ptr = renderPaint.as<RenderPaint*>(allow_raw_pointers());
        return std::unique_ptr<RenderPaint>(ptr);
    }

    std::unique_ptr<RenderImage> decodeImage(Span<const uint8_t> bytes) override { return nullptr; }
};

std::unique_ptr<rive::Factory> MakeC2DFactory() { return std::make_unique<rive::C2DFactory>(); }

std::unique_ptr<rive::Renderer> MakeC2DRenderer(const char* canvasID)
{
    val renderer =
        val::module_property("renderFactory")
            .call<val>("makeRenderer", reinterpret_cast<intptr_t>(canvasID), strlen(canvasID));
    auto ptr = renderer.as<rive::Renderer*>(allow_raw_pointers());
    return std::unique_ptr<rive::Renderer>(ptr);
}

void ClearC2DRenderer(rive::Renderer* renderer)
{
    static_cast<RendererWrapper*>(renderer)->clear();
}
} // namespace rive

EMSCRIPTEN_KEEPALIVE
EMSCRIPTEN_BINDINGS(RiveWASM_C2D)
{
    class_<rive::Renderer>("Renderer")
        .function("save", &RendererWrapper::save, pure_virtual(), allow_raw_pointers())
        .function("restore", &RendererWrapper::restore, pure_virtual(), allow_raw_pointers())
        .function("transform", &RendererWrapper::transform, pure_virtual(), allow_raw_pointers())
        .function("drawPath", &RendererWrapper::drawPath, pure_virtual(), allow_raw_pointers())
        .function("clipPath", &RendererWrapper::clipPath, pure_virtual(), allow_raw_pointers())
        .function("align", &RendererWrapper::align, pure_virtual(), allow_raw_pointers())
        .allow_subclass<RendererWrapper>("RendererWrapper");

    class_<rive::RenderPath>("RenderPath")
        .function("rewind", &RenderPathWrapper::rewind, pure_virtual(), allow_raw_pointers())
        .function("addPath",
                  &RenderPathWrapper::addRenderPath,
                  pure_virtual(),
                  allow_raw_pointers())
        .function("fillRule", &RenderPathWrapper::fillRule, pure_virtual())
        .function("moveTo", &RenderPathWrapper::moveTo, pure_virtual(), allow_raw_pointers())
        .function("lineTo", &RenderPathWrapper::lineTo, pure_virtual(), allow_raw_pointers())
        .function("cubicTo", &RenderPathWrapper::cubicTo, pure_virtual(), allow_raw_pointers())
        .function("close", &RenderPathWrapper::close, pure_virtual(), allow_raw_pointers())
        .allow_subclass<RenderPathWrapper>("RenderPathWrapper");
    enum_<rive::RenderPaintStyle>("RenderPaintStyle")
        .value("fill", rive::RenderPaintStyle::fill)
        .value("stroke", rive::RenderPaintStyle::stroke);

    enum_<rive::FillRule>("FillRule")
        .value("nonZero", rive::FillRule::nonZero)
        .value("evenOdd", rive::FillRule::evenOdd);

    enum_<rive::StrokeCap>("StrokeCap")
        .value("butt", rive::StrokeCap::butt)
        .value("round", rive::StrokeCap::round)
        .value("square", rive::StrokeCap::square);

    enum_<rive::StrokeJoin>("StrokeJoin")
        .value("miter", rive::StrokeJoin::miter)
        .value("round", rive::StrokeJoin::round)
        .value("bevel", rive::StrokeJoin::bevel);

    enum_<rive::BlendMode>("BlendMode")
        .value("srcOver", rive::BlendMode::srcOver)
        .value("screen", rive::BlendMode::screen)
        .value("overlay", rive::BlendMode::overlay)
        .value("darken", rive::BlendMode::darken)
        .value("lighten", rive::BlendMode::lighten)
        .value("colorDodge", rive::BlendMode::colorDodge)
        .value("colorBurn", rive::BlendMode::colorBurn)
        .value("hardLight", rive::BlendMode::hardLight)
        .value("softLight", rive::BlendMode::softLight)
        .value("difference", rive::BlendMode::difference)
        .value("exclusion", rive::BlendMode::exclusion)
        .value("multiply", rive::BlendMode::multiply)
        .value("hue", rive::BlendMode::hue)
        .value("saturation", rive::BlendMode::saturation)
        .value("color", rive::BlendMode::color)
        .value("luminosity", rive::BlendMode::luminosity);

    class_<rive::rcp<rive::RenderShader>>("RenderShader");

    class_<rive::RenderPaint>("RenderPaint")
        .function("color", &RenderPaintWrapper::color, pure_virtual(), allow_raw_pointers())

        .function("style", &RenderPaintWrapper::style, pure_virtual(), allow_raw_pointers())
        .function("thickness", &RenderPaintWrapper::thickness, pure_virtual(), allow_raw_pointers())
        .function("join", &RenderPaintWrapper::join, pure_virtual(), allow_raw_pointers())
        .function("cap", &RenderPaintWrapper::cap, pure_virtual(), allow_raw_pointers())
        .function("blendMode", &RenderPaintWrapper::blendMode, pure_virtual(), allow_raw_pointers())
        .function("shader", &RenderPaintWrapper::shader, pure_virtual(), allow_raw_pointers())
        .allow_subclass<RenderPaintWrapper>("RenderPaintWrapper");

    class_<rive::RenderImage>("RenderImage")
        //      .function("decode", &RenderImageWrapper::decode, pure_virtual(),
        //      allow_raw_pointers())
        .function("size", &RenderImageWrapper::size)
        .allow_subclass<RenderImageWrapper>("RenderImageWrapper");
}
