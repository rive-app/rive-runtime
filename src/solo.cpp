#include "rive/solo.hpp"
#include "rive/constraints/constraint.hpp"
#include "rive/artboard.hpp"

using namespace rive;

void Solo::propagateCollapse(bool collapse)
{
    Core* active = collapse ? nullptr : artboard()->resolve(activeComponentId());
    for (Component* child : children())
    {
        // We don't want to collapse constraints applied to the Solo
        if (child->is<Constraint>())
        {
            continue;
        }
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