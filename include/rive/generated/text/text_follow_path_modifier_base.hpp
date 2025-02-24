#ifndef _RIVE_TEXT_FOLLOW_PATH_MODIFIER_BASE_HPP_
#define _RIVE_TEXT_FOLLOW_PATH_MODIFIER_BASE_HPP_
#include "rive/text/text_target_modifier.hpp"
namespace rive
{
class TextFollowPathModifierBase : public TextTargetModifier
{
protected:
    typedef TextTargetModifier Super;

public:
    static const uint16_t typeKey = 547;

    /// Helper to quickly determine if a core object extends another without
    /// RTTI at runtime.
    bool isTypeOf(uint16_t typeKey) const override
    {
        switch (typeKey)
        {
            case TextFollowPathModifierBase::typeKey:
            case TextTargetModifierBase::typeKey:
            case TextModifierBase::typeKey:
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