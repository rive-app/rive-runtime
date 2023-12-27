#include "utils/no_op_factory.hpp"

using namespace rive;

namespace
{
class NoOpRenderImage : public RenderImage
{
public:
};

class NoOpRenderPaint : public RenderPaint
{
public:
    void color(unsigned int value) override {}
    void style(RenderPaintStyle value) override {}
    void thickness(float value) override {}
    void join(StrokeJoin value) override {}
    void cap(StrokeCap value) override {}
    void blendMode(BlendMode value) override {}
    void shader(rcp<RenderShader>) override {}
    void invalidateStroke() override {}
};

class NoOpRenderPath : public RenderPath
{
public:
    void rewind() override {}

    void fillRule(FillRule value) override {}
    void addPath(CommandPath* path, const Mat2D& transform) override {}
    void addRenderPath(RenderPath* path, const Mat2D& transform) override {}

    void moveTo(float x, float y) override {}
    void lineTo(float x, float y) override {}
    void cubicTo(float ox, float oy, float ix, float iy, float x, float y) override {}
    void close() override {}
};
} // namespace

rcp<RenderBuffer> NoOpFactory::makeRenderBuffer(RenderBufferType, RenderBufferFlags, size_t)
{
    return nullptr;
}

rcp<RenderShader> NoOpFactory::makeLinearGradient(float sx,
                                                  float sy,
                                                  float ex,
                                                  float ey,
                                                  const ColorInt colors[], // [count]
                                                  const float stops[],     // [count]
                                                  size_t count)
{
    return nullptr;
}

rcp<RenderShader> NoOpFactory::makeRadialGradient(float cx,
                                                  float cy,
                                                  float radius,
                                                  const ColorInt colors[], // [count]
                                                  const float stops[],     // [count]
                                                  size_t count)
{
    return nullptr;
}

rcp<RenderPath> NoOpFactory::makeRenderPath(RawPath&, FillRule)
{
    return make_rcp<NoOpRenderPath>();
}

rcp<RenderPath> NoOpFactory::makeEmptyRenderPath() { return make_rcp<NoOpRenderPath>(); }

rcp<RenderPaint> NoOpFactory::makeRenderPaint() { return make_rcp<NoOpRenderPaint>(); }

rcp<RenderImage> NoOpFactory::decodeImage(Span<const uint8_t>) { return nullptr; }
