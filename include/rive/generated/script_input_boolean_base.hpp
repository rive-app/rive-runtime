#ifndef _RIVE_SCRIPT_INPUT_BOOLEAN_BASE_HPP_
#define _RIVE_SCRIPT_INPUT_BOOLEAN_BASE_HPP_
#include "rive/custom_property_boolean.hpp"
namespace rive
{
class ScriptInputBooleanBase : public CustomPropertyBoolean
{
protected:
    typedef CustomPropertyBoolean Super;

public:
    static const uint16_t typeKey = 631;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case ScriptInputBooleanBase::typeKey:
            case CustomPropertyBooleanBase::typeKey:
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