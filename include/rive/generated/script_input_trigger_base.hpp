#ifndef _RIVE_SCRIPT_INPUT_TRIGGER_BASE_HPP_
#define _RIVE_SCRIPT_INPUT_TRIGGER_BASE_HPP_
#include "rive/custom_property_trigger.hpp"
namespace rive
{
class ScriptInputTriggerBase : public CustomPropertyTrigger
{
protected:
    typedef CustomPropertyTrigger Super;

public:
    static const uint16_t typeKey = 618;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ScriptInputTriggerBase::typeKey:
            case CustomPropertyTriggerBase::typeKey:
            case CustomPropertyBase::typeKey:
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