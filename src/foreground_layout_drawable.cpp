
#include "rive/foreground_layout_drawable.hpp"
#include "rive/layout_component.hpp"
#include "rive/shapes/paint/fill.hpp"
#include "rive/shapes/paint/stroke.hpp"

using namespace rive;

void ForegroundLayoutDrawable::buildDependencies()
{
    Super::buildDependencies();
    // Set the blend mode on all the shape paints. If we ever animate this
    // property, we'll need to update it in the update cycle/mark dirty when the
    // blend mode changes.
    auto parentLayout = (parent()->as<LayoutComponent>());
    if (parentLayout != nullptr)
    {
        for (auto paint : m_ShapePaints)
        {
            paint->blendMode(parentLayout->blendMode());
        }
    }
}

void ForegroundLayoutDrawable::update(ComponentDirt value)
{
    Super::update(value);
    auto parentLayout = (parent()->as<LayoutComponent>());
    if (parentLayout != nullptr)
    {
        if (hasDirt(value, ComponentDirt::RenderOpacity))
        {
            propagateOpacity(parentLayout->childOpacity());
        }
    }
}

void ForegroundLayoutDrawable::draw(Renderer* renderer)
{
    auto parentLayoutComponent = (parent()->as<LayoutComponent>());

    for (auto shapePaint : m_ShapePaints)
    {
        if (!shapePaint->isVisible())
        {
            continue;
        }
        auto shapePaintPath = shapePaint->pickPath(parentLayoutComponent);
        if (shapePaintPath == nullptr)
        {
            continue;
        }
        shapePaint->draw(renderer,
                         shapePaintPath,
                         parentLayoutComponent->worldTransform());
    }
}

Core* ForegroundLayoutDrawable::hitTest(HitInfo* hinfo, const Mat2D& xform)
{
    return nullptr;
}

Component* ForegroundLayoutDrawable::pathBuilder() { return parent(); }

ShapePaintPath* ForegroundLayoutDrawable::worldPath()
{
    return parent()->as<LayoutComponent>()->worldPath();
}

ShapePaintPath* ForegroundLayoutDrawable::localPath()
{
    return parent()->as<LayoutComponent>()->localPath();
}

ShapePaintPath* ForegroundLayoutDrawable::localClockwisePath()
{
    return parent()->as<LayoutComponent>()->localClockwisePath();
}