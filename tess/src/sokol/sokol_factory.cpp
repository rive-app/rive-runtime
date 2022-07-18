#include "rive/tess/sokol/sokol_factory.hpp"

using namespace rive;

class NoOpRenderPaint : public RenderPaint {
public:
    void color(unsigned int value) override {}
    void style(RenderPaintStyle value) override {}
    void thickness(float value) override {}
    void join(StrokeJoin value) override {}
    void cap(StrokeCap value) override {}
    void blendMode(BlendMode value) override {}
    void shader(rcp<RenderShader>) override {}
};

class NoOpRenderPath : public RenderPath {
public:
    void reset() override {}

    void fillRule(FillRule value) override {}
    void addPath(CommandPath* path, const Mat2D& transform) override {}
    void addRenderPath(RenderPath* path, const Mat2D& transform) override {}

    void moveTo(float x, float y) override {}
    void lineTo(float x, float y) override {}
    void cubicTo(float ox, float oy, float ix, float iy, float x, float y) override {}
    void close() override {}
};

SokolFactory::SokolFactory() {}

rcp<RenderShader> SokolFactory::makeLinearGradient(float sx,
                                                   float sy,
                                                   float ex,
                                                   float ey,
                                                   const ColorInt colors[], // [count]
                                                   const float stops[],     // [count]
                                                   size_t count,
                                                   RenderTileMode,
                                                   const Mat2D* localMatrix) {
    return nullptr;
}

rcp<RenderShader> SokolFactory::makeRadialGradient(float cx,
                                                   float cy,
                                                   float radius,
                                                   const ColorInt colors[], // [count]
                                                   const float stops[],     // [count]
                                                   size_t count,
                                                   RenderTileMode,
                                                   const Mat2D* localMatrix) {
    return nullptr;
}

// Returns a full-formed RenderPath -- can be treated as immutable
std::unique_ptr<RenderPath>
SokolFactory::makeRenderPath(Span<const Vec2D> points, Span<const uint8_t> verbs, FillRule) {
    return std::make_unique<NoOpRenderPath>();
}

std::unique_ptr<RenderPath> SokolFactory::makeEmptyRenderPath() {
    return std::make_unique<NoOpRenderPath>();
}

std::unique_ptr<RenderPaint> SokolFactory::makeRenderPaint() {
    return std::make_unique<NoOpRenderPaint>();
}