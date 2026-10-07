#ifndef _RIVE_CUBIC_EASE_INTERPOLATOR_BASE_HPP_
#define _RIVE_CUBIC_EASE_INTERPOLATOR_BASE_HPP_
#include "rive/animation/cubic_interpolator.hpp"
namespace rive
{
class CubicEaseInterpolatorBase : public CubicInterpolator
{
protected:
    typedef CubicInterpolator Super;

public:
    static const uint16_t typeKey = 28;

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;
};
} // namespace rive

#endif