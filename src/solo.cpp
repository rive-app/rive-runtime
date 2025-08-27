#include "rive/solo.hpp"
#include "rive/constraints/constraint.hpp"
#include "rive/shapes/clipping_shape.hpp"
#include "rive/artboard.hpp"

using namespace rive;

void Solo::propagateCollapse(bool collapse)
{
    Core* active =
        collapse ? nullptr : artboard()->resolve(activeComponentId());
    for (Component* child : children())
    {
        // Some child components shouldn't be considered as part of the solo set
        // as they are more aking to properties of the solo itself. For those
        // components, simply pass on the collapse value of the solo itself.
        if (child->is<Constraint>() || child->is<ClippingShape>())
        {
            child->collapse(collapse);
            continue;
        }

        // This child is part of the solo set so only make it active if it's the
        // currently marked solo object.
        child->collapse(child != active);
    }
}

bool Solo::collapse(bool value)
{
    // Intentionally using Component instead of Super as we don't want to call
    // collapse on the Container logic which just propagates blindly to
    // children.
    if (!Component::collapse(value))
    {
        return false;
    }
    propagateCollapse(value);
    return true;
}

void Solo::activeComponentIdChanged() { propagateCollapse(isCollapsed()); }

StatusCode Solo::onAddedClean(CoreContext* context)
{
    StatusCode code = Super::onAddedClean(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }

    propagateCollapse(isCollapsed());
    return StatusCode::Ok;
}

void Solo::updateByIndex(size_t index)
{
    if (index >= 0 && index < children().size() && artboard())
    {
        auto child = children()[index];
        int globalIndex = artboard()->idOf(child);
        activeComponentId(globalIndex);
    }
}

void Solo::updateByName(std::string& name)
{
    for (auto& child : children())
    {
        if (child->name() == name)
        {

            int globalIndex = artboard()->idOf(child);
            activeComponentId(globalIndex);
            break;
        }
    }
}

int Solo::getActiveChildIndex()
{
    Core* active = artboard()->resolve(activeComponentId());
    if (active)
    {
        int index = 0;
        for (auto& child : children())
        {
            if (child == active)
            {
                return index;
            }
            index++;
        }
    }
    return -1;
}

std::string Solo::getActiveChildName()
{
    Core* active = artboard()->resolve(activeComponentId());
    if (active && active->is<Component>())
    {
        return active->as<Component>()->name();
    }
    return "";
}
