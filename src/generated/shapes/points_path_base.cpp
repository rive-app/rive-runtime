#include "rive/generated/shapes/points_path_base.hpp"
#include "rive/shapes/points_path.hpp"

using namespace rive;

Core* PointsPathBase::clone() const
{
    auto cloned = new PointsPath();
    cloned->copy(*this);
    return cloned;
}
