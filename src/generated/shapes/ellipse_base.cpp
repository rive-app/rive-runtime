#include "rive/generated/shapes/ellipse_base.hpp"
#include "rive/shapes/ellipse.hpp"

using namespace rive;

Core* EllipseBase::clone() const
{
    auto cloned = new Ellipse();
    cloned->copy(*this);
    return cloned;
}
