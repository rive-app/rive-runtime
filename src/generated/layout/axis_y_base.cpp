#include "rive/generated/layout/axis_y_base.hpp"
#include "rive/layout/axis_y.hpp"

using namespace rive;

Core* AxisYBase::clone() const
{
    auto cloned = new AxisY();
    cloned->copy(*this);
    return cloned;
}
