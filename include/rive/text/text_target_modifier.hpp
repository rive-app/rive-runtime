#ifndef _RIVE_TEXT_TARGET_MODIFIER_HPP_
#define _RIVE_TEXT_TARGET_MODIFIER_HPP_
#include "rive/generated/text/text_target_modifier_base.hpp"
#ifdef WITH_RIVE_EDITOR
#include "rive/editor/object_arena.hpp"
#endif
#include <stdio.h>
namespace rive
{
class Text;
class TransformComponent;
class TextTargetModifier : public TextTargetModifierBase
{
private:
    // Slice 6 Phase E dual-storage — see targeted_constraint.hpp for
    // the rationale. TextTargetModifier carries a separate target
    // pointer (sibling hierarchy to TargetedConstraint) but the
    // edit-time hazard is the same: if the target is deleted while
    // the modifier still holds a raw pointer, update() crashes. The
    // generational handle resolves to nullptr in that case.
    TransformComponent* m_Target = nullptr;
#ifdef WITH_RIVE_EDITOR
    CoreHandle m_TargetHandle;
#endif

protected:
#ifdef WITH_RIVE_EDITOR
    // Body in `editor_native/native/src/editor/text/
    // text_target_modifier_editor.cpp`.
    TransformComponent* target() const;
    void setTargetForEditor(TransformComponent* t);
#else
    inline TransformComponent* target() const { return m_Target; }
#endif

public:
    StatusCode onAddedDirty(CoreContext* context) override;
    Text* textComponent() const;
};
} // namespace rive

#endif
