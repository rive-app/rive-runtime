#include "rive/generated/shapes/paint/dash_path_base.hpp"
#include "rive/shapes/paint/dash_path.hpp"

using namespace rive;

Core* DashPathBase::clone() const
{
    auto cloned = new DashPath();
    cloned->copy(*this);
    return cloned;
}
