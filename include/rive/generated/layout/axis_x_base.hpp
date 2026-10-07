#ifndef _RIVE_AXIS_XBASE_HPP_
#define _RIVE_AXIS_XBASE_HPP_
#include "rive/layout/axis.hpp"
namespace rive
{
class AxisXBase : public Axis
{
protected:
    typedef Axis Super;

public:
    static const uint16_t typeKey = 495;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif