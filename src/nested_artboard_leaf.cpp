#include "rive/nested_artboard_leaf.hpp"
#include "rive/renderer.hpp"
#include "rive/layout_component.hpp"
#include "rive/artboard.hpp"

using namespace rive;

Core* NestedArtboardLeaf::clone() const
{
    NestedArtboardLeaf* nestedArtboard =
        static_cast<NestedArtboardLeaf*>(NestedArtboardLeafBase::clone());
    nestedArtboard->file(file());
    if (m_referencedArtboard == nullptr)
    {
        return nestedArtboard;
    }
    auto ni = m_referencedArtboard->instance();
    nestedArtboard->referencedArtboard(ni.release());
    return nestedArtboard;
}

void NestedArtboardLeaf::update(ComponentDirt value)
{
    Super::update(value);
    auto artboard = artboardInstance();
    if (hasDirt(value, ComponentDirt::WorldTransform) && artboard != nullptr)
    {
        // The layout that sizes us, which may sit above a Solo. A leaf owns no
        // layout node, so it uses the content-sizing reach.
        auto* sizingLayout = contentSizingLayout(parent());

        AABB bounds = sizingLayout != nullptr ? sizingLayout->localBounds()
                                              : artboard->bounds();

        auto artboardFit = (Fit)fit();
        if (artboardFit == Fit::layout)
        {
            // Layout fit owns the artboard's size: match the leaf's bounds
            // and let the artboard's own layout reflow.
            bool resized = false;
            if (artboard->width() != bounds.width())
            {
                artboard->width(bounds.width());
                resized = true;
            }
            if (artboard->height() != bounds.height())
            {
                artboard->height(bounds.height());
                resized = true;
            }
            if (resized)
            {
                // The instance already advanced; reflow now or this frame
                // draws the old layout.
                artboard->updatePass(false);
            }
        }

        auto viewTransform =
            computeAlignment(artboardFit,
                             Alignment(alignmentX(), alignmentY()),
                             bounds,
                             artboard->bounds());

        m_WorldTransform *= viewTransform;
    }
}
