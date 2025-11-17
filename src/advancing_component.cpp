#include "rive/component.hpp"
#include "rive/constraints/scrolling/scroll_constraint.hpp"
#include "rive/artboard.hpp"
#include "rive/artboard_component_list.hpp"
#include "rive/layout_component.hpp"
#include "rive/nested_artboard.hpp"
#include "rive/nested_artboard_layout.hpp"
#include "rive/nested_artboard_leaf.hpp"
#include "rive/scripted/scripted_data_converter.hpp"
#include "rive/scripted/scripted_drawable.hpp"
#include "rive/scripted/scripted_layout.hpp"

using namespace rive;

AdvancingComponent* AdvancingComponent::from(Core* component)
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
        case ArtboardComponentListBase::typeKey:
            return component->as<ArtboardComponentList>();
        case ScrollConstraint::typeKey:
            return component->as<ScrollConstraint>();
        case ScriptedDataConverter::typeKey:
            return component->as<ScriptedDataConverter>();
        case ScriptedDrawable::typeKey:
            return component->as<ScriptedDrawable>();
        case ScriptedLayout::typeKey:
            return component->as<ScriptedLayout>();
    }
    return nullptr;
}