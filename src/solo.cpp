#include "rive/solo.hpp"
#include "rive/constraints/constraint.hpp"
#include "rive/shapes/clipping_shape.hpp"
#include "rive/focus_data.hpp"
#include "rive/semantic/semantic_data.hpp"
#include "rive/artboard.hpp"

using namespace rive;

// Some child components shouldn't be considered as part of the solo set as they
// are more akin to properties/metadata of the solo itself (constraints,
// clipping shapes, focus and semantic data) rather than selectable solo
// options. These are excluded both from collapse propagation and from
// index/name based selection so that data binding targets only the real solo
// options.
static bool isSoloSetMember(Component* child)
{
    return !(child->is<Constraint>() || child->is<ClippingShape>() ||
             child->is<FocusData>() || child->is<SemanticData>());
}

void Solo::propagateCollapse(bool collapse)
{
    Core* active =
        collapse ? nullptr : artboard()->resolve(activeComponentId());
    for (Component* child : children())
    {
        // For components that aren't part of the solo set, simply pass on the
        // collapse value of the solo itself.
        if (!isSoloSetMember(child))
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
    // The number of solo options is always <= children().size(), so any index
    // that big can never match. Bail early to avoid an O(n) walk every frame
    // for out-of-range indices (the data-binding path casts rounded floats to
    // size_t without clamping, so negative values arrive as huge indices).
    if (!artboard() || index >= children().size())
    {
        return;
    }
    // The index refers to the Nth solo option, skipping property-like children
    // (constraints, clipping shapes, focus/semantic data) so it matches the
    // ordering exposed by getActiveChildIndex.
    size_t soloIndex = 0;
    for (auto& child : children())
    {
        if (!isSoloSetMember(child))
        {
            continue;
        }
        if (soloIndex == index)
        {
            activeComponentId(artboard()->idOf(child));
            return;
        }
        soloIndex++;
    }
}

void Solo::updateByName(const std::string& name)
{
    if (!artboard())
    {
        return;
    }
    for (auto& child : children())
    {
        if (!isSoloSetMember(child))
        {
            continue;
        }
        if (child->name() == name)
        {
            activeComponentId(artboard()->idOf(child));
            break;
        }
    }
}

int Solo::getActiveChildIndex()
{
    if (!artboard())
    {
        return -1;
    }
    Core* active = artboard()->resolve(activeComponentId());
    if (active)
    {
        int index = 0;
        for (auto& child : children())
        {
            if (!isSoloSetMember(child))
            {
                continue;
            }
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
    if (!artboard())
    {
        return "";
    }
    Core* active = artboard()->resolve(activeComponentId());
    if (active && active->is<Component>())
    {
        return active->as<Component>()->name();
    }
    return "";
}
