#ifndef _RIVE_TARGETED_CONSTRAINT_HPP_
#define _RIVE_TARGETED_CONSTRAINT_HPP_
#include "rive/generated/constraints/targeted_constraint_base.hpp"
#ifdef WITH_RIVE_EDITOR
#include "rive/editor/object_arena.hpp"
#endif
#include <stdio.h>
namespace rive
{
class TransformComponent;
class TargetedConstraint : public TargetedConstraintBase
{
private:
    // Raw target pointer — sole storage in runtime build, fallback in
    // editor build for non-arena Cores. Slice 6 Phase E mirrors the
    // m_Parent dual-storage pattern from Phase B (component.hpp): the
    // generational handle catches the "target was deleted at edit time"
    // case where a raw pointer would dangle.
    TransformComponent* m_Target = nullptr;
#ifdef WITH_RIVE_EDITOR
    CoreHandle m_TargetHandle;
#endif

protected:
    virtual bool requiresTarget() { return true; };

#ifdef WITH_RIVE_EDITOR
    // Body in `editor_native/native/src/editor/constraints/
    // targeted_constraint_editor.cpp` — needs full TransformComponent
    // for the `as<>` upcast.
    TransformComponent* target() const;
    void setTargetForEditor(TransformComponent* t);
#else
    inline TransformComponent* target() const { return m_Target; }
#endif

public:
    void buildDependencies() override;
    bool validate(CoreContext* context) override;
    StatusCode onAddedDirty(CoreContext* context) override;
};
} // namespace rive

#endif
