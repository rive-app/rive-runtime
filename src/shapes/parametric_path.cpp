#include "rive/layout_component.hpp"
#include "rive/math/aabb.hpp"
#include "rive/node.hpp"
#include "rive/shapes/parametric_path.hpp"
#include "rive/shapes/shape.hpp"

using namespace rive;

Vec2D ParametricPath::measureLayout(float width,
                                    LayoutMeasureMode widthMode,
                                    float height,
                                    LayoutMeasureMode heightMode)
{
    return Vec2D(
        std::min(
            (widthMode == LayoutMeasureMode::undefined ? std::numeric_limits<float>::max() : width),
            ParametricPath::width()),
        std::min((heightMode == LayoutMeasureMode::undefined ? std::numeric_limits<float>::max()
                                                             : height),
                 ParametricPath::height()));
}

void ParametricPath::controlSize(Vec2D size)
{
    width(size.x);
    height(size.y);
    markWorldTransformDirty();
    markPathDirty(false);
}

void ParametricPath::markPathDirty(bool sendToLayout)
{
    Super::markPathDirty();
#ifdef WITH_RIVE_LAYOUT
    if (sendToLayout)
    {
        for (ContainerComponent* p = parent(); p != nullptr; p = p->parent())
        {
            if (p->is<LayoutComponent>())
            {
                p->as<LayoutComponent>()->markLayoutNodeDirty();
                break;
            }
            // If we're in a group we break out because objects in groups do
            // not affect nor are affected by parent LayoutComponents
            if (p->is<Node>())
            {
                if (p->is<Shape>() && p->as<Shape>() == shape())
                {
                    continue;
                }
                break;
            }
        }
    }
#endif
}

void ParametricPath::widthChanged() { markPathDirty(); }
void ParametricPath::heightChanged() { markPathDirty(); }
void ParametricPath::originXChanged() { markPathDirty(); }
void ParametricPath::originYChanged() { markPathDirty(); }