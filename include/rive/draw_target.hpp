#ifndef _RIVE_DRAW_TARGET_HPP_
#define _RIVE_DRAW_TARGET_HPP_

#include "rive/draw_target_placement.hpp"
#include "rive/generated/draw_target_base.hpp"
#ifdef WITH_RIVE_EDITOR
#include "rive/editor/object_arena.hpp"
#endif
#include <stdio.h>

namespace rive
{
class Drawable;
class Artboard;
class DrawTarget : public DrawTargetBase
{
    friend class Artboard;

private:
    Drawable* m_Drawable = nullptr;
#ifdef WITH_RIVE_EDITOR
    // Slice 6 Phase E dual-storage. See targeted_constraint.hpp.
    CoreHandle m_DrawableHandle;
#endif

    // Controlled by the artboard.
    Drawable* first = nullptr;
    Drawable* last = nullptr;

public:
#ifdef WITH_RIVE_EDITOR
    // Body in editor_native/native/src/editor/draw_target_editor.cpp.
    Drawable* drawable() const;
    void setDrawableForEditor(Drawable* d);
#else
    inline Drawable* drawable() const { return m_Drawable; }
#endif
    StatusCode onAddedDirty(CoreContext* context) override;
    StatusCode onAddedClean(CoreContext* context) override;

    DrawTargetPlacement placement() const
    {
        return (DrawTargetPlacement)placementValue();
    }

protected:
    void placementValueChanged() override;
};
} // namespace rive

#endif
