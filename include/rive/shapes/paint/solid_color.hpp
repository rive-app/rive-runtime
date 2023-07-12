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
    void applyTo(RenderPaint* renderPaint, float opacityModifier) const override;

protected:
    void renderOpacityChanged() override;
    void colorValueChanged() override;
    bool internalIsTranslucent() const override;
};
} // namespace rive

#endif
