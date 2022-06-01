#include "no_op_factory.hpp"
#include "no_op_renderer.hpp"

using namespace rive;

NoOpFactory gNoOpFactory;

rcp<RenderBuffer> NoOpFactory::makeBufferU16(Span<const uint16_t>) { return nullptr; }
rcp<RenderBuffer> NoOpFactory::makeBufferU32(Span<const uint32_t>) { return nullptr; }
rcp<RenderBuffer> NoOpFactory::makeBufferF32(Span<const float>) { return nullptr; }

rcp<RenderShader> NoOpFactory::makeLinearGradient(float sx, float sy,
                                                float ex, float ey,
                                                const ColorInt colors[],    // [count]
                                                const float stops[],        // [count]
                                                size_t count,
                                                RenderTileMode,
                                                const Mat2D* localMatrix) { return nullptr; }

rcp<RenderShader> NoOpFactory::makeRadialGradient(float cx, float cy, float radius,
                                                const ColorInt colors[],    // [count]
                                                const float stops[],        // [count]
                                                size_t count,
                                                RenderTileMode,
                                                const Mat2D* localMatrix) { return nullptr; }

std::unique_ptr<RenderPath> NoOpFactory::makeRenderPath(Span<const Vec2D> points,
                                                        Span<const uint8_t> verbs,
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
