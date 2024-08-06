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
    void controlSize(Vec2D size) override;
    void markPathDirty(bool sendToLayout = true) override;

protected:
    void widthChanged() override;
    void heightChanged() override;
    void originXChanged() override;
    void originYChanged() override;
};
} // namespace rive

#endif