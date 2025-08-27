#include "rive/generated/shapes/polygon_base.hpp"
#include "rive/shapes/polygon.hpp"

using namespace rive;

Core* PolygonBase::clone() const
{
    auto cloned = new Polygon();
    cloned->copy(*this);
    return cloned;
}
