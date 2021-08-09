#include "rive/shapes/paint/radial_gradient.hpp"
#include "rive/renderer.hpp"

using namespace rive;

void RadialGradient::makeGradient(const Vec2D& start, const Vec2D& end)
{
	renderPaint()->radialGradient(start[0], start[1], end[0], end[1]);
}