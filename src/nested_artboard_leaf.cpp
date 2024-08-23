#include "rive/nested_artboard_leaf.hpp"
#include "rive/renderer.hpp"
#include "rive/layout_component.hpp"
#include "rive/artboard.hpp"

using namespace rive;

Core* NestedArtboardLeaf::clone() const
{
    NestedArtboardLeaf* nestedArtboard =
        static_cast<NestedArtboardLeaf*>(NestedArtboardLeafBase::clone());
    if (m_Artboard == nullptr)
    {
        return nestedArtboard;
    }
    auto ni = m_Artboard->instance();
    nestedArtboard->nest(ni.release());
    return nestedArtboard;
}

void NestedArtboardLeaf::update(ComponentDirt value)
{
    Super::update(value);
    auto artboard = artboardInstance();
    if (hasDirt(value, ComponentDirt::WorldTransform) && artboard != nullptr)
    {
        auto p = parent();

        AABB bounds = p != nullptr && p->is<LayoutComponent>()
                          ? p->as<LayoutComponent>()->localBounds()
                          : AABB();

        auto viewTransform = computeAlignment((Fit)fit(),
                                              Alignment(alignmentX(), alignmentY()),
                                              bounds,
                                              artboard->bounds());

        m_WorldTransform *= viewTransform;
    }
}
