#ifndef _RIVE_STATE_MACHINE_BASE_HPP_
#define _RIVE_STATE_MACHINE_BASE_HPP_
#include "rive/animation/animation.hpp"
namespace rive
{
class StateMachineBase : public Animation
{
protected:
    typedef Animation Super;

public:
    static const uint16_t typeKey = 53;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case StateMachineBase::typeKey:
            case AnimationBase::typeKey:
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