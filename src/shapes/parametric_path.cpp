#include "rive/math/aabb.hpp"
#include "rive/shapes/parametric_path.hpp"

using namespace rive;

AABB ParametricPath::computeIntrinsicSize(AABB min, AABB max)
{
    return AABB::fromLTWH(0, 0, width(), height());
}

void ParametricPath::controlSize(AABB size)
{
    width(size.width());
    height(size.height());
    markWorldTransformDirty();
}

void ParametricPath::widthChanged() { markPathDirty(); }
void ParametricPath::heightChanged() { markPathDirty(); }
void ParametricPath::originXChanged() { markPathDirty(); }
void ParametricPath::originYChanged() { markPathDirty(); }