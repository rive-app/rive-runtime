#include "rive/shapes/paint/angular_gradient.hpp"
#include "rive/artboard.hpp"
#include "rive/factory.hpp"
#include "rive/math/mat2d.hpp"

using namespace rive;

// The sweep is the one gradient whose geometry is entirely its transform.
// Linear and radial hand their points to the factory, which folds them into
// the paint matrix as coefficients; `makeAngularGradient` takes colours and
// stops alone and leaves that matrix as the gradient transform alone (see
// `getGradientMatrixAndCoord`: "paintMatrixOut is unchanged" for angular). So
// where the sweep sits, which way it starts and how it is scaled all come
// from here.
void AngularGradient::makeGradient(
    RenderPaint* renderPaint,
    Vec2D start,
    Vec2D end,
    const ColorInt colors[],
    const float stops[],
    size_t count) const
{
  auto factory = artboard()->factory();
  renderPaint->shader(factory->makeAngularGradient(colors, stops, count));
  // Scale, then rotate, then move: the order a transform is read in, so a
  // rotation turns the sweep about its own centre rather than the origin.
  renderPaint->shaderTransform(
      Mat2D::fromTranslate(transformX(), transformY()) *
      Mat2D::fromRotation(transformRotation()) *
      Mat2D::fromScale(transformScaleX(), transformScaleY()));
}
