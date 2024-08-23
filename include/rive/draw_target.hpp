#ifndef _RIVE_DRAW_TARGET_HPP_
#define _RIVE_DRAW_TARGET_HPP_

#include "rive/draw_target_placement.hpp"
#include "rive/generated/draw_target_base.hpp"
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

    // Controlled by the artboard.
    Drawable* first = nullptr;
    Drawable* last = nullptr;

public:
    Drawable* drawable() const { return m_Drawable; }
    StatusCode onAddedDirty(CoreContext* context) override;
    StatusCode onAddedClean(CoreContext* context) override;

    DrawTargetPlacement placement() const { return (DrawTargetPlacement)placementValue(); }

protected:
    void placementValueChanged() override;
};
} // namespace rive

#endif