#ifndef _RIVE_SOLID_COLOR_HPP_
#define _RIVE_SOLID_COLOR_HPP_
#include "rive/generated/shapes/paint/solid_color_base.hpp"
#include "rive/shapes/paint/shape_paint_mutator.hpp"
namespace rive
{
class SolidColor : public SolidColorBase, public ShapePaintMutator
{
public:
    StatusCode onAddedDirty(CoreContext* context) override;
    void applyTo(RenderPaint* renderPaint, float opacityModifier) override;
#ifdef WITH_RIVE_EDITOR
    // Editor: when our parent ShapePaint resolves (initial coop
    // hydration with late parent, or user re-parenting), retry
    // `initPaintMutator` so the render paint hooks up. Body in
    // `component_parent_editor.cpp`. See `Component::
    // editorParentChanged` for the lifecycle contract.
    void editorParentChanged(ContainerComponent* from,
                             ContainerComponent* to) override;
#endif

protected:
    void renderOpacityChanged() override;
    void colorValueChanged() override;
};
} // namespace rive

#endif
