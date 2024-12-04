#ifndef _RIVE_CLAMPED_SCROLL_PHYSICS_BASE_HPP_
#define _RIVE_CLAMPED_SCROLL_PHYSICS_BASE_HPP_
#include "rive/constraints/scrolling/scroll_physics.hpp"
namespace rive
{
class ClampedScrollPhysicsBase : public ScrollPhysics
{
protected:
    typedef ScrollPhysics Super;

public:
    static const uint16_t typeKey = 524;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ClampedScrollPhysicsBase::typeKey:
            case ScrollPhysicsBase::typeKey:
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