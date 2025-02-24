#ifndef _RIVE_TEXT_TARGET_MODIFIER_HPP_
#define _RIVE_TEXT_TARGET_MODIFIER_HPP_
#include "rive/generated/text/text_target_modifier_base.hpp"
#include <stdio.h>
namespace rive
{
class Text;
class TransformComponent;
class TextTargetModifier : public TextTargetModifierBase
{
protected:
    TransformComponent* m_Target = nullptr;

public:
    StatusCode onAddedDirty(CoreContext* context) override;
    Text* textComponent() const;
};
} // namespace rive

#endif