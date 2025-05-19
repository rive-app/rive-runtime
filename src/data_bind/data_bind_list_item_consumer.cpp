#include "rive/artboard_component_list.hpp"
#include "rive/component.hpp"
#include "rive/data_bind/data_bind_list_item_consumer.hpp"
#include "rive/generated/core_registry.hpp"

using namespace rive;

DataBindListItemConsumer* DataBindListItemConsumer::from(Core* component)
{
    switch (component->coreType())
    {
        case ArtboardComponentList::typeKey:
            return component->as<ArtboardComponentList>();
    }
    return nullptr;
}