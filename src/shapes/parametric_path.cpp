#include "rive/math/aabb.hpp"
#include "rive/shapes/parametric_path.hpp"

using namespace rive;

Vec2D ParametricPath::measureLayout(float width,
                                    LayoutMeasureMode widthMode,
                                    float height,
                                    LayoutMeasureMode heightMode)
{
    float measuredWidth, measuredHeight;
    switch (widthMode)
    {
        case LayoutMeasureMode::atMost:
            measuredWidth = std::max(ParametricPath::width(), width);
            break;
        case LayoutMeasureMode::exactly:
            measuredWidth = width;
            break;
        case LayoutMeasureMode::undefined:
            measuredWidth = ParametricPath::width();
            break;
    }
    switch (heightMode)
    {
        case LayoutMeasureMode::atMost:
            measuredHeight = std::max(ParametricPath::height(), height);
            break;
        case LayoutMeasureMode::exactly:
            measuredHeight = height;
            break;
        case LayoutMeasureMode::undefined:
            measuredHeight = ParametricPath::height();
            break;
    }
    return Vec2D(measuredWidth, measuredHeight);
}

void ParametricPath::controlSize(Vec2D size)
{
    width(size.x);
    height(size.y);
    markWorldTransformDirty();
}

void ParametricPath::widthChanged() { markPathDirty(); }
void ParametricPath::heightChanged() { markPathDirty(); }
void ParametricPath::originXChanged() { markPathDirty(); }
void ParametricPath::originYChanged() { markPathDirty(); }