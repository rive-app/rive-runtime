#ifndef _RIVE_PARAMETRIC_PATH_HPP_
#define _RIVE_PARAMETRIC_PATH_HPP_
#include "rive/math/aabb.hpp"
#include "rive/generated/shapes/parametric_path_base.hpp"
namespace rive
{
class ParametricPath : public ParametricPathBase
{
public:
    Vec2D measureLayout(float width,
                        LayoutMeasureMode widthMode,
                        float height,
                        LayoutMeasureMode heightMode) override;
    void controlSize(Vec2D size,
                     LayoutScaleType widthScaleType,
                     LayoutScaleType heightScaleType,
                     LayoutDirection direction) override;
    void markPathDirty(bool sendToLayout = true) override;
    // A parametric path occupies its declared box, offset by its origin — the
    // same extent update() lays its vertices out in. Known before any build,
    // so a participant can measure and scale on the first pass.
    bool tryPropertyBounds(AABB& result) const override
    {
        result = AABB::fromLTWH(-originX() * width(),
                                -originY() * height(),
                                width(),
                                height());
        return true;
    }

protected:
    void widthChanged() override;
    void heightChanged() override;
    void originXChanged() override;
    void originYChanged() override;
};
} // namespace rive

#endif