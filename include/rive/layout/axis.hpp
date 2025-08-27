#ifndef _RIVE_AXIS_HPP_
#define _RIVE_AXIS_HPP_
#include "rive/generated/layout/axis_base.hpp"
#include <stdio.h>
namespace rive
{
enum class AxisType : int
{
    X = 0,
    Y = 1,
};

class Axis : public AxisBase
{
public:
    StatusCode onAddedDirty(CoreContext* context) override;
    void offsetChanged() override;
};
} // namespace rive

#endif