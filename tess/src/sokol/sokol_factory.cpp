#include "rive/tess/sokol/sokol_factory.hpp"

using namespace rive;

class NoOpRenderPaint : public LITE_RTTI_OVERRIDE(RenderPaint, NoOpRenderPaint)
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

class NoOpRenderPath : public LITE_RTTI_OVERRIDE(RenderPath, NoOpRenderPath)
{
public:
    void rewind() override {}

    void fillRule(FillRule value) override {}
    void addPath(CommandPath* path, const Mat2D& transform) override {}
    void addRenderPath(RenderPath* path, const Mat2D& transform) override {}
    void addRawPath(const RawPath& path) override {}

    void moveTo(float x, float y) override {}
    void lineTo(float x, float y) override {}
    void cubicTo(float ox, float oy, float ix, float iy, float x, float y)
        override
    {}
    void close() override {}
};

SokolFactory::SokolFactory() {}
