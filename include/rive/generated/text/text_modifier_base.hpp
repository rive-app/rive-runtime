#ifndef _RIVE_TEXT_MODIFIER_BASE_HPP_
#define _RIVE_TEXT_MODIFIER_BASE_HPP_
#include "rive/component.hpp"
namespace rive
{
class TextModifierBase : public Component
{
protected:
    typedef Component Super;

public:
    static const uint16_t typeKey = 160;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case TextModifierBase::typeKey:
            case ComponentBase::typeKey:
                return true;
            default:
                return false;
        }
    }

    uint16_t coreType() const override { return typeKey; }

protected:
};
} // namespace rive

#endif