#ifndef _RIVE_NESTED_TRIGGER_BASE_HPP_
#define _RIVE_NESTED_TRIGGER_BASE_HPP_
#include "rive/animation/nested_input.hpp"
namespace rive
{
class NestedTriggerBase : public NestedInput
{
protected:
    typedef NestedInput Super;

public:
    static const uint16_t typeKey = 122;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case NestedTriggerBase::typeKey:
            case NestedInputBase::typeKey:
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