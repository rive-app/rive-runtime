#include "rive/layout_component.hpp"
#include "rive/layout/n_sliced_node.hpp"
#include "rive/math/math_types.hpp"
#include "rive/math/n_slicer_helpers.hpp"

using namespace rive;

void NSlicedNode::markPathDirtyRecursive(bool sendToLayout)
{
    // Tell all Shape descendants to re-render when the n slicer changes (e.g.
    // when axis offset animates).
    addDirt(ComponentDirt::NSlicer, true);

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

void NSlicedNode::update(ComponentDirt value)
{
    Super::update(value);

    // Update whenever children axes change, or our world transform changes
    // in case any dependent needs local path (we need to first map to the
    // NSlicer space using world transform, then back).
    if (hasDirt(value, ComponentDirt::NSlicer | ComponentDirt::WorldTransform))
    {
        updateMapWorldPoint();
    }
}

void NSlicedNode::updateMapWorldPoint()
{
    const Mat2D& world = worldTransform();
    Mat2D inverseWorld;
    if (!world.invert(&inverseWorld) || initialHeight() <= 0 ||
        initialWidth() <= 0)
    {
        mapWorldPoint = [](Vec2D& v) {};
        return;
    }

    Vec2D size = Vec2D(initialWidth(), initialHeight());
    Vec2D scale = scaleForNSlicer();

    std::vector<float> xPxStops = NSlicerHelpers::pxStops(xs(), size.x);
    std::vector<float> yPxStops = NSlicerHelpers::pxStops(ys(), size.y);

    std::vector<float> xUVStops = NSlicerHelpers::uvStops(xs(), size.x);
    std::vector<float> yUVStops = NSlicerHelpers::uvStops(ys(), size.y);

    ScaleInfo xScaleInfo =
        NSlicerHelpers::analyzeUVStops(xUVStops, size.x, std::abs(scale.x));
    ScaleInfo yScaleInfo =
        NSlicerHelpers::analyzeUVStops(yUVStops, size.y, std::abs(scale.y));

    mapWorldPoint = [this,
                     world,
                     inverseWorld,
                     scale,
                     xPxStops,
                     xScaleInfo,
                     yPxStops,
                     yScaleInfo](Vec2D& worldP) {
        // 1. Map from world space to the effective NSlicer's space
        Vec2D localP = inverseWorld * worldP;

        // 2. N-Slice it in the NSlicer's space
        Vec2D slicedP =
            Vec2D(scale.x == 0 ? 0.0
                               : NSlicerHelpers::mapValue(xPxStops,
                                                          xScaleInfo,
                                                          std::abs(width()),
                                                          localP.x) *
                                     std::copysign(1.0f, scale.x),
                  scale.y == 0 ? 0.0
                               : NSlicerHelpers::mapValue(yPxStops,
                                                          yScaleInfo,
                                                          std::abs(height()),
                                                          localP.y) *
                                     std::copysign(1.0f, scale.y));

        // 3. Back to world space
        worldP = world * slicedP;
    };
}

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

Vec2D NSlicedNode::deformLocalPoint(Vec2D point,
                                    const Mat2D& worldTransform,
                                    const Mat2D& inverseWorld) const
{
    Vec2D worldP = worldTransform * point;
    Vec2D deformedWorldP = deformWorldPoint(worldP);
    return inverseWorld * deformedWorldP;
}

Vec2D NSlicedNode::deformWorldPoint(Vec2D point) const
{
    Vec2D result = point;
    mapWorldPoint(result);
    return result;
}

Vec2D NSlicedNode::scaleForNSlicer() const
{
    return Vec2D(width() / initialWidth(), height() / initialHeight());
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
void NSlicedNode::controlSize(Vec2D size,
                              LayoutScaleType widthScaleType,
                              LayoutScaleType heightScaleType,
                              LayoutDirection direction)
{
    width(size.x);
    height(size.y);
    markWorldTransformDirty();
    markPathDirtyRecursive(false);
}
