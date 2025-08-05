#include "rive/component.hpp"
#include "rive/resetting_component.hpp"
#include "rive/artboard.hpp"
#include "rive/artboard_component_list.hpp"
#include "rive/nested_artboard.hpp"
#include "rive/nested_artboard_layout.hpp"
#include "rive/nested_artboard_leaf.hpp"

using namespace rive;

ResettingComponent* ResettingComponent::from(Component* component)
{
    switch (component->coreType())
    {
        case NestedArtboardLeaf::typeKey:
        case NestedArtboardLayout::typeKey:
        case NestedArtboard::typeKey:
            return component->as<NestedArtboard>();
        case ArtboardComponentListBase::typeKey:
            return component->as<ArtboardComponentList>();
    }
    return nullptr;
}