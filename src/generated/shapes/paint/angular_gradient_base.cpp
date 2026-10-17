#include "rive/generated/shapes/paint/angular_gradient_base.hpp"
#include "rive/shapes/paint/angular_gradient.hpp"

using namespace rive;

Core* AngularGradientBase::clone() const
{
  auto cloned = new AngularGradient();
  cloned->copy(*this);
  return cloned;
}
