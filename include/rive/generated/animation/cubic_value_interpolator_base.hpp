#ifndef _RIVE_CUBIC_VALUE_INTERPOLATOR_BASE_HPP_
#define _RIVE_CUBIC_VALUE_INTERPOLATOR_BASE_HPP_
#include "rive/animation/cubic_interpolator.hpp"
namespace rive
{
class CubicValueInterpolatorBase : public CubicInterpolator
{
protected:
    typedef CubicInterpolator Super;

public:
    static const uint16_t typeKey = 138;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case CubicValueInterpolatorBase::typeKey:
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