#ifndef _RIVE_DRAW_RULES_HPP_
#define _RIVE_DRAW_RULES_HPP_
#include "rive/generated/draw_rules_base.hpp"
#ifdef WITH_RIVE_EDITOR
#include "rive/editor/object_arena.hpp"
#endif
#include <stdio.h>
namespace rive
{
class DrawTarget;
class DrawRules : public DrawRulesBase
{
private:
    DrawTarget* m_ActiveTarget = nullptr;
#ifdef WITH_RIVE_EDITOR
    // Slice 6 Phase E dual-storage. See targeted_constraint.hpp.
    CoreHandle m_ActiveTargetHandle;
#endif

public:
#ifdef WITH_RIVE_EDITOR
    // Body in editor_native/native/src/editor/draw_rules_editor.cpp.
    DrawTarget* activeTarget() const;
    void setActiveTargetForEditor(DrawTarget* t);
#else
    inline DrawTarget* activeTarget() const { return m_ActiveTarget; }
#endif

    StatusCode onAddedDirty(CoreContext* context) override;
    StatusCode onAddedClean(CoreContext* context) override;

protected:
    void drawTargetIdChanged() override;
};
} // namespace rive

#endif
