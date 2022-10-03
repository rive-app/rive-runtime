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

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case RadialGradientBase::typeKey:
            case LinearGradientBase::typeKey:
            case ContainerComponentBase::typeKey:
            case ComponentBase::typeKey:
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