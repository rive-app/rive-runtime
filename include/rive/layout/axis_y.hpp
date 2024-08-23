#ifndef _RIVE_AXIS_Y_HPP_
#define _RIVE_AXIS_Y_HPP_
#include "rive/generated/layout/axis_y_base.hpp"
#include <stdio.h>
namespace rive
{
class AxisY : public AxisYBase
{
public:
    StatusCode onAddedDirty(CoreContext* context) override;
};
} // namespace rive

#endif