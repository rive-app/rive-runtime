
#include "rive/foreground_layout_drawable.hpp"
#include "rive/layout_component.hpp"
#include "rive/shapes/paint/fill.hpp"
#include "rive/shapes/paint/stroke.hpp"

using namespace rive;

void ForegroundLayoutDrawable::draw(Renderer* renderer)
{
    auto parentLayoutComponent = (parent()->as<LayoutComponent>());
    auto backgroundPath = parentLayoutComponent->backgroundPath();
    auto backgroundRect = parentLayoutComponent->backgroundRect();
    renderer->save();
    renderer->transform(parentLayoutComponent->worldTransform());
    for (auto shapePaint : m_ShapePaints)
    {
        if (!shapePaint->isVisible())
        {
            continue;
        }
        if (shapePaint->is<Stroke>())
        {
            shapePaint->draw(renderer,
                             backgroundPath,
                             &backgroundRect->rawPath());
        }
        if (shapePaint->is<Fill>())
        {
            shapePaint->draw(renderer,
                             backgroundPath,
                             &backgroundRect->rawPath());
        }
    }
    renderer->restore();
}

Core* ForegroundLayoutDrawable::hitTest(HitInfo* hinfo, const Mat2D& xform)
{
    return nullptr;
}