#include "rive/component.hpp"
#include "rive/constraints/scrolling/scroll_constraint.hpp"
#include "rive/artboard.hpp"
#include "rive/layout_component.hpp"
#include "rive/nested_artboard.hpp"
#include "rive/nested_artboard_layout.hpp"
#include "rive/nested_artboard_leaf.hpp"

using namespace rive;

AdvancingComponent* AdvancingComponent::from(Component* component)
{
    switch (component->coreType())
    {
        case NestedArtboardLeaf::typeKey:
        case NestedArtboardLayout::typeKey:
        case NestedArtboard::typeKey:
            return component->as<NestedArtboard>();
        case LayoutComponent::typeKey:
            return component->as<LayoutComponent>();
        case Artboard::typeKey:
            return component->as<Artboard>();
        case ScrollConstraint::typeKey:
            return component->as<ScrollConstraint>();
    }
    return nullptr;
}