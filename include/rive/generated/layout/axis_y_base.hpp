#ifndef _RIVE_AXIS_YBASE_HPP_
#define _RIVE_AXIS_YBASE_HPP_
#include "rive/layout/axis.hpp"
namespace rive
{
class AxisYBase : public Axis
{
protected:
    typedef Axis Super;

public:
    static const uint16_t typeKey = 494;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif