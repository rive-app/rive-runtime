#ifndef _RIVE_TEXT_SHAPE_MODIFIER_BASE_HPP_
#define _RIVE_TEXT_SHAPE_MODIFIER_BASE_HPP_
#include "rive/text/text_modifier.hpp"
namespace rive
{
class TextShapeModifierBase : public TextModifier
{
protected:
    typedef TextModifier Super;

public:
    static const uint16_t typeKey = 161;

    /// Helper to quickly determine if a core object extends another without RTTI
    /// at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case TextShapeModifierBase::typeKey:
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