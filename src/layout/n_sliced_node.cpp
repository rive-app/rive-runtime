#include "rive/layout_component.hpp"
#include "rive/layout/n_sliced_node.hpp"
#include "rive/math/n_slicer_helpers.hpp"

using namespace rive;

void NSlicedNode::markPathDirtyRecursive(bool sendToLayout)
{
    // Tell all Shape descendants to re-render when the n slicer changed (e.g.
    // when axis offset animates).
    addDirt(ComponentDirt::Path, true);

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
        }
    }
#endif
}

void NSlicedNode::axisChanged() { markPathDirtyRecursive(); }
void NSlicedNode::widthChanged() { markPathDirtyRecursive(); }
void NSlicedNode::heightChanged() { markPathDirtyRecursive(); }

void NSlicedNode::deformWorldRenderPath(RawPath& path) const
{
    NSlicerHelpers::deformWorldRenderPathWithNSlicer(*this, path);
}

void NSlicedNode::deformLocalRenderPath(RawPath& path,
                                        const Mat2D& world,
                                        const Mat2D& inverseWorld) const
{
    NSlicerHelpers::deformLocalRenderPathWithNSlicer(*this,
                                                     path,
                                                     world,
                                                     inverseWorld);
}

Vec2D NSlicedNode::scaleForNSlicer() const
{
    return Vec2D(width() / initialWidth(), height() / initialHeight());
}

Mat2D NSlicedNode::boundsTransform() const
{
    Mat2D transform = Mat2D();
    Vec2D scale = scaleForNSlicer();
    transform.scaleByValues(scale.x, scale.y);
    return transform;
}

// We are telling a Layout what our size is
Vec2D NSlicedNode::measureLayout(float width,
                                 LayoutMeasureMode widthMode,
                                 float height,
                                 LayoutMeasureMode heightMode)
{
    return Vec2D(std::min((widthMode == LayoutMeasureMode::undefined
                               ? std::numeric_limits<float>::max()
                               : width),
                          NSlicedNode::width()),
                 std::min((heightMode == LayoutMeasureMode::undefined
                               ? std::numeric_limits<float>::max()
                               : height),
                          NSlicedNode::height()));
}

// We are told by a Layout to be a particular size
void NSlicedNode::controlSize(Vec2D size)
{
    width(size.x);
    height(size.y);
    markWorldTransformDirty();
    markPathDirtyRecursive(false);
}
