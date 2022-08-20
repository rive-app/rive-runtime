#include "utils/no_op_factory.hpp"
#include "utils/no_op_renderer.hpp"

using namespace rive;

namespace {
class NoOpRenderImage : public RenderImage {
public:
};

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
} // namespace

rcp<RenderBuffer> NoOpFactory::makeBufferU16(Span<const uint16_t>) { return nullptr; }
rcp<RenderBuffer> NoOpFactory::makeBufferU32(Span<const uint32_t>) { return nullptr; }
rcp<RenderBuffer> NoOpFactory::makeBufferF32(Span<const float>) { return nullptr; }

rcp<RenderShader> NoOpFactory::makeLinearGradient(float sx,
                                                  float sy,
                                                  float ex,
                                                  float ey,
                                                  const ColorInt colors[], // [count]
                                                  const float stops[],     // [count]
                                                  size_t count) {
    return nullptr;
}

rcp<RenderShader> NoOpFactory::makeRadialGradient(float cx,
                                                  float cy,
                                                  float radius,
                                                  const ColorInt colors[], // [count]
                                                  const float stops[],     // [count]
                                                  size_t count) {
    return nullptr;
}

std::unique_ptr<RenderPath> NoOpFactory::makeRenderPath(Span<const Vec2D> points,
                                                        Span<const PathVerb> verbs,
                                                        FillRule) {
    return std::make_unique<NoOpRenderPath>();
}

std::unique_ptr<RenderPath> NoOpFactory::makeEmptyRenderPath() {
    return std::make_unique<NoOpRenderPath>();
}

std::unique_ptr<RenderPaint> NoOpFactory::makeRenderPaint() {
    return std::make_unique<NoOpRenderPaint>();
}

std::unique_ptr<RenderImage> NoOpFactory::decodeImage(Span<const uint8_t>) {
    return std::make_unique<NoOpRenderImage>();
}
