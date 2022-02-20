#include "no_op_renderer.hpp"
#include <rive/renderer.hpp>

namespace rive {
    RenderPaint* makeRenderPaint() { return new NoOpRenderPaint(); }
    RenderPath* makeRenderPath() { return new NoOpRenderPath(); }
    RenderImage* makeRenderImage() { return new NoOpRenderImage(); }

    rcp<RenderShader> makeLinearGradient(float sx, float sy,
                                         float ex, float ey,
                                         const ColorInt colors[],
                                         const float stops[],
                                         int count,
                                         RenderTileMode,
                                         const Mat2D* localMatrix) {
        return nullptr;
    }

    rcp<RenderShader> makeRadialGradient(float cx, float cy, float radius,
                                         const ColorInt colors[],
                                         const float stops[],
                                         int count,
                                         RenderTileMode,
                                         const Mat2D* localMatrix) {
        return nullptr;
    }

    rcp<RenderShader> makeSweepGradient(float cx, float cy,
                                        const ColorInt colors[],
                                        const float stops[],
                                        int count,
                                        const Mat2D* localMatrix) {
        return nullptr;
    }

} // namespace rive
