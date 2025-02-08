#ifndef _RIVE_FOREGROUND_LAYOUT_DRAWABLE_HPP_
#define _RIVE_FOREGROUND_LAYOUT_DRAWABLE_HPP_
#include "rive/generated/foreground_layout_drawable_base.hpp"
#include "rive/shapes/shape_paint_container.hpp"
#include <stdio.h>
namespace rive
{
class ForegroundLayoutDrawable : public ForegroundLayoutDrawableBase,
                                 public ShapePaintContainer
{
public:
    void draw(Renderer* renderer) override;
    void update(ComponentDirt value) override;
    void buildDependencies() override;
    Core* hitTest(HitInfo*, const Mat2D&) override;

    Artboard* getArtboard() override { return artboard(); }

    // Implemented for ShapePaintContainer.
    const Mat2D& shapeWorldTransform() const override
    {
        return worldTransform();
    }

    Component* pathBuilder() override;
    ShapePaintPath* worldPath() override;
    ShapePaintPath* localPath() override;
    ShapePaintPath* localClockwisePath() override;
};
} // namespace rive

#endif