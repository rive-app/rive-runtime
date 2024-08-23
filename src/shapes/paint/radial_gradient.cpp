#include "rive/shapes/paint/radial_gradient.hpp"
#include "rive/artboard.hpp"
#include "rive/factory.hpp"

using namespace rive;

void RadialGradient::makeGradient(RenderPaint* renderPaint,
                                  Vec2D start,
                                  Vec2D end,
                                  const ColorInt colors[],
                                  const float stops[],
                                  size_t count) const
{
    auto factory = artboard()->factory();
    renderPaint->shader(factory->makeRadialGradient(start.x,
                                                    start.y,
                                                    Vec2D::distance(start, end),
                                                    colors,
                                                    stops,
                                                    count));
}
