#ifndef _RIVE_TEXT_MODIFIER_HPP_
#define _RIVE_TEXT_MODIFIER_HPP_
#include "rive/generated/text/text_modifier_base.hpp"

namespace rive
{
class TextModifier : public TextModifierBase
{
public:
    StatusCode onAddedDirty(CoreContext* context) override;
#ifdef WITH_RIVE_EDITOR
    void editorParentChanged(ContainerComponent* from,
                             ContainerComponent* to) override;
#endif
};
} // namespace rive

#endif