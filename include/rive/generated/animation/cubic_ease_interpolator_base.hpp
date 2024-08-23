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

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case CubicEaseInterpolatorBase::typeKey:
            case CubicInterpolatorBase::typeKey:
            case KeyFrameInterpolatorBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

    Core* clone() const override;

protected:
};
} // namespace rive

#endif