#include "rive/shapes/paint/radial_gradient.hpp"
#include "rive/renderer.hpp"

using namespace rive;

void RadialGradient::makeGradient(Vec2D start, Vec2D end,
                                  const ColorInt colors[], const float stops[], size_t count) {
    auto paint = renderPaint();
    paint->shader(makeRadialGradient(start[0], start[1], end[0],
                                     colors, stops, count, RenderTileMode::clamp));
}
