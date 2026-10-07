#ifndef _RIVE_RADIAL_GRADIENT_BASE_HPP_
#define _RIVE_RADIAL_GRADIENT_BASE_HPP_
#include "rive/shapes/paint/linear_gradient.hpp"
namespace rive
{
class RadialGradientBase : public LinearGradient
{
protected:
    typedef LinearGradient Super;

public:
    static const uint16_t typeKey = 17;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif