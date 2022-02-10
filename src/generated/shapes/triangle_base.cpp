#include "rive/generated/shapes/triangle_base.hpp"
#include "rive/shapes/triangle.hpp"

using namespace rive;

Core* TriangleBase::clone() const
{
    auto cloned = new Triangle();
    cloned->copy(*this);
    return cloned;
}
