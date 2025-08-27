#include "rive/artboard.hpp"
#include "rive/artboard_component_list.hpp"
#include "rive/component.hpp"
#include "rive/virtualizing_component.hpp"

using namespace rive;

VirtualizingComponent* VirtualizingComponent::from(Component* component)
{
    switch (component->coreType())
    {
        case ArtboardComponentList::typeKey:
            return component->as<ArtboardComponentList>();
    }
    return nullptr;
}