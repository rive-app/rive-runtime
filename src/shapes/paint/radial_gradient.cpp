#include "rive/shapes/paint/radial_gradient.hpp"
#include "rive/artboard.hpp"
#include "rive/factory.hpp"

using namespace rive;

void RadialGradient::makeGradient(
    Vec2D start, Vec2D end, const ColorInt colors[], const float stops[], size_t count) {
    auto factory = artboard()->factory();
    renderPaint()->shader(factory->makeRadialGradient(start[0],
                                                      start[1],
                                                      Vec2D::distance(start, end),
                                                      colors,
                                                      stops,
                                                      count,
                                                      RenderTileMode::clamp));
}
