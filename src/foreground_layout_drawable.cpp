
#include "rive/foreground_layout_drawable.hpp"
#include "rive/layout_component.hpp"
#include "rive/shapes/paint/fill.hpp"
#include "rive/shapes/paint/stroke.hpp"

using namespace rive;

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